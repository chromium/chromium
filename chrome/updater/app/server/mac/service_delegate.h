// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_APP_SERVER_MAC_SERVICE_DELEGATE_H_
#define CHROME_UPDATER_APP_SERVER_MAC_SERVICE_DELEGATE_H_

#import <Foundation/Foundation.h>

#include "base/memory/scoped_refptr.h"

namespace updater {
class UpdateService;
class AppServerMac;
}  // namespace updater

@interface CRUUpdateCheckServiceXPCDelegate : NSObject <NSXPCListenerDelegate>

- (instancetype)init NS_UNAVAILABLE;

// Designated initializer.
- (instancetype)
    initWithUpdateService:(scoped_refptr<updater::UpdateService>)service
                appServer:(scoped_refptr<updater::AppServerMac>)appServer
    NS_DESIGNATED_INITIALIZER;

@end

#endif  // CHROME_UPDATER_APP_SERVER_MAC_SERVICE_DELEGATE_H_
