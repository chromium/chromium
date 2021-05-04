// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/instance_update.h"
#include "chrome/test/base/testing_profile.h"
#include "components/services/app_service/public/cpp/instance.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char app_id[] = "abcdefgh";
const char test_launch_id0[] = "abc";
const char test_launch_id1[] = "xyz";

}  // namespace

class InstanceUpdateTest : public testing::Test {
 protected:
  void ExpectNoChange() {
    expect_launch_id_changed_ = false;
    expect_state_changed_ = false;
    expect_last_updated_time_changed_ = false;
  }

  void CheckExpects(const apps::InstanceUpdate& u) {
    EXPECT_EQ(expect_launch_id_, u.LaunchId());
    EXPECT_EQ(expect_launch_id_changed_, u.LaunchIdChanged());
    EXPECT_EQ(expect_state_, u.State());
    EXPECT_EQ(expect_state_changed_, u.StateChanged());
    EXPECT_EQ(expect_last_updated_time_, u.LastUpdatedTime());
    EXPECT_EQ(expect_last_updated_time_changed_, u.LastUpdatedTimeChanged());
  }

  void TestInstanceUpdate(apps::Instance* state, apps::Instance* delta) {
    apps::InstanceUpdate u(state, delta);
    EXPECT_EQ(app_id, u.AppId());
    EXPECT_EQ(state == nullptr, u.StateIsNull());
    expect_launch_id_ = base::EmptyString();
    expect_state_ = apps::InstanceState::kUnknown;
    expect_last_updated_time_ = base::Time();

    ExpectNoChange();
    CheckExpects(u);

    if (delta) {
      delta->SetLaunchId(test_launch_id0);
      expect_launch_id_ = test_launch_id0;
      expect_launch_id_changed_ = true;
      CheckExpects(u);
    }
    if (state) {
      state->SetLaunchId(test_launch_id0);
      expect_launch_id_ = test_launch_id0;
      expect_launch_id_changed_ = false;
      CheckExpects(u);
    }
    if (state) {
      apps::InstanceUpdate::Merge(state, delta);
      ExpectNoChange();
      CheckExpects(u);
    }
    if (delta) {
      delta->SetLaunchId(test_launch_id1);
      expect_launch_id_ = test_launch_id1;
      expect_launch_id_changed_ = true;
      CheckExpects(u);
    }

    // State and StateTime tests.
    if (state) {
      state->UpdateState(apps::InstanceState::kRunning,
                         base::Time::FromDoubleT(1000.0));
      expect_state_ = apps::InstanceState::kRunning;
      expect_last_updated_time_ = base::Time::FromDoubleT(1000.0);
      expect_state_changed_ = false;
      expect_last_updated_time_changed_ = false;
      CheckExpects(u);
    }
    if (delta) {
      delta->UpdateState(apps::InstanceState::kActive,
                         base::Time::FromDoubleT(2000.0));
      expect_state_ = apps::InstanceState::kActive;
      expect_last_updated_time_ = base::Time::FromDoubleT(2000.0);
      expect_state_changed_ = true;
      expect_last_updated_time_changed_ = true;
      CheckExpects(u);
    }
    if (state) {
      apps::InstanceUpdate::Merge(state, delta);
      ExpectNoChange();
      CheckExpects(u);
    }
  }

