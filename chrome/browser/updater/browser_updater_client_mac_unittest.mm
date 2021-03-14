// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/updater/browser_updater_client_mac.h"

#import <Foundation/Foundation.h>

#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/run_loop.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ui/webui/help/version_updater.h"
#include "chrome/browser/updater/browser_updater_client_util.h"
#import "chrome/updater/app/server/mac/service_protocol.h"
#import "chrome/updater/app/server/mac/update_service_wrappers.h"
#include "chrome/updater/update_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kUnittestAppId[] = "cr-unknown";
constexpr char kUnittestVersion[] = "0.0.0.0";
constexpr char kFakeAppId[] = "fakebundleid";

base::FilePath GetPerUserUpdaterPath() {
  return base::mac::GetUserLibraryPath().Append(GetUpdaterFolderName());
}

}  // namespace

@interface TestCRUUpdateClientOnDemandImpl : CRUUpdateClientOnDemandImpl {
  bool _useStateChange;
  updater::UpdateService::UpdateState::State _statusValue;
}

- (instancetype)initWithStatusValue:
                    (updater::UpdateService::UpdateState::State)statusValue
                     useStateUpdate:(bool)useStateChange;
@end

@implementation TestCRUUpdateClientOnDemandImpl

- (instancetype)initWithStatusValue:
                    (updater::UpdateService::UpdateState::State)statusValue
                     useStateUpdate:(bool)useStateChange {
  if (self = [super init]) {
    _statusValue = statusValue;
    _useStateChange = useStateChange;
  }

  return self;
}

- (void)getUpdaterVersionWithReply:(void (^_Nonnull)(NSString* version))reply {
  NOTIMPLEMENTED();
}

- (void)registerForUpdatesWithAppId:(NSString* _Nullable)appId
                          brandCode:(NSString* _Nullable)brandCode
                                tag:(NSString* _Nullable)tag
                            version:(NSString* _Nullable)version
               existenceCheckerPath:(NSString* _Nullable)existenceCheckerPath
                              reply:(void (^_Nonnull)(int rc))reply {
  reply(static_cast<int>(_statusValue));
}

// Checks for updates and returns the result in the reply block.
- (void)checkForUpdatesWithUpdateState:
            (CRUUpdateStateObserver* _Nonnull)updateState
                                 reply:(void (^_Nonnull)(int rc))reply {
  NOTIMPLEMENTED();
}

// Checks for update of a given app, with specified priority. Sends repeated
// updates of progress and returns the result in the reply block.
- (void)checkForUpdateWithAppID:(NSString* _Nonnull)appID
                       priority:(CRUPriorityWrapper* _Nonnull)priority
                    updateState:(CRUUpdateStateObserver* _Nonnull)updateState
                          reply:(void (^_Nonnull)(int rc))reply {
  if (_useStateChange) {
    base::scoped_nsobject<CRUUpdateStateStateWrapper> stateWrapper(
        [[CRUUpdateStateStateWrapper alloc]
            initWithUpdateStateState:_statusValue]);
    base::scoped_nsobject<CRUErrorCategoryWrapper> errorCategoryWrapper(
        [[CRUErrorCategoryWrapper alloc]
            initWithErrorCategory:updater::UpdateService::ErrorCategory::
                                      kNone]);

    base::scoped_nsobject<CRUUpdateStateWrapper> wrapper(
        [[CRUUpdateStateWrapper alloc]
              initWithAppId:base::SysUTF8ToNSString(kUnittestAppId)
                      state:stateWrapper
                    version:base::SysUTF8ToNSString(kUnittestVersion)
            downloadedBytes:0
                 totalBytes:100  // use a fake total bytes to curb FPE_INTDIV
            installProgress:0
              errorCategory:errorCategoryWrapper
                  errorCode:5
                  extraCode:12]);
    [updateState observeUpdateState:wrapper];
  } else {
    reply(static_cast<int>(_statusValue));
  }
}

- (void)haltForUpdateToVersion:(NSString* _Nonnull)version
                         reply:(void (^_Nonnull)(BOOL shouldUpdate))reply {
  NOTIMPLEMENTED();
}
@end

class UpdateClientMacTest : public ::testing::Test {
 public:
  scoped_refptr<BrowserUpdaterClientMac> update_client() const {
    return update_client_;
  }
  void set_update_client(scoped_refptr<BrowserUpdaterClientMac> client) {
    update_client_ = client;
  }

