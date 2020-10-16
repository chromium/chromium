// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/base/window_properties.h"

#include "chromeos/ui/base/window_state_type.h"
// TODO(crbug.com/1138662): Remove this include and the associated property
// and histogram entry.
#include "chromeos/ui/frame/immersive/immersive_fullscreen_controller.h"  // nogncheck
#include "ui/aura/window.h"

namespace chromeos {

DEFINE_UI_CLASS_PROPERTY_KEY(bool, kImmersiveImpliedByFullscreen, true)
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kImmersiveIsActive, false)
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(gfx::Rect,
                                   kImmersiveTopContainerBoundsInScreen,
                                   nullptr)
DEFINE_UI_CLASS_PROPERTY_KEY(
    int,
    kImmersiveWindowType,
    ImmersiveFullscreenController::WindowType::WINDOW_TYPE_OTHER)

DEFINE_UI_CLASS_PROPERTY_KEY(bool, kIsShowingInOverviewKey, false)

DEFINE_UI_CLASS_PROPERTY_KEY(WindowStateType,
                             kWindowStateTypeKey,
                             WindowStateType::kDefault)

}  // namespace chromeos
