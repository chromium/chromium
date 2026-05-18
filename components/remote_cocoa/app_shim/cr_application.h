// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REMOTE_COCOA_APP_SHIM_CR_APPLICATION_H_
#define COMPONENTS_REMOTE_COCOA_APP_SHIM_CR_APPLICATION_H_

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_sending_event.h"
#include "base/message_loop/message_pump_apple.h"
#include "components/remote_cocoa/app_shim/remote_cocoa_app_shim_export.h"

REMOTE_COCOA_APP_SHIM_EXPORT
@interface CrApplication : NSApplication <CrAppProtocol, CrAppControlProtocol>
@end

#endif  // COMPONENTS_REMOTE_COCOA_APP_SHIM_CR_APPLICATION_H_