 protected:
  void SetUp() override {
    base::mac::SetBaseBundleID(kFakeAppId);

    if (!base::PathExists(GetPerUserUpdaterPath()))
      ASSERT_TRUE(base::CreateDirectory(GetPerUserUpdaterPath()));
  }

  void TearDown() override {
    if (base::PathExists(GetPerUserUpdaterPath()))
      ASSERT_TRUE(base::DeletePathRecursively(GetPerUserUpdaterPath()));
  }

 private:
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<BrowserUpdaterClientMac> update_client_;
};

TEST_F(UpdateClientMacTest, SuccessfullyUpdatedStatus) {
  base::RunLoop run_loop;
  updater::UpdateService::UpdateState::State expected_status =
      updater::UpdateService::UpdateState::State::kUpdated;

  set_update_client(base::MakeRefCounted<BrowserUpdaterClientMac>(
      base::scoped_nsobject<CRUUpdateClientOnDemandImpl>(
          [[TestCRUUpdateClientOnDemandImpl alloc]
              initWithStatusValue:expected_status
                   useStateUpdate:true])));

  update_client()->CheckForUpdate(
      base::BindRepeating(base::BindLambdaForTesting(
          [&](updater::UpdateService::UpdateState update_state) {
            // Ignore checking for updates - this is the initial status from
            // running CheckForUpdates.
            if (update_state.state ==
                updater::UpdateService::UpdateState::State::kCheckingForUpdates)
              return;

            EXPECT_EQ(expected_status, update_state.state);
            run_loop.Quit();
          })));
  run_loop.Run();
}

TEST_F(UpdateClientMacTest, UpdateDownloadingStatus) {
  base::RunLoop run_loop;
  updater::UpdateService::UpdateState::State expected_status =
      updater::UpdateService::UpdateState::State::kDownloading;

  set_update_client(base::MakeRefCounted<BrowserUpdaterClientMac>(
      base::scoped_nsobject<CRUUpdateClientOnDemandImpl>(
          [[TestCRUUpdateClientOnDemandImpl alloc]
              initWithStatusValue:expected_status
                   useStateUpdate:true])));

  update_client()->CheckForUpdate(
      base::BindRepeating(base::BindLambdaForTesting(
          [&](updater::UpdateService::UpdateState update_state) {
            // Ignore checking for updates - this is the initial status from
            // running CheckForUpdates.
            if (update_state.state ==
                updater::UpdateService::UpdateState::State::kCheckingForUpdates)
              return;

            EXPECT_EQ(expected_status, update_state.state);
            run_loop.Quit();
          })));
  run_loop.Run();
}

TEST_F(UpdateClientMacTest, NoUpdateStatus) {
  base::RunLoop run_loop;
  updater::UpdateService::UpdateState::State expected_status =
      updater::UpdateService::UpdateState::State::kNoUpdate;

  set_update_client(base::MakeRefCounted<BrowserUpdaterClientMac>(
      base::scoped_nsobject<CRUUpdateClientOnDemandImpl>(
          [[TestCRUUpdateClientOnDemandImpl alloc]
              initWithStatusValue:expected_status
                   useStateUpdate:true])));

  update_client()->CheckForUpdate(
      base::BindRepeating(base::BindLambdaForTesting(
          [&](updater::UpdateService::UpdateState update_state) {
            // Ignore checking for updates - this is the initial status from
            // running CheckForUpdates.
            if (update_state.state ==
                updater::UpdateService::UpdateState::State::kCheckingForUpdates)
              return;

            EXPECT_EQ(expected_status, update_state.state);
            run_loop.Quit();
          })));
  run_loop.Run();
}

TEST_F(UpdateClientMacTest, ErrorStatus) {
  base::RunLoop run_loop;
  updater::UpdateService::UpdateState::State expected_status =
      updater::UpdateService::UpdateState::State::kUpdateError;

  set_update_client(base::MakeRefCounted<BrowserUpdaterClientMac>(
      base::scoped_nsobject<CRUUpdateClientOnDemandImpl>(
          [[TestCRUUpdateClientOnDemandImpl alloc]
              initWithStatusValue:expected_status
                   useStateUpdate:true])));

  update_client()->CheckForUpdate(
      base::BindRepeating(base::BindLambdaForTesting(
          [&](updater::UpdateService::UpdateState update_state) {
            // Ignore checking for updates - this is the initial status from
            // running CheckForUpdates.
            if (update_state.state ==
                updater::UpdateService::UpdateState::State::kCheckingForUpdates)
              return;

            EXPECT_EQ(expected_status, update_state.state);
            run_loop.Quit();
          })));
  run_loop.Run();
}
