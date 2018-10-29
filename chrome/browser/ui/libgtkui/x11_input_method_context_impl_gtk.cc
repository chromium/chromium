// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/libgtkui/x11_input_method_context_impl_gtk.h"

#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <gdk/gdkx.h>
#include <stddef.h>

#include <gtk/gtk.h>

#include "base/strings/utf_string_conversions.h"
#include "ui/base/ime/composition_text.h"
#include "ui/base/ime/composition_text_util_pango.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_code_conversion_x.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/x11_types.h"
#include "ui/views/linux_ui/linux_ui.h"

namespace libgtkui {

X11InputMethodContextImplGtk::X11InputMethodContextImplGtk(
    ui::LinuxInputMethodContextDelegate* delegate,
    bool is_simple)
    : delegate_(delegate),
      is_simple_(is_simple),
      has_focus_(false),
      gtk_context_(nullptr),
      gdk_last_set_client_window_(nullptr) {
  CHECK(delegate_);

  ResetXModifierKeycodesCache();

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

X11InputMethodContextImplGtk::~X11InputMethodContextImplGtk() {
  if (gtk_context_) {
    g_object_unref(gtk_context_);
    gtk_context_ = nullptr;
  }
}

// Overriden from ui::LinuxInputMethodContext

bool X11InputMethodContextImplGtk::DispatchKeyEvent(
    const ui::KeyEvent& key_event) {
  if (!key_event.HasNativeEvent() || !gtk_context_)
    return false;

  // Translate a XKeyEvent to a GdkEventKey.
  GdkEvent* event = GdkEventFromNativeEvent(key_event.native_event());
  if (!event) {
    LOG(ERROR) << "Cannot translate a XKeyEvent to a GdkEvent.";
    return false;
  }

  if (event->key.window != gdk_last_set_client_window_) {
#if GTK_CHECK_VERSION(3, 90, 0)
    gtk_im_context_set_client_widget(gtk_context_,
                                     GTK_WIDGET(event->key.window));
#else
    gtk_im_context_set_client_window(gtk_context_, event->key.window);
#endif
    gdk_last_set_client_window_ = event->key.window;
  }

  // Convert the last known caret bounds relative to the screen coordinates
  // to a GdkRectangle relative to the client window.
  gint win_x = 0;
  gint win_y = 0;
  gdk_window_get_origin(event->key.window, &win_x, &win_y);

  gint factor = gdk_window_get_scale_factor(event->key.window);
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

void X11InputMethodContextImplGtk::Reset() {
  gtk_im_context_reset(gtk_context_);

  // Some input methods may not honour the reset call.
  // Focusing out/in the to make sure it gets reset correctly.
  if (!is_simple_ && has_focus_) {
    Blur();
    Focus();
  }
}

void X11InputMethodContextImplGtk::Focus() {
  gtk_im_context_focus_in(gtk_context_);
  has_focus_ = true;
}

void X11InputMethodContextImplGtk::Blur() {
  gtk_im_context_focus_out(gtk_context_);
  has_focus_ = false;
}

void X11InputMethodContextImplGtk::SetCursorLocation(const gfx::Rect& rect) {
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

void X11InputMethodContextImplGtk::SetSurroundingText(
    const base::string16& text,
    const gfx::Range& selection_range) {}

// private:

void X11InputMethodContextImplGtk::ResetXModifierKeycodesCache() {
  modifier_keycodes_.clear();
  meta_keycodes_.clear();
  super_keycodes_.clear();
  hyper_keycodes_.clear();

  Display* display = gfx::GetXDisplay();
  gfx::XScopedPtr<XModifierKeymap,
                  gfx::XObjectDeleter<XModifierKeymap, int, XFreeModifiermap>>
      modmap(XGetModifierMapping(display));
  int min_keycode = 0;
  int max_keycode = 0;
  int keysyms_per_keycode = 1;
  XDisplayKeycodes(display, &min_keycode, &max_keycode);
  gfx::XScopedPtr<KeySym[]> keysyms(
      XGetKeyboardMapping(display, min_keycode, max_keycode - min_keycode + 1,
                          &keysyms_per_keycode));
  for (int i = 0; i < 8 * modmap->max_keypermod; ++i) {
    const int keycode = modmap->modifiermap[i];
    if (!keycode)
      continue;
    modifier_keycodes_.insert(keycode);

    if (!keysyms)
      continue;
    for (int j = 0; j < keysyms_per_keycode; ++j) {
      switch (keysyms[(keycode - min_keycode) * keysyms_per_keycode + j]) {
        case XK_Meta_L:
        case XK_Meta_R:
          meta_keycodes_.push_back(keycode);
          break;
        case XK_Super_L:
        case XK_Super_R:
          super_keycodes_.push_back(keycode);
          break;
        case XK_Hyper_L:
        case XK_Hyper_R:
          hyper_keycodes_.push_back(keycode);
          break;
      }
    }
  }
}

GdkEvent* X11InputMethodContextImplGtk::GdkEventFromNativeEvent(
    const ui::PlatformEvent& native_event) {
  XEvent xkeyevent;
  if (native_event->type == GenericEvent) {
    // If this is an XI2 key event, build a matching core X event, to avoid
    // having two cases for every use.
    ui::InitXKeyEventFromXIDeviceEvent(*native_event, &xkeyevent);
  } else {
    DCHECK(native_event->type == KeyPress || native_event->type == KeyRelease);
    xkeyevent.xkey = native_event->xkey;
  }
  XKeyEvent& xkey = xkeyevent.xkey;

  // Get a GdkDisplay.
  GdkDisplay* display = gdk_x11_lookup_xdisplay(xkey.display);
  if (!display) {
    // Fall back to the default display.
    display = gdk_display_get_default();
  }
  if (!display) {
    LOG(ERROR) << "Cannot get a GdkDisplay for a key event.";
    return nullptr;
  }
  // Get a keysym and group.
  KeySym keysym = NoSymbol;
  guint8 keyboard_group = 0;
  XLookupString(&xkey, nullptr, 0, &keysym, nullptr);
  GdkKeymap* keymap = gdk_keymap_get_for_display(display);
  GdkKeymapKey* keys = nullptr;
  guint* keyvals = nullptr;
  gint n_entries = 0;
  if (keymap && gdk_keymap_get_entries_for_keycode(keymap, xkey.keycode, &keys,
                                                   &keyvals, &n_entries)) {
    for (gint i = 0; i < n_entries; ++i) {
      if (keyvals[i] == keysym) {
        keyboard_group = keys[i].group;
        break;
      }
    }
  }
  g_free(keys);
  keys = nullptr;
  g_free(keyvals);
  keyvals = nullptr;
// Get a GdkWindow.
  GdkWindow* window = gdk_x11_window_lookup_for_display(display, xkey.window);
  if (window)
    g_object_ref(window);
  else
    window = gdk_x11_window_foreign_new_for_display(display, xkey.window);
  if (!window) {
    LOG(ERROR) << "Cannot get a GdkWindow for a key event.";
    return nullptr;
  }

  // Create a GdkEvent.
  GdkEventType event_type =
      xkey.type == KeyPress ? GDK_KEY_PRESS : GDK_KEY_RELEASE;
  GdkEvent* event = gdk_event_new(event_type);
  event->key.type = event_type;
  event->key.window = window;
  // GdkEventKey and XKeyEvent share the same definition for time and state.
  event->key.send_event = xkey.send_event;
  event->key.time = xkey.time;
  event->key.state = xkey.state;
  event->key.keyval = keysym;
  event->key.length = 0;
  event->key.string = nullptr;
  event->key.hardware_keycode = xkey.keycode;
  event->key.group = keyboard_group;
  event->key.is_modifier = IsKeycodeModifierKey(xkey.keycode);

  char keybits[32] = {0};
  XQueryKeymap(xkey.display, keybits);
  if (IsAnyOfKeycodesPressed(meta_keycodes_, keybits, sizeof keybits * 8))
    event->key.state |= GDK_META_MASK;
  if (IsAnyOfKeycodesPressed(super_keycodes_, keybits, sizeof keybits * 8))
    event->key.state |= GDK_SUPER_MASK;
  if (IsAnyOfKeycodesPressed(hyper_keycodes_, keybits, sizeof keybits * 8))
    event->key.state |= GDK_HYPER_MASK;

  return event;
}

bool X11InputMethodContextImplGtk::IsKeycodeModifierKey(
    unsigned int keycode) const {
  return modifier_keycodes_.find(keycode) != modifier_keycodes_.end();
}

bool X11InputMethodContextImplGtk::IsAnyOfKeycodesPressed(
    const std::vector<int>& keycodes,
    const char* keybits,
    int num_keys) const {
  for (size_t i = 0; i < keycodes.size(); ++i) {
    const int keycode = keycodes[i];
    if (keycode < 0 || num_keys <= keycode)
      continue;
    if (keybits[keycode / 8] & 1 << (keycode % 8))
      return true;
  }
  return false;
}

// GtkIMContext event handlers.

void X11InputMethodContextImplGtk::OnCommit(GtkIMContext* context,
                                            gchar* text) {
  if (context != gtk_context_)
    return;

  delegate_->OnCommit(base::UTF8ToUTF16(text));
}

void X11InputMethodContextImplGtk::OnPreeditChanged(GtkIMContext* context) {
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

void X11InputMethodContextImplGtk::OnPreeditEnd(GtkIMContext* context) {
  if (context != gtk_context_)
    return;

  delegate_->OnPreeditEnd();
}

void X11InputMethodContextImplGtk::OnPreeditStart(GtkIMContext* context) {
  if (context != gtk_context_)
    return;

  delegate_->OnPreeditStart();
}

}  // namespace libgtkui
