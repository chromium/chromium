// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LIBGTKUI_X11_INPUT_METHOD_CONTEXT_IMPL_GTK_H_
#define CHROME_BROWSER_UI_LIBGTKUI_X11_INPUT_METHOD_CONTEXT_IMPL_GTK_H_

#include <vector>

#include "base/containers/hash_tables.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "ui/base/glib/glib_integers.h"
#include "ui/base/glib/glib_signal.h"
#include "ui/base/ime/linux/linux_input_method_context.h"
#include "ui/events/platform_event.h"
#include "ui/gfx/geometry/rect.h"

typedef union _GdkEvent GdkEvent;
typedef struct _GtkIMContext GtkIMContext;

namespace libgtkui {

// An implementation of LinuxInputMethodContext which is based on X11 event loop
// and uses GtkIMContext(gtk-immodule) as a bridge from/to underlying IMEs.
class X11InputMethodContextImplGtk : public ui::LinuxInputMethodContext {
 public:
  X11InputMethodContextImplGtk(ui::LinuxInputMethodContextDelegate* delegate,
                               bool is_simple);
  ~X11InputMethodContextImplGtk() override;

  // Overriden from ui::LinuxInputMethodContext
  bool DispatchKeyEvent(const ui::KeyEvent& key_event) override;
  void SetCursorLocation(const gfx::Rect& rect) override;
  void Reset() override;
  void Focus() override;
  void Blur() override;
  void SetSurroundingText(const base::string16& text,
                          const gfx::Range& selection_range) override;

 private:
  // Resets the cache of X modifier keycodes.
  // TODO(yukishiino): We should call this method whenever X keyboard mapping
  // changes, for example when a user switched to another keyboard layout.
  void ResetXModifierKeycodesCache();

  // Constructs a GdkEventKey from a XKeyEvent and returns it.  Otherwise,
  // returns nullptr.  The returned GdkEvent must be freed by gdk_event_free.
  GdkEvent* GdkEventFromNativeEvent(const ui::PlatformEvent& native_event);

  // Returns true if the hardware |keycode| is assigned to a modifier key.
  bool IsKeycodeModifierKey(unsigned int keycode) const;

  // Returns true if one of |keycodes| is pressed.  |keybits| is a bit vector
  // returned by XQueryKeymap, and |num_keys| is the number of keys in
  // |keybits|.
  bool IsAnyOfKeycodesPressed(const std::vector<int>& keycodes,
                              const char* keybits,
                              int num_keys) const;

  // GtkIMContext event handlers.  They are shared among |gtk_context_simple_|
  // and |gtk_multicontext_|.
  CHROMEG_CALLBACK_1(X11InputMethodContextImplGtk,
                     void,
                     OnCommit,
                     GtkIMContext*,
                     gchar*);
  CHROMEG_CALLBACK_0(X11InputMethodContextImplGtk,
                     void,
                     OnPreeditChanged,
                     GtkIMContext*);
  CHROMEG_CALLBACK_0(X11InputMethodContextImplGtk,
                     void,
                     OnPreeditEnd,
                     GtkIMContext*);
  CHROMEG_CALLBACK_0(X11InputMethodContextImplGtk,
                     void,
                     OnPreeditStart,
                     GtkIMContext*);

  // A set of callback functions.  Must not be nullptr.
  ui::LinuxInputMethodContextDelegate* delegate_;

  // Input method context type flag.
  //   - true if it supports table-based input methods
  //   - false if it supports multiple, loadable input methods
  bool is_simple_;

  // Keeps track of current focus state.
  bool has_focus_;

  // IME's input GTK context.
  GtkIMContext* gtk_context_;

  gpointer gdk_last_set_client_window_;

  // Last known caret bounds relative to the screen coordinates.
  gfx::Rect last_caret_bounds_;

  // A set of hardware keycodes of modifier keys.
  base::hash_set<unsigned int> modifier_keycodes_;

  // A list of keycodes of each modifier key.
  std::vector<int> meta_keycodes_;
  std::vector<int> super_keycodes_;
  std::vector<int> hyper_keycodes_;

  DISALLOW_COPY_AND_ASSIGN(X11InputMethodContextImplGtk);
};

}  // namespace libgtkui

#endif  // CHROME_BROWSER_UI_LIBGTKUI_X11_INPUT_METHOD_CONTEXT_IMPL_GTK_H_
