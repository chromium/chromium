// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LOGIN_OOBE_DIALOG_SIZE_UTILS_H_
#define CHROME_BROWSER_UI_ASH_LOGIN_OOBE_DIALOG_SIZE_UTILS_H_

#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace ash {

// Exposed for testing.
extern const gfx::Size kMaxDialogSize;
extern const gfx::Size kMinDialogSize;
extern const gfx::Insets kMinMargins;
extern const gfx::Size kMaxLandscapeDialogSize;
extern const gfx::Size kMinLandscapeDialogSize;
extern const gfx::Size kMaxPortraitDialogSize;
extern const gfx::Size kMinPortraitDialogSize;

extern const int kOobeDialogShadowElevation;
extern const int kOobeDialogCornerRadius;

// Calculated the size of OOBE dialog for particular host_size. It is expected
// that `host_size` is the hosting display size for fullscreen OOBE dialog and
// available remainig size for on-login OOBE.
gfx::Size CalculateOobeDialogSize(const gfx::Size& host_size,
                                  int shelf_height,
                                  bool is_horizontal);

// Calculated the size of OOBE dialog for primary display by passing primary
// display screen size and shelf height to `CalculateOobeDialogSize`.
gfx::Size CalculateOobeDialogSizeForPrimaryDisplay();

// Position OOBE dialog according to specs inside `host_bounds` excluding shelf.
// `host_bounds` is in coordinates of oobe dialog widget. `result` is
// in the same coordinates of `host_bounds`.
void CalculateOobeDialogBounds(const gfx::Rect& host_bounds,
                               int shelf_height,
                               bool is_horizontal,
                               gfx::Rect* result);

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_LOGIN_OOBE_DIALOG_SIZE_UTILS_H_
