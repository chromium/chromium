// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#import "chrome/updater/app/server/mac/service_protocol.h"

namespace updater {

NSXPCInterface* GetXPCUpdateServicingInterface() {
  NSXPCInterface* updateCheckingInterface =
      [NSXPCInterface interfaceWithProtocol:@protocol(CRUUpdateServicing)];
  NSXPCInterface* updateStateObservingInterface =
      [NSXPCInterface interfaceWithProtocol:@protocol(CRUUpdateStateObserving)];

  [updateCheckingInterface
       setInterface:updateStateObservingInterface
        forSelector:@selector(checkForUpdatesWithUpdateState:reply:)
      argumentIndex:0
            ofReply:NO];

  [updateCheckingInterface
       setInterface:updateStateObservingInterface
        forSelector:@selector
        (checkForUpdateWithAppId:
                installDataIndex:priority:policySameVersionUpdate:updateState
                                :reply:)
      argumentIndex:4
            ofReply:NO];

  [updateCheckingInterface
       setInterface:updateStateObservingInterface
        forSelector:@selector
        (runInstallerWithAppId:
                 installerPath:installArgs:installData:installSettings
                              :updateState:reply:)
      argumentIndex:5
            ofReply:NO];
  return updateCheckingInterface;
}

}  // namespace updater
