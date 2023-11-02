// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_APP_SERVER_MAC_UPDATE_SERVICE_WRAPPERS_H_
#define CHROME_UPDATER_APP_SERVER_MAC_UPDATE_SERVICE_WRAPPERS_H_

#import <Foundation/Foundation.h>

#include <vector>

#include "base/task/sequenced_task_runner.h"
#import "chrome/updater/app/server/mac/service_protocol.h"
#include "chrome/updater/update_service.h"

using StateChangeCallback =
    base::RepeatingCallback<void(const updater::UpdateService::UpdateState&)>;

@interface CRUUpdateStateObserver : NSObject <CRUUpdateStateObserving>

@property(readonly, nonatomic) StateChangeCallback callback;

- (instancetype)initWithRepeatingCallback:(StateChangeCallback)callback
                           callbackRunner:
                               (scoped_refptr<base::SequencedTaskRunner>)
                                   callbackRunner;

@end

@interface CRUUpdateStateStateWrapper : NSObject <NSSecureCoding>

@property(readonly, nonatomic)
    updater::UpdateService::UpdateState::State updateStateState;

- (instancetype)initWithUpdateStateState:
    (updater::UpdateService::UpdateState::State)updateStateState;

@end

@interface CRUPriorityWrapper : NSObject <NSSecureCoding>

@property(readonly, nonatomic) updater::UpdateService::Priority priority;

- (instancetype)initWithPriority:(updater::UpdateService::Priority)priority;

@end

@interface CRUPolicySameVersionUpdateWrapper : NSObject <NSSecureCoding>

@property(readonly, nonatomic)
    updater::UpdateService::PolicySameVersionUpdate policySameVersionUpdate;

- (instancetype)initWithPolicySameVersionUpdate:
    (updater::UpdateService::PolicySameVersionUpdate)policySameVersionUpdate;

@end

@interface CRUErrorCategoryWrapper : NSObject <NSSecureCoding>

@property(readonly, nonatomic)
    updater::UpdateService::ErrorCategory errorCategory;

- (instancetype)initWithErrorCategory:
    (updater::UpdateService::ErrorCategory)errorCategory;

@end

@interface CRUUpdateStateWrapper : NSObject <NSSecureCoding>

@property(readonly, nonatomic) updater::UpdateService::UpdateState updateState;

@property(readonly, nonatomic) NSString* appId;
@property(readonly, nonatomic) CRUUpdateStateStateWrapper* state;
@property(readonly, nonatomic) NSString* version;
@property(readonly, nonatomic) int64_t downloadedBytes;
@property(readonly, nonatomic) int64_t totalBytes;
@property(readonly, nonatomic) int installProgress;
@property(readonly, nonatomic) CRUErrorCategoryWrapper* errorCategory;
@property(readonly, nonatomic) int errorCode;
@property(readonly, nonatomic) int extraCode;

- (instancetype)initWithAppId:(NSString*)appId
                        state:(CRUUpdateStateStateWrapper*)state
                      version:(NSString*)version
              downloadedBytes:(int64_t)downloadedBytes
                   totalBytes:(int64_t)totalBytes
              installProgress:(int)installProgress
                errorCategory:(CRUErrorCategoryWrapper*)errorCategory
                    errorCode:(int)errorCode
                    extraCode:(int)extraCode;

@end

@interface CRUAppStateWrapper : NSObject <NSSecureCoding>

@property(readonly, nonatomic) updater::UpdateService::AppState state;
- (instancetype)initWithAppState:
                    (const updater::UpdateService::AppState&)appState
                  restrictedView:(bool)restrictedView;
@end

@interface CRUAppStatesWrapper : NSObject <NSSecureCoding>

@property(readonly, nonatomic, getter=states)
    std::vector<updater::UpdateService::AppState>
        states;

- (instancetype)initWithAppStateWrappers:
    (NSArray<CRUAppStateWrapper*>*)appStateWrappers;
- (instancetype)initWithAppStates:
                    (const std::vector<updater::UpdateService::AppState>&)
                        appStates
                   restrictedView:(bool)restrictedView;

@end

#endif  // CHROME_UPDATER_APP_SERVER_MAC_UPDATE_SERVICE_WRAPPERS_H_
