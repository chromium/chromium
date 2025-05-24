// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONSTRAINED_WINDOW_CONSTRAINED_WINDOW_VIEWS_CLIENT_H_
#define COMPONENTS_CONSTRAINED_WINDOW_CONSTRAINED_WINDOW_VIEWS_CLIENT_H_

#include "ui/gfx/native_widget_types.h"

namespace web_modal {
class ModalDialogHost;
}

namespace constrained_window {

class ConstrainedWindowViewsClient {
 public:
  virtual ~ConstrainedWindowViewsClient() = default;

  // Returns the modal window host for the |parent| native window.
  virtual web_modal::ModalDialogHost* GetModalDialogHost(
      gfx::NativeWindow parent) = 0;

  // Returns the native view in |window| appropriate for positioning dialogs.
  virtual gfx::NativeView GetDialogHostView(gfx::NativeWindow window) = 0;
};

}  // namespace constrained window

#endif  // COMPONENTS_CONSTRAINED_WINDOW_CONSTRAINED_WINDOW_VIEWS_CLIENT_H_
