// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_COMPAT_MODE_OVERLAY_DIALOG_H_
#define COMPONENTS_ARC_COMPAT_MODE_OVERLAY_DIALOG_H_

#include <memory>

namespace aura {
class Window;
}  // namespace aura

namespace views {
class View;
}  // namespace views

namespace arc {

// Show |dialog_view| on |base_window| with "scrim" (semi-transparent black)
// background and horizontal margin.
// The |dialog_view|'s width is responsive to the width of |base_window|. It
// matches the |base_window|'s width inside the horizontal margin unless it
// exceeds the |dialog_view|'s preferred width. Note that if |base_window| has
// another overlay already, the view will not be added to the view tree.
void ShowOverlayDialog(aura::Window* base_window,
                       std::unique_ptr<views::View> dialog_view);

// Close overlay view on |base_window| if it has any.
void CloseOverlayDialogIfAny(aura::Window* base_window);

}  // namespace arc

#endif  // COMPONENTS_ARC_COMPAT_MODE_OVERLAY_DIALOG_H_
