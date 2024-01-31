// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REMOTE_COCOA_APP_SHIM_MENU_CONTROLLER_COCOA_DELEGATE_IMPL_H_
#define COMPONENTS_REMOTE_COCOA_APP_SHIM_MENU_CONTROLLER_COCOA_DELEGATE_IMPL_H_

#include "components/remote_cocoa/app_shim/remote_cocoa_app_shim_export.h"
#include "components/remote_cocoa/common/menu.mojom.h"
#include "third_party/skia/include/core/SkColor.h"
#import "ui/base/cocoa/menu_controller.h"
#include "ui/gfx/font.h"

REMOTE_COCOA_APP_SHIM_EXPORT
@interface MenuControllerCocoaDelegateImpl
    : NSObject <MenuControllerCocoaDelegate>
- (instancetype)initWithParams:
    (remote_cocoa::mojom::MenuControllerParamsPtr)params;
@end

#endif  // COMPONENTS_REMOTE_COCOA_APP_SHIM_MENU_CONTROLLER_COCOA_DELEGATE_IMPL_H_
