// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/help/version_updater_chromeos.h"

#include <memory>
#include <optional>

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chromeos/ash/components/dbus/shill/shill_service_client.h"
#include "chromeos/ash/components/dbus/update_engine/fake_update_engine_client.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::Return;
using ::testing::StrictMock;

namespace chromeos {

namespace {

void CheckNotification(VersionUpdater::Status /* status */,
                       int /* progress */,
                       bool /* rollback */,
                       bool /* powerwash */,
                       const std::string& /* version */,
                       int64_t /* size */,
                       const std::u16string& /* message */) {}

}  // namespace

class VersionUpdaterCrosTest : public ::testing::Test {
 public:
  VersionUpdaterCrosTest(const VersionUpdaterCrosTest&) = delete;
  VersionUpdaterCrosTest& operator=(const VersionUpdaterCrosTest&) = delete;

 protected:
  VersionUpdaterCrosTest()
      : version_updater_(std::make_unique<VersionUpdaterCros>(nullptr)),
        fake_update_engine_client_(nullptr),
        user_manager_enabler_(std::make_unique<FakeChromeUserManager>()) {}

  ~VersionUpdaterCrosTest() override {}

  void SetUp() override {
    fake_update_engine_client_ =
        ash::UpdateEngineClient::InitializeFakeForTest();

    network_handler_test_helper_ =
        std::make_unique<ash::NetworkHandlerTestHelper>();
    base::RunLoop().RunUntilIdle();
  }

  void SetEthernetService() {
    ash::ShillServiceClient::TestInterface* service_test =
        network_handler_test_helper_->service_test();
    service_test->ClearServices();
    service_test->AddService("/service/eth",
                             "eth" /* guid */,
                             "eth",
                             shill::kTypeEthernet, shill::kStateOnline,
                             true /* visible */);
    base::RunLoop().RunUntilIdle();
  }

