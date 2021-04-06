// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_COMPAT_MODE_RESIZE_CONFIRMATION_DIALOG_H_
#define COMPONENTS_ARC_COMPAT_MODE_RESIZE_CONFIRMATION_DIALOG_H_

#include "base/callback_forward.h"

namespace aura {
class Window;
}  // namespace aura

namespace views {
class Widget;
}  // namespace views

namespace arc {

// Callback to notify user's confirmation for allowing to resize the app.
// If user accept it, the callback is invoked with 1st argument true.
// Otherwise, with false.
// If the user marked the "Don't ask me again", 2nd argument will be true.
using ResizeConfirmationCallback = base::OnceCallback<void(bool, bool)>;

// Shows confirmation dialog for asking user if really want to perform resize
// operation for the resize-locked ARC app.
views::Widget* ShowResizeConfirmationDialog(
    aura::Window* parent,
    ResizeConfirmationCallback callback);

}  // namespace arc

#endif  // COMPONENTS_ARC_COMPAT_MODE_RESIZE_CONFIRMATION_DIALOG_H_
