// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LIBGTKUI_INPUT_METHOD_CONTEXT_IMPL_GTK_H_
#define CHROME_BROWSER_UI_LIBGTKUI_INPUT_METHOD_CONTEXT_IMPL_GTK_H_

#include "base/macros.h"
#include "base/strings/string16.h"
#include "ui/base/glib/glib_integers.h"
#include "ui/base/glib/glib_signal.h"
#include "ui/base/ime/linux/linux_input_method_context.h"
#include "ui/gfx/geometry/rect.h"

typedef struct _GdkWindow GdkWindow;
typedef struct _GtkIMContext GtkIMContext;

namespace libgtkui {

// An implementation of LinuxInputMethodContext which uses GtkIMContext
// (gtk-immodule) as a bridge from/to underlying IMEs.
class InputMethodContextImplGtk : public ui::LinuxInputMethodContext {
 public:
  InputMethodContextImplGtk(ui::LinuxInputMethodContextDelegate* delegate,
                            bool is_simple);
  ~InputMethodContextImplGtk() override;

  // Overridden from ui::LinuxInputMethodContext
  bool DispatchKeyEvent(const ui::KeyEvent& key_event) override;
  void SetCursorLocation(const gfx::Rect& rect) override;
  void Reset() override;
  void Focus() override;
  void Blur() override;
  void SetSurroundingText(const base::string16& text,
                          const gfx::Range& selection_range) override;

 private:
  // GtkIMContext event handlers.  They are shared among |gtk_context_simple_|
  // and |gtk_multicontext_|.
  CHROMEG_CALLBACK_1(InputMethodContextImplGtk,
                     void,
                     OnCommit,
                     GtkIMContext*,
                     gchar*);
  CHROMEG_CALLBACK_0(InputMethodContextImplGtk,
                     void,
                     OnPreeditChanged,
                     GtkIMContext*);
  CHROMEG_CALLBACK_0(InputMethodContextImplGtk,
                     void,
                     OnPreeditEnd,
                     GtkIMContext*);
  CHROMEG_CALLBACK_0(InputMethodContextImplGtk,
                     void,
                     OnPreeditStart,
                     GtkIMContext*);

  void SetContextClientWindow(GdkWindow* window);

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

  DISALLOW_COPY_AND_ASSIGN(InputMethodContextImplGtk);
};

}  // namespace libgtkui

#endif  // CHROME_BROWSER_UI_LIBGTKUI_INPUT_METHOD_CONTEXT_IMPL_GTK_H_
