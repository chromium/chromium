// Copyright 2020 The Chromium Authors. All rights reserved.
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

// Checks for updates and returns the result in the reply block.
- (void)checkForUpdatesWithUpdateState:
            (CRUUpdateStateObserver* _Nonnull)updateState
                                 reply:(void (^_Nonnull)(int rc))reply;

// Runs periodic updater tasks like checking for uninstalls and background
// update checks.
- (void)runPeriodicTasksWithReply:(void (^_Nullable)(void))reply;

// Checks for update of a given app, with specified priority. Sends repeated
// updates of progress and returns the result in the reply block.
- (void)checkForUpdateWithAppID:(NSString* _Nonnull)appID
                       priority:(CRUPriorityWrapper* _Nonnull)priority
                    updateState:(CRUUpdateStateObserver* _Nonnull)updateState
                          reply:(void (^_Nonnull)(int rc))reply;

// Registers app and returns the result in the reply block.
- (void)registerForUpdatesWithAppId:(NSString* _Nullable)appId
                          brandCode:(NSString* _Nullable)brandCode
                                tag:(NSString* _Nullable)tag
                            version:(NSString* _Nullable)version
               existenceCheckerPath:(NSString* _Nullable)existenceCheckerPath
                              reply:(void (^_Nonnull)(int rc))reply;

@end

// Protocol for the XPC update service internal tasks of the Updater.
@protocol CRUUpdateServicingInternal <NSObject>

// Performs the task (activate service, uninstall service, or no-op) that is
// relevant to the state of the Updater.
- (void)performTasksWithReply:(void (^_Nullable)(void))reply;

// Performs the task that is relevant to the state of the Updater.
// Does not perform an UpdateCheck.
- (void)performInitializeUpdateServiceWithReply:(void (^_Nullable)(void))reply;

@end

namespace updater {

// Constructs an NSXPCInterface for a connection using CRUUpdateServicing
// and CRUUpdateStateObserving protocols.
NSXPCInterface* _Nonnull GetXPCUpdateServicingInterface();

// Constructs an NSXPCInterface for a connection using
// CRUUpdateServicingInternal protocol.
NSXPCInterface* _Nonnull GetXPCUpdateServicingInternalInterface();

}  // namespace updater

#endif  // CHROME_UPDATER_APP_SERVER_MAC_SERVICE_PROTOCOL_H_