  void SetCellularService() {
    ash::ShillServiceClient::TestInterface* service_test =
        network_handler_test_helper_->service_test();
    service_test->ClearServices();
    service_test->AddService("/service/cell", "cell" /* guid */, "cell",
                             shill::kTypeCellular, shill::kStateOnline,
                             true /* visible */);
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    network_handler_test_helper_.reset();
    version_updater_.reset();
    ash::UpdateEngineClient::Shutdown();
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<ash::NetworkHandlerTestHelper> network_handler_test_helper_;
  std::unique_ptr<VersionUpdaterCros> version_updater_;
  raw_ptr<ash::FakeUpdateEngineClient, DanglingUntriaged>
      fake_update_engine_client_;  // Not owned.

  user_manager::ScopedUserManager user_manager_enabler_;
  ash::ScopedCrosSettingsTestHelper cros_settings_test_helper_;
};

// The test checks following behaviour:
// 1. The device is currently on the dev channel and an user decides to switch
// to the beta channel.
// 2. In the middle of channel switch the user decides to switch to the stable
// channel.
// 3. Update engine reports an error because downloading channel (beta) is not
// equal
// to the target channel (stable).
// 4. When update engine becomes idle downloading of the stable channel is
// initiated.
TEST_F(VersionUpdaterCrosTest, TwoOverlappingSetChannelRequests) {
  SetEthernetService();
  version_updater_->SetChannel("beta-channel", true);

  {
    update_engine::StatusResult status;
    status.set_current_operation(update_engine::Operation::IDLE);
    fake_update_engine_client_->set_default_status(status);
    fake_update_engine_client_->NotifyObserversThatStatusChanged(status);
  }

  EXPECT_EQ(0, fake_update_engine_client_->request_update_check_call_count());

  // IDLE -> DOWNLOADING transition after update check.
  version_updater_->CheckForUpdate(base::BindRepeating(&CheckNotification),
                                   VersionUpdater::PromoteCallback());
  EXPECT_EQ(1, fake_update_engine_client_->request_update_check_call_count());

  {
    update_engine::StatusResult status;
    status.set_current_operation(update_engine::Operation::DOWNLOADING);
    status.set_progress(0.1);
    fake_update_engine_client_->set_default_status(status);
    fake_update_engine_client_->NotifyObserversThatStatusChanged(status);
  }

  version_updater_->SetChannel("stable-channel", true);

  // DOWNLOADING -> REPORTING_ERROR_EVENT transition since target channel is not
  // equal to downloading channel now.
  {
    update_engine::StatusResult status;
    status.set_current_operation(
        update_engine::Operation::REPORTING_ERROR_EVENT);
    fake_update_engine_client_->set_default_status(status);
    fake_update_engine_client_->NotifyObserversThatStatusChanged(status);
  }

  version_updater_->CheckForUpdate(base::BindRepeating(&CheckNotification),
                                   VersionUpdater::PromoteCallback());
  EXPECT_EQ(1, fake_update_engine_client_->request_update_check_call_count());

  // REPORTING_ERROR_EVENT -> IDLE transition, update check should be
  // automatically scheduled.
  {
    update_engine::StatusResult status;
    status.set_current_operation(update_engine::Operation::IDLE);
    fake_update_engine_client_->set_default_status(status);
    fake_update_engine_client_->NotifyObserversThatStatusChanged(status);
  }

  EXPECT_EQ(2, fake_update_engine_client_->request_update_check_call_count());
}

// Test that when interactively checking for update, cellular connection is
// allowed in Chrome by default, so that the request will be sent to Update
// Engine.
TEST_F(VersionUpdaterCrosTest, InteractiveCellularUpdateAllowed) {
  SetCellularService();
  EXPECT_EQ(0, fake_update_engine_client_->request_update_check_call_count());
  version_updater_->CheckForUpdate(base::BindRepeating(&CheckNotification),
                                   VersionUpdater::PromoteCallback());
  EXPECT_EQ(1, fake_update_engine_client_->request_update_check_call_count());
}

// Test that after update over cellular one time permission is set successfully,
// an update check will be triggered.
TEST_F(VersionUpdaterCrosTest, CellularUpdateOneTimePermission) {
  SetCellularService();
  EXPECT_EQ(0, fake_update_engine_client_->request_update_check_call_count());
  const std::string& update_version = "9999.0.0";
  const int64_t update_size = 99999;
  version_updater_->SetUpdateOverCellularOneTimePermission(
      base::BindRepeating(&CheckNotification), update_version, update_size);
  EXPECT_EQ(1, fake_update_engine_client_->request_update_check_call_count());
}

TEST_F(VersionUpdaterCrosTest, GetUpdateStatus_NoCallbackDuringInstallations) {
  SetEthernetService();
  update_engine::StatusResult status;
  status.set_is_install(true);
  fake_update_engine_client_->set_default_status(status);

  // Expect the callback not to be called as it's an installation (not update).
  StrictMock<base::MockCallback<VersionUpdater::StatusCallback>> mock_callback;
  version_updater_->GetUpdateStatus(mock_callback.Get());
}

TEST_F(VersionUpdaterCrosTest, GetUpdateStatus_CallbackDuringUpdates) {
  SetEthernetService();
  update_engine::StatusResult status;
  fake_update_engine_client_->set_default_status(status);

  // Expect the callbac kto be called as it's an update status change.
  StrictMock<base::MockCallback<VersionUpdater::StatusCallback>> mock_callback;
  EXPECT_CALL(mock_callback, Run(_, _, _, _, _, _, _)).Times(1);
  version_updater_->GetUpdateStatus(mock_callback.Get());
}

TEST_F(VersionUpdaterCrosTest,
       GetUpdateStatus_SetToUpdatedForNonInteractiveDeferredUpdate) {
  SetEthernetService();
  update_engine::StatusResult status;
  // The update is non-interactive and will be deferred.
  status.set_is_interactive(false);
  status.set_will_defer_update(true);
  fake_update_engine_client_->set_default_status(status);

  // Expect to set status to `UPDATED`.
  StrictMock<base::MockCallback<VersionUpdater::StatusCallback>> mock_callback;
  EXPECT_CALL(mock_callback, Run(VersionUpdater::UPDATED, 0, _, _, _, _, _))
      .Times(1);
  version_updater_->GetUpdateStatus(mock_callback.Get());
}

TEST_F(VersionUpdaterCrosTest, GetUpdateStatus_UpdatedButDeferred) {
  SetEthernetService();
  update_engine::StatusResult status;
  // The update is deferred.
  status.set_is_interactive(false);
  status.set_will_defer_update(true);
  status.set_current_operation(update_engine::Operation::UPDATED_BUT_DEFERRED);
  fake_update_engine_client_->set_default_status(status);

  // Expect the status to be `DEFERRED`.
  StrictMock<base::MockCallback<VersionUpdater::StatusCallback>> mock_callback;
  EXPECT_CALL(mock_callback, Run(VersionUpdater::DEFERRED, _, _, _, _, _, _))
      .Times(1);
  version_updater_->GetUpdateStatus(mock_callback.Get());
}

TEST_F(VersionUpdaterCrosTest, GetUpdateStatus_UpdatedNeedReboot) {
  SetEthernetService();
  update_engine::StatusResult status;
  status.set_is_interactive(false);
  status.set_current_operation(update_engine::Operation::UPDATED_NEED_REBOOT);
  fake_update_engine_client_->set_default_status(status);

  // Expect the status to be `NEARLY_UPDATED`.
  StrictMock<base::MockCallback<VersionUpdater::StatusCallback>> mock_callback;
  EXPECT_CALL(mock_callback,
              Run(VersionUpdater::NEARLY_UPDATED, _, _, _, _, _, _))
      .Times(1);
  version_updater_->GetUpdateStatus(mock_callback.Get());
}

TEST_F(VersionUpdaterCrosTest,
       GetUpdateStatus_UpdateToRollbackVersionDisallowed) {
  SetEthernetService();
  update_engine::StatusResult status;
  status.set_is_interactive(true);
  status.set_current_operation(update_engine::Operation::DISABLED);
  int32_t error_code = static_cast<int32_t>(
      update_engine::ErrorCode::kOmahaUpdateIgnoredPerPolicy);
  status.set_last_attempt_error(error_code);
  fake_update_engine_client_->set_default_status(status);

  // Expect the status to be `UPDATE_TO_ROLLBACK_VERSION_DISALLOWED`.
  StrictMock<base::MockCallback<VersionUpdater::StatusCallback>> mock_callback;
  EXPECT_CALL(mock_callback,
              Run(VersionUpdater::UPDATE_TO_ROLLBACK_VERSION_DISALLOWED, _, _,
                  _, _, _, _))
      .Times(1);
  version_updater_->GetUpdateStatus(mock_callback.Get());
}

TEST_F(VersionUpdaterCrosTest, ToggleFeature) {
  EXPECT_EQ(0, fake_update_engine_client_->toggle_feature_count());
  version_updater_->ToggleFeature("feature-foo", true);
  EXPECT_EQ(1, fake_update_engine_client_->toggle_feature_count());
  version_updater_->ToggleFeature("feature-foo", false);
  EXPECT_EQ(2, fake_update_engine_client_->toggle_feature_count());
}

TEST_F(VersionUpdaterCrosTest, IsFeatureEnabled) {
  EXPECT_EQ(0, fake_update_engine_client_->is_feature_enabled_count());

  StrictMock<base::MockCallback<VersionUpdater::IsFeatureEnabledCallback>>
      mock_callback;
  EXPECT_CALL(mock_callback, Run(_)).Times(1);
  version_updater_->IsFeatureEnabled("feature-foo", mock_callback.Get());

  EXPECT_EQ(1, fake_update_engine_client_->is_feature_enabled_count());
}

TEST_F(VersionUpdaterCrosTest, ApplyDeferredUpdate) {
  update_engine::StatusResult status;
  status.set_current_operation(update_engine::Operation::UPDATED_BUT_DEFERRED);
  fake_update_engine_client_->set_default_status(status);
  fake_update_engine_client_->NotifyObserversThatStatusChanged(status);

  EXPECT_EQ(0, fake_update_engine_client_->apply_deferred_update_count());
  version_updater_->ApplyDeferredUpdate();
  EXPECT_EQ(1, fake_update_engine_client_->apply_deferred_update_count());
}

}  // namespace chromeos
