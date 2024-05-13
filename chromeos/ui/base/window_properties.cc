// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/base/window_properties.h"

#include "chromeos/ui/base/chromeos_ui_constants.h"
#include "chromeos/ui/base/window_pin_type.h"
#include "chromeos/ui/base/window_state_type.h"
#include "ui/base/class_property.h"
#include "ui/gfx/geometry/rect.h"

DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(COMPONENT_EXPORT(CHROMEOS_UI_BASE),
                                       chromeos::AppType)
DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(COMPONENT_EXPORT(CHROMEOS_UI_BASE),
                                       chromeos::WindowPinType)
DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(COMPONENT_EXPORT(CHROMEOS_UI_BASE),
                                       chromeos::WindowStateType)

namespace chromeos {

DEFINE_UI_CLASS_PROPERTY_KEY(AppType, kAppTypeKey, AppType::NON_APP)

DEFINE_UI_CLASS_PROPERTY_KEY(bool, kAutoMaximizeXdgShellEnabled, true)

DEFINE_UI_CLASS_PROPERTY_KEY(bool, kBlockedForAssistantSnapshotKey, false)

DEFINE_UI_CLASS_PROPERTY_KEY(bool, kEscHoldToExitFullscreen, false)
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kUseOverviewToExitFullscreen, false)
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kUseOverviewToExitPointerLock, false)
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kNoExitFullscreenOnLock, false)

DEFINE_UI_CLASS_PROPERTY_KEY(bool, kTrackDefaultFrameColors, true)
DEFINE_UI_CLASS_PROPERTY_KEY(SkColor, kFrameActiveColorKey, kDefaultFrameColor)
DEFINE_UI_CLASS_PROPERTY_KEY(SkColor,
                             kFrameInactiveColorKey,
                             kDefaultFrameColor)

DEFINE_UI_CLASS_PROPERTY_KEY(bool, kFrameRestoreLookKey, false)

DEFINE_UI_CLASS_PROPERTY_KEY(bool, kHideShelfWhenFullscreenKey, true)
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kImmersiveImpliedByFullscreen, true)
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kImmersiveIsActive, false)
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(gfx::Rect,
                                   kImmersiveTopContainerBoundsInScreen,
                                   nullptr)

DEFINE_UI_CLASS_PROPERTY_KEY(bool, kIsGameKey, false)

DEFINE_UI_CLASS_PROPERTY_KEY(bool, kIsShowingInOverviewKey, false)

DEFINE_UI_CLASS_PROPERTY_KEY(bool, kShouldHaveHighlightBorderOverlay, false)

DEFINE_UI_CLASS_PROPERTY_KEY(bool, kSupportsFloatedStateKey, true)

DEFINE_UI_CLASS_PROPERTY_KEY(bool, kWindowManagerManagesOpacityKey, false)

DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(std::u16string,
                                   kWindowOverviewTitleKey,
                                   nullptr)

DEFINE_UI_CLASS_PROPERTY_KEY(WindowStateType,
                             kWindowStateTypeKey,
                             WindowStateType::kDefault)

}  // namespace chromeos
