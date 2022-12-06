// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_APP_SERVER_MAC_SERVICE_PROTOCOL_H_
#define CHROME_UPDATER_APP_SERVER_MAC_SERVICE_PROTOCOL_H_

#import <Foundation/Foundation.h>

#include "chrome/updater/registration_data.h"
#include "chrome/updater/update_service.h"

@class CRUUpdateStateObserver;
@class CRUUpdateStateWrapper;
@class CRUPriorityWrapper;
@class CRUPolicySameVersionUpdateWrapper;
@class CRUAppStatesWrapper;

// Protocol which observes the state of the XPC update checking service.
@protocol CRUUpdateStateObserving <NSObject>

// Checks for updates and returns the result in the reply block.
- (void)observeUpdateState:(CRUUpdateStateWrapper* _Nonnull)updateState;

@end

// Protocol for the XPC update checking service.
@protocol CRUUpdateServicing <NSObject>

// Checks for the updater's version and returns the result in the reply block.
- (void)getVersionWithReply:
    (void (^_Nonnull)(NSString* _Nullable version))reply;

// Fetches policies from device management.
- (void)fetchPoliciesWithReply:(void (^_Nullable)(int))reply;

// Checks for updates and returns the result in the reply block.
- (void)checkForUpdatesWithUpdateState:
            (CRUUpdateStateObserver* _Nonnull)updateState
                                 reply:(void (^_Nonnull)(int rc))reply;

// Runs periodic updater tasks like checking for uninstalls and background
// update checks.
- (void)runPeriodicTasksWithReply:(void (^_Nullable)(void))reply;

// Checks for update of a given app, with specified priority. Sends repeated
// updates of progress and returns the result in the reply block.
- (void)checkForUpdateWithAppId:(NSString* _Nonnull)appId
               installDataIndex:(NSString* _Nullable)installDataIndex
                       priority:(CRUPriorityWrapper* _Nonnull)priority
        policySameVersionUpdate:
            (CRUPolicySameVersionUpdateWrapper* _Nonnull)policySameVersionUpdate
                    updateState:(CRUUpdateStateObserver* _Nonnull)updateState
                          reply:(void (^_Nonnull)(int rc))reply;

// Installs the given app.
- (void)installWithAppId:(NSString* _Nonnull)appId
               brandCode:(NSString* _Nullable)brandCode
               brandPath:(NSString* _Nullable)brandPath
                     tag:(NSString* _Nullable)ap
                 version:(NSString* _Nullable)version
    existenceCheckerPath:(NSString* _Nullable)existenceCheckerPath
       clientInstallData:(NSString* _Nullable)clientInstallData
        installDataIndex:(NSString* _Nullable)installDataIndex
                priority:(CRUPriorityWrapper* _Nonnull)priority
             updateState:(CRUUpdateStateObserver* _Nonnull)updateState
                   reply:(void (^_Nonnull)(int rc))reply;

// Cancels any in-progress installations for the app ID.
- (void)cancelInstallsWithAppId:(NSString* _Nonnull)appId;

// Registers app and returns the result in the reply block.
- (void)registerForUpdatesWithAppId:(NSString* _Nullable)appId
                          brandCode:(NSString* _Nullable)brandCode
                          brandPath:(NSString* _Nullable)brandPath
                                tag:(NSString* _Nullable)ap
                            version:(NSString* _Nullable)version
               existenceCheckerPath:(NSString* _Nullable)existenceCheckerPath
                              reply:(void (^_Nonnull)(int rc))reply;

// Gets states of all registered apps.
- (void)getAppStatesWithReply:
    (void (^_Nonnull)(CRUAppStatesWrapper* _Nullable apps))reply;

- (void)runInstallerWithAppId:(NSString* _Nonnull)appId
                installerPath:(NSString* _Nonnull)installerPath
                  installArgs:(NSString* _Nullable)installArgs
                  installData:(NSString* _Nullable)installData
              installSettings:(NSString* _Nullable)installSettings
                  updateState:(CRUUpdateStateObserver* _Nonnull)updateState
                        reply:(void (^_Nonnull)(
                                  updater::UpdateService::Result rc))reply;

@end

namespace updater {

// Constructs an NSXPCInterface for a connection using CRUUpdateServicing
// and CRUUpdateStateObserving protocols.
NSXPCInterface* _Nonnull GetXPCUpdateServicingInterface();

}  // namespace updater

#endif  // CHROME_UPDATER_APP_SERVER_MAC_SERVICE_PROTOCOL_H_
