// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/help/version_updater_chromeos.h"

#include <memory>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "chrome/browser/chromeos/login/users/mock_user_manager.h"
#include "chrome/browser/chromeos/settings/scoped_cros_settings_test_helper.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_update_engine_client.h"
#include "chromeos/dbus/shill/shill_service_client.h"
#include "chromeos/network/network_handler.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using ::testing::AtLeast;
using ::testing::Return;

namespace chromeos {

namespace {

void CheckNotification(VersionUpdater::Status /* status */,
                       int /* progress */,
                       bool /* rollback */,
                       const std::string& /* version */,
                       int64_t /* size */,
                       const base::string16& /* message */) {}

}  // namespace

class VersionUpdaterCrosTest : public ::testing::Test {
 protected:
  VersionUpdaterCrosTest()
      : version_updater_(VersionUpdater::Create(nullptr)),
        fake_update_engine_client_(NULL),
        mock_user_manager_(new MockUserManager()),
        user_manager_enabler_(base::WrapUnique(mock_user_manager_)) {}

  ~VersionUpdaterCrosTest() override {}

  void SetUp() override {
    fake_update_engine_client_ = new FakeUpdateEngineClient();
    std::unique_ptr<DBusThreadManagerSetter> dbus_setter =
        DBusThreadManager::GetSetterForTesting();
    dbus_setter->SetUpdateEngineClient(
        std::unique_ptr<UpdateEngineClient>(fake_update_engine_client_));

    EXPECT_CALL(*mock_user_manager_, IsCurrentUserOwner())
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*mock_user_manager_, Shutdown()).Times(AtLeast(0));

    NetworkHandler::Initialize();
    base::RunLoop().RunUntilIdle();
  }

  void SetEthernetService() {
    ShillServiceClient::TestInterface* service_test =
        DBusThreadManager::Get()->GetShillServiceClient()->GetTestInterface();
    service_test->ClearServices();
    service_test->AddService("/service/eth",
                             "eth" /* guid */,
                             "eth",
                             shill::kTypeEthernet, shill::kStateOnline,
                             true /* visible */);
    base::RunLoop().RunUntilIdle();
  }

  void SetCellularService() {
    ShillServiceClient::TestInterface* service_test =
        DBusThreadManager::Get()->GetShillServiceClient()->GetTestInterface();
    service_test->ClearServices();
    service_test->AddService("/service/cell", "cell" /* guid */, "cell",
                             shill::kTypeCellular, shill::kStateOnline,
                             true /* visible */);
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    NetworkHandler::Shutdown();
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<VersionUpdater> version_updater_;
  FakeUpdateEngineClient* fake_update_engine_client_;  // Not owned.

  MockUserManager* mock_user_manager_;  // Not owned.
  user_manager::ScopedUserManager user_manager_enabler_;
  ScopedCrosSettingsTestHelper cros_settings_test_helper_;

  DISALLOW_COPY_AND_ASSIGN(VersionUpdaterCrosTest);
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
  version_updater_->CheckForUpdate(base::Bind(&CheckNotification),
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

  version_updater_->CheckForUpdate(base::Bind(&CheckNotification),
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
  version_updater_->CheckForUpdate(base::Bind(&CheckNotification),
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
      base::Bind(&CheckNotification), update_version, update_size);
  EXPECT_EQ(1, fake_update_engine_client_->request_update_check_call_count());
}

}  // namespace chromeos