  std::string expect_launch_id_;
  bool expect_launch_id_changed_;
  apps::InstanceState expect_state_;
  bool expect_state_changed_;
  base::Time expect_last_updated_time_;
  bool expect_last_updated_time_changed_;

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(InstanceUpdateTest, StateIsNonNull) {
  aura::Window window(nullptr);
  window.Init(ui::LAYER_NOT_DRAWN);
  std::unique_ptr<apps::Instance> state =
      std::make_unique<apps::Instance>(app_id, &window);
  EXPECT_TRUE(apps::InstanceUpdate::Equals(state.get(), nullptr));
  TestInstanceUpdate(state.get(), nullptr);
}

TEST_F(InstanceUpdateTest, DeltaIsNonNull) {
  aura::Window window(nullptr);
  window.Init(ui::LAYER_NOT_DRAWN);
  std::unique_ptr<apps::Instance> delta =
      std::make_unique<apps::Instance>(app_id, &window);
  EXPECT_FALSE(apps::InstanceUpdate::Equals(nullptr, delta.get()));
  TestInstanceUpdate(nullptr, delta.get());
}

TEST_F(InstanceUpdateTest, BothAreNonNull) {
  aura::Window window(nullptr);
  window.Init(ui::LAYER_NOT_DRAWN);
  std::unique_ptr<apps::Instance> state =
      std::make_unique<apps::Instance>(app_id, &window);
  std::unique_ptr<apps::Instance> delta =
      std::make_unique<apps::Instance>(app_id, &window);
  EXPECT_TRUE(apps::InstanceUpdate::Equals(state.get(), delta.get()));
  TestInstanceUpdate(state.get(), delta.get());
}

TEST_F(InstanceUpdateTest, LaunchIdIsUpdated) {
  aura::Window window(nullptr);
  window.Init(ui::LAYER_NOT_DRAWN);
  std::unique_ptr<apps::Instance> state =
      std::make_unique<apps::Instance>(app_id, &window);
  std::unique_ptr<apps::Instance> delta =
      std::make_unique<apps::Instance>(app_id, &window);
  delta->SetLaunchId("abc");
  EXPECT_FALSE(apps::InstanceUpdate::Equals(state.get(), delta.get()));
}

TEST_F(InstanceUpdateTest, LaunchIdIsNotUpdated) {
  aura::Window window(nullptr);
  window.Init(ui::LAYER_NOT_DRAWN);
  std::unique_ptr<apps::Instance> state =
      std::make_unique<apps::Instance>(app_id, &window);
  state->SetLaunchId("abc");
  std::unique_ptr<apps::Instance> delta =
      std::make_unique<apps::Instance>(app_id, &window);
  EXPECT_TRUE(apps::InstanceUpdate::Equals(state.get(), delta.get()));
}

TEST_F(InstanceUpdateTest, StateIsUpdated) {
  aura::Window window(nullptr);
  window.Init(ui::LAYER_NOT_DRAWN);
  std::unique_ptr<apps::Instance> state =
      std::make_unique<apps::Instance>(app_id, &window);
  std::unique_ptr<apps::Instance> delta =
      std::make_unique<apps::Instance>(app_id, &window);
  delta->UpdateState(apps::InstanceState::kStarted, base::Time::Now());
  EXPECT_FALSE(apps::InstanceUpdate::Equals(state.get(), delta.get()));
}

TEST_F(InstanceUpdateTest, StateIsNotUpdated) {
  aura::Window window(nullptr);
  window.Init(ui::LAYER_NOT_DRAWN);
  std::unique_ptr<apps::Instance> state =
      std::make_unique<apps::Instance>(app_id, &window);
  state->UpdateState(apps::InstanceState::kStarted, base::Time::Now());
  std::unique_ptr<apps::Instance> delta =
      std::make_unique<apps::Instance>(app_id, &window);
  EXPECT_TRUE(apps::InstanceUpdate::Equals(state.get(), delta.get()));
}

TEST_F(InstanceUpdateTest, BothLaunchAndStateIsUpdated) {
  aura::Window window(nullptr);
  window.Init(ui::LAYER_NOT_DRAWN);
  std::unique_ptr<apps::Instance> state =
      std::make_unique<apps::Instance>(app_id, &window);
  state->SetLaunchId("aaa");
  state->UpdateState(apps::InstanceState::kStarted, base::Time::Now());
  std::unique_ptr<apps::Instance> delta =
      std::make_unique<apps::Instance>(app_id, &window);
  delta->SetLaunchId("bbb");
  delta->UpdateState(apps::InstanceState::kRunning, base::Time::Now());
  EXPECT_FALSE(apps::InstanceUpdate::Equals(state.get(), delta.get()));
}

TEST_F(InstanceUpdateTest, BrowserContextIsUpdated) {
  aura::Window window(nullptr);
  window.Init(ui::LAYER_NOT_DRAWN);
  std::unique_ptr<apps::Instance> state =
      std::make_unique<apps::Instance>(app_id, &window);
  std::unique_ptr<apps::Instance> delta =
      std::make_unique<apps::Instance>(app_id, &window);
  delta->SetBrowserContext(&profile_);
  EXPECT_FALSE(apps::InstanceUpdate::Equals(state.get(), delta.get()));
}

TEST_F(InstanceUpdateTest, BrowserContextIsNotUpdated) {
  aura::Window window(nullptr);
  window.Init(ui::LAYER_NOT_DRAWN);
  std::unique_ptr<apps::Instance> state =
      std::make_unique<apps::Instance>(app_id, &window);
  state->SetBrowserContext(&profile_);
  std::unique_ptr<apps::Instance> delta =
      std::make_unique<apps::Instance>(app_id, &window);
  EXPECT_TRUE(apps::InstanceUpdate::Equals(state.get(), delta.get()));
}
