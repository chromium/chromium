// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/libgtkui/input_method_context_impl_gtk.h"

#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <stddef.h>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/libgtkui/gtk_util.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/composition_text.h"
#include "ui/base/ime/linux/composition_text_util_pango.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/views/linux_ui/linux_ui.h"

namespace libgtkui {

namespace {

// Get IME KeyEvent's target window. Assumes root aura::Window is set to
// Event::target(), otherwise returns null.
GdkWindow* GetTargetWindow(const ui::KeyEvent& key_event) {
  if (!key_event.target())
    return nullptr;

  GdkDisplay* display = GetGdkDisplay();
  aura::Window* window = static_cast<aura::Window*>(key_event.target());
  XID xwindow = window->GetHost()->GetAcceleratedWidget();

  GdkWindow* gdk_window = gdk_x11_window_lookup_for_display(display, xwindow);
  if (gdk_window)
    g_object_ref(gdk_window);
  else
    gdk_window = gdk_x11_window_foreign_new_for_display(display, xwindow);
  return gdk_window;
}

// Translate IME ui::KeyEvent to a GdkEventKey.
GdkEvent* GdkEventFromImeKeyEvent(const ui::KeyEvent& key_event) {
  GdkEvent* event = GdkEventFromKeyEvent(key_event);
  if (!event)
    return nullptr;

  GdkWindow* target_window = GetTargetWindow(key_event);
  if (!target_window) {
    gdk_event_free(event);
    return nullptr;
  }
  event->key.window = target_window;
  return event;
}

}  // namespace

InputMethodContextImplGtk::InputMethodContextImplGtk(
    ui::LinuxInputMethodContextDelegate* delegate,
    bool is_simple)
    : delegate_(delegate),
      is_simple_(is_simple),
      has_focus_(false),
      gtk_context_(nullptr),
      gdk_last_set_client_window_(nullptr) {
  CHECK(delegate_);

  gtk_context_ =
      is_simple ? gtk_im_context_simple_new() : gtk_im_multicontext_new();

  g_signal_connect(gtk_context_, "commit", G_CALLBACK(OnCommitThunk), this);
  g_signal_connect(gtk_context_, "preedit-changed",
                   G_CALLBACK(OnPreeditChangedThunk), this);
  g_signal_connect(gtk_context_, "preedit-end", G_CALLBACK(OnPreeditEndThunk),
                   this);
  g_signal_connect(gtk_context_, "preedit-start",
                   G_CALLBACK(OnPreeditStartThunk), this);
  // TODO(shuchen): Handle operations on surrounding text.
  // "delete-surrounding" and "retrieve-surrounding" signals should be
  // handled.
}

InputMethodContextImplGtk::~InputMethodContextImplGtk() {
  if (gtk_context_) {
    g_object_unref(gtk_context_);
    gtk_context_ = nullptr;
  }
}

// Overridden from ui::LinuxInputMethodContext
bool InputMethodContextImplGtk::DispatchKeyEvent(
    const ui::KeyEvent& key_event) {
  if (!gtk_context_)
    return false;

  GdkEvent* event = GdkEventFromImeKeyEvent(key_event);
  if (!event) {
    LOG(ERROR) << "Cannot translate a Keyevent to a GdkEvent.";
    return false;
  }

  GdkWindow* target_window = event->key.window;
  if (!target_window) {
    LOG(ERROR) << "Cannot get target GdkWindow for KeyEvent.";
    return false;
  }

  SetContextClientWindow(target_window);

  // Convert the last known caret bounds relative to the screen coordinates
  // to a GdkRectangle relative to the client window.
  gint win_x = 0;
  gint win_y = 0;
  gdk_window_get_origin(target_window, &win_x, &win_y);

  gint factor = gdk_window_get_scale_factor(target_window);
  gint caret_x = last_caret_bounds_.x() / factor;
  gint caret_y = last_caret_bounds_.y() / factor;
  gint caret_w = last_caret_bounds_.width() / factor;
  gint caret_h = last_caret_bounds_.height() / factor;

  GdkRectangle gdk_rect = {caret_x - win_x, caret_y - win_y, caret_w, caret_h};
  gtk_im_context_set_cursor_location(gtk_context_, &gdk_rect);

  const bool handled =
      gtk_im_context_filter_keypress(gtk_context_, &event->key);
  gdk_event_free(event);
  return handled;
}

void InputMethodContextImplGtk::Reset() {
  gtk_im_context_reset(gtk_context_);

  // Some input methods may not honour the reset call.
  // Focusing out/in the to make sure it gets reset correctly.
  if (!is_simple_ && has_focus_) {
    Blur();
    Focus();
  }
}

void InputMethodContextImplGtk::Focus() {
  gtk_im_context_focus_in(gtk_context_);
  has_focus_ = true;
}

void InputMethodContextImplGtk::Blur() {
  gtk_im_context_focus_out(gtk_context_);
  has_focus_ = false;
}

void InputMethodContextImplGtk::SetCursorLocation(const gfx::Rect& rect) {
  // Remember the caret bounds so that we can set the cursor location later.
  // gtk_im_context_set_cursor_location() takes the location relative to the
  // client window, which is unknown at this point.  So we'll call
  // gtk_im_context_set_cursor_location() later in ProcessKeyEvent() where
  // (and only where) we know the client window.
  if (views::LinuxUI::instance()) {
    last_caret_bounds_ = gfx::ConvertRectToPixel(
        views::LinuxUI::instance()->GetDeviceScaleFactor(), rect);
  } else {
    last_caret_bounds_ = rect;
  }
}

void InputMethodContextImplGtk::SetSurroundingText(
    const base::string16& text,
    const gfx::Range& selection_range) {}

// private:

// GtkIMContext event handlers.

void InputMethodContextImplGtk::OnCommit(GtkIMContext* context, gchar* text) {
  if (context != gtk_context_)
    return;

  delegate_->OnCommit(base::UTF8ToUTF16(text));
}

void InputMethodContextImplGtk::OnPreeditChanged(GtkIMContext* context) {
  if (context != gtk_context_)
    return;

  gchar* str = nullptr;
  PangoAttrList* attrs = nullptr;
  gint cursor_pos = 0;
  gtk_im_context_get_preedit_string(context, &str, &attrs, &cursor_pos);
  ui::CompositionText composition_text;
  ui::ExtractCompositionTextFromGtkPreedit(str, attrs, cursor_pos,
                                           &composition_text);
  g_free(str);
  pango_attr_list_unref(attrs);

  delegate_->OnPreeditChanged(composition_text);
}

void InputMethodContextImplGtk::OnPreeditEnd(GtkIMContext* context) {
  if (context != gtk_context_)
    return;

  delegate_->OnPreeditEnd();
}

void InputMethodContextImplGtk::OnPreeditStart(GtkIMContext* context) {
  if (context != gtk_context_)
    return;

  delegate_->OnPreeditStart();
}

void InputMethodContextImplGtk::SetContextClientWindow(GdkWindow* window) {
  if (window == gdk_last_set_client_window_)
    return;
#if GTK_CHECK_VERSION(3, 90, 0)
  gtk_im_context_set_client_widget(gtk_context_, GTK_WIDGET(window));
#else
  gtk_im_context_set_client_window(gtk_context_, window);
#endif

  // Prevent leaks when overriding last client window
  if (gdk_last_set_client_window_)
    g_object_unref(gdk_last_set_client_window_);
  gdk_last_set_client_window_ = window;
}

}  // namespace libgtkui
