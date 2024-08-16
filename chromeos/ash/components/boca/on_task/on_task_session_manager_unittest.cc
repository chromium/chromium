// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/on_task/on_task_session_manager.h"

#include <memory>

#include "base/functional/callback.h"
#include "chromeos/ash/components/boca/on_task/on_task_system_web_app_manager.h"
#include "chromeos/ash/components/boca/proto/roster.pb.h"
#include "chromeos/ash/components/boca/proto/session.pb.h"
#include "components/sessions/core/session_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Return;

namespace ash::boca {
namespace {

// Mock implementation of the `OnTaskSystemWebAppManager`.
class OnTaskSystemWebAppManagerMock : public OnTaskSystemWebAppManager {
 public:
  OnTaskSystemWebAppManagerMock() = default;
  ~OnTaskSystemWebAppManagerMock() override = default;

  MOCK_METHOD(void,
              LaunchSystemWebAppAsync,
              (base::OnceCallback<void(bool)>),
              (override));
  MOCK_METHOD(void, CloseSystemWebAppWindow, (SessionID window_id), (override));
  MOCK_METHOD(SessionID, GetActiveSystemWebAppWindowID, (), (override));
  MOCK_METHOD(void,
              SetPinStateForSystemWebAppWindow,
              (bool pinned, SessionID window_id),
              (override));
  MOCK_METHOD(void,
              SetWindowTrackerForSystemWebAppWindow,
              (SessionID window_id),
              (override));
};

class OnTaskSessionManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    auto system_web_app_manager =
        std::make_unique<OnTaskSystemWebAppManagerMock>();
    system_web_app_manager_ptr_ = system_web_app_manager.get();
    session_manager_ = std::make_unique<OnTaskSessionManager>(
        std::move(system_web_app_manager));
  }

  std::unique_ptr<OnTaskSessionManager> session_manager_;
  raw_ptr<OnTaskSystemWebAppManagerMock> system_web_app_manager_ptr_;
};

TEST_F(OnTaskSessionManagerTest, ShouldLaunchBocaSWAOnSessionStart) {
  EXPECT_CALL(*system_web_app_manager_ptr_, GetActiveSystemWebAppWindowID())
      .Times(2)
      .WillRepeatedly(Return(SessionID::InvalidValue()));
  EXPECT_CALL(*system_web_app_manager_ptr_, LaunchSystemWebAppAsync(_))
      .WillOnce([](base::OnceCallback<void(bool)> callback) {
        std::move(callback).Run(true);
      });
  session_manager_->OnSessionStarted("test_session_id", ::boca::UserIdentity());
}

TEST_F(OnTaskSessionManagerTest, ShouldPrepareBocaSWAOnLaunch) {
  const SessionID kWindowId = SessionID::NewUnique();
  EXPECT_CALL(*system_web_app_manager_ptr_,
              GetActiveSystemWebAppWindowID())
      .WillOnce(
          Return(SessionID::InvalidValue()))  // Initial check before launch.
      .WillOnce(Return(kWindowId));
  EXPECT_CALL(*system_web_app_manager_ptr_,
              SetWindowTrackerForSystemWebAppWindow(kWindowId))
      .Times(1);
  EXPECT_CALL(*system_web_app_manager_ptr_,
              SetPinStateForSystemWebAppWindow(true, kWindowId))
      .Times(1);
  EXPECT_CALL(*system_web_app_manager_ptr_,
              SetPinStateForSystemWebAppWindow(false, kWindowId))
      .Times(1);
  EXPECT_CALL(*system_web_app_manager_ptr_, LaunchSystemWebAppAsync(_))
      .WillOnce([](base::OnceCallback<void(bool)> callback) {
        std::move(callback).Run(true);
      });
  session_manager_->OnSessionStarted("test_session_id", ::boca::UserIdentity());
}

TEST_F(OnTaskSessionManagerTest, ShouldClosePreExistingBocaSWAOnSessionStart) {
  const SessionID kWindowId = SessionID::NewUnique();
  EXPECT_CALL(*system_web_app_manager_ptr_, GetActiveSystemWebAppWindowID())
      .WillOnce(Return(kWindowId))
      .WillRepeatedly(Return(SessionID::InvalidValue()));
  EXPECT_CALL(*system_web_app_manager_ptr_, CloseSystemWebAppWindow(kWindowId))
      .Times(1);
  EXPECT_CALL(*system_web_app_manager_ptr_, LaunchSystemWebAppAsync(_))
      .WillOnce([](base::OnceCallback<void(bool)> callback) {
        std::move(callback).Run(true);
      });
  session_manager_->OnSessionStarted("test_session_id", ::boca::UserIdentity());
}

TEST_F(OnTaskSessionManagerTest, ShouldCloseBocaSWAOnSessionEnd) {
  const SessionID kWindowId = SessionID::NewUnique();
  EXPECT_CALL(*system_web_app_manager_ptr_, GetActiveSystemWebAppWindowID())
      .WillOnce(Return(kWindowId));
  EXPECT_CALL(*system_web_app_manager_ptr_, CloseSystemWebAppWindow(kWindowId))
      .Times(1);
  session_manager_->OnSessionEnded("test_session_id");
}

TEST_F(OnTaskSessionManagerTest, ShouldIgnoreWhenNoBocaSWAOpenOnSessionEnd) {
  EXPECT_CALL(*system_web_app_manager_ptr_, GetActiveSystemWebAppWindowID())
      .WillOnce(Return(SessionID::InvalidValue()));
  EXPECT_CALL(*system_web_app_manager_ptr_, CloseSystemWebAppWindow(_))
      .Times(0);
  session_manager_->OnSessionEnded("test_session_id");
}

}  // namespace
}  // namespace ash::boca
