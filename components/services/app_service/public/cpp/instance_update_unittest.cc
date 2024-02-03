// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/instance_update.h"

#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
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
    expect_window_changed_ = false;
    expect_launch_id_changed_ = false;
    expect_state_changed_ = false;
    expect_last_updated_time_changed_ = false;
  }

  void CheckExpects(const apps::InstanceUpdate& u) {
    EXPECT_EQ(expect_window_, u.Window());
    EXPECT_EQ(expect_window_changed_, u.WindowChanged());
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

    aura::Window test_window(nullptr);
    test_window.Init(ui::LAYER_NOT_DRAWN);

    expect_launch_id_.clear();
    expect_state_ = apps::InstanceState::kUnknown;
    expect_last_updated_time_ = base::Time();

    ExpectNoChange();

    // Window tests.
    if (state) {
      expect_window_ = state->Window();
      expect_window_changed_ = false;
      CheckExpects(u);
    }
    if (delta) {
      delta->SetWindow(&test_window);
      expect_window_ = &test_window;
      expect_window_changed_ = true;
      CheckExpects(u);
    }
    if (state) {
      apps::InstanceUpdate::Merge(state, delta);
      ExpectNoChange();
      CheckExpects(u);
    }

    // Launch id tests.
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
                         base::Time::FromSecondsSinceUnixEpoch(1000));
      expect_state_ = apps::InstanceState::kRunning;
      expect_last_updated_time_ = base::Time::FromSecondsSinceUnixEpoch(1000);
      expect_state_changed_ = false;
      expect_last_updated_time_changed_ = false;
      CheckExpects(u);
    }
    if (delta) {
      delta->UpdateState(apps::InstanceState::kActive,
                         base::Time::FromSecondsSinceUnixEpoch(2000));
      expect_state_ = apps::InstanceState::kActive;
      expect_last_updated_time_ = base::Time::FromSecondsSinceUnixEpoch(2000);
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

  raw_ptr<aura::Window> expect_window_;
  bool expect_window_changed_;
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
  auto state = std::make_unique<apps::Instance>(
      app_id, base::UnguessableToken::Create(), &window);
  EXPECT_TRUE(apps::InstanceUpdate::Equals(state.get(), nullptr));
  TestInstanceUpdate(state.get(), nullptr);
}

TEST_F(InstanceUpdateTest, DeltaIsNonNull) {
  aura::Window window(nullptr);
  window.Init(ui::LAYER_NOT_DRAWN);
  auto delta = std::make_unique<apps::Instance>(
      app_id, base::UnguessableToken::Create(), &window);
  EXPECT_FALSE(apps::InstanceUpdate::Equals(nullptr, delta.get()));
  TestInstanceUpdate(nullptr, delta.get());
}

TEST_F(InstanceUpdateTest, BothAreNonNull) {
  aura::Window window(nullptr);
  window.Init(ui::LAYER_NOT_DRAWN);
  base::UnguessableToken instance_id = base::UnguessableToken::Create();
  auto state = std::make_unique<apps::Instance>(app_id, instance_id, &window);
  auto delta = std::make_unique<apps::Instance>(app_id, instance_id, &window);
  EXPECT_TRUE(apps::InstanceUpdate::Equals(state.get(), delta.get()));
  TestInstanceUpdate(state.get(), delta.get());
}

TEST_F(InstanceUpdateTest, LaunchIdIsUpdated) {
  aura::Window window(nullptr);
  window.Init(ui::LAYER_NOT_DRAWN);
  base::UnguessableToken instance_id = base::UnguessableToken::Create();
  auto state = std::make_unique<apps::Instance>(app_id, instance_id, &window);
  auto delta = std::make_unique<apps::Instance>(app_id, instance_id, &window);
  delta->SetLaunchId("abc");
  EXPECT_FALSE(apps::InstanceUpdate::Equals(state.get(), delta.get()));
}

TEST_F(InstanceUpdateTest, LaunchIdIsNotUpdated) {
  aura::Window window(nullptr);
  window.Init(ui::LAYER_NOT_DRAWN);
  base::UnguessableToken instance_id = base::UnguessableToken::Create();
  auto state = std::make_unique<apps::Instance>(app_id, instance_id, &window);
  state->SetLaunchId("abc");
  auto delta = std::make_unique<apps::Instance>(app_id, instance_id, &window);
  EXPECT_TRUE(apps::InstanceUpdate::Equals(state.get(), delta.get()));
}

TEST_F(InstanceUpdateTest, StateIsUpdated) {
  aura::Window window(nullptr);
  window.Init(ui::LAYER_NOT_DRAWN);
  base::UnguessableToken instance_id = base::UnguessableToken::Create();
  auto state = std::make_unique<apps::Instance>(app_id, instance_id, &window);
  auto delta = std::make_unique<apps::Instance>(app_id, instance_id, &window);
  delta->UpdateState(apps::InstanceState::kStarted, base::Time::Now());
  EXPECT_FALSE(apps::InstanceUpdate::Equals(state.get(), delta.get()));
}

TEST_F(InstanceUpdateTest, StateIsNotUpdated) {
  aura::Window window(nullptr);
  window.Init(ui::LAYER_NOT_DRAWN);
  base::UnguessableToken instance_id = base::UnguessableToken::Create();
  auto state = std::make_unique<apps::Instance>(app_id, instance_id, &window);
  state->UpdateState(apps::InstanceState::kStarted, base::Time::Now());
  auto delta = std::make_unique<apps::Instance>(app_id, instance_id, &window);
  EXPECT_TRUE(apps::InstanceUpdate::Equals(state.get(), delta.get()));
}

TEST_F(InstanceUpdateTest, BothLaunchAndStateIsUpdated) {
  aura::Window window(nullptr);
  window.Init(ui::LAYER_NOT_DRAWN);
  base::UnguessableToken instance_id = base::UnguessableToken::Create();
  auto state = std::make_unique<apps::Instance>(app_id, instance_id, &window);
  state->SetLaunchId("aaa");
  state->UpdateState(apps::InstanceState::kStarted, base::Time::Now());
  auto delta = std::make_unique<apps::Instance>(app_id, instance_id, &window);
  delta->SetLaunchId("bbb");
  delta->UpdateState(apps::InstanceState::kRunning, base::Time::Now());
  EXPECT_FALSE(apps::InstanceUpdate::Equals(state.get(), delta.get()));
}

TEST_F(InstanceUpdateTest, BrowserContextIsUpdated) {
  aura::Window window(nullptr);
  window.Init(ui::LAYER_NOT_DRAWN);
  base::UnguessableToken instance_id = base::UnguessableToken::Create();
  auto state = std::make_unique<apps::Instance>(app_id, instance_id, &window);
  auto delta = std::make_unique<apps::Instance>(app_id, instance_id, &window);
  delta->SetBrowserContext(&profile_);
  EXPECT_FALSE(apps::InstanceUpdate::Equals(state.get(), delta.get()));
}

TEST_F(InstanceUpdateTest, BrowserContextIsNotUpdated) {
  aura::Window window(nullptr);
  window.Init(ui::LAYER_NOT_DRAWN);
  base::UnguessableToken instance_id = base::UnguessableToken::Create();
  auto state = std::make_unique<apps::Instance>(app_id, instance_id, &window);
  state->SetBrowserContext(&profile_);
  auto delta = std::make_unique<apps::Instance>(app_id, instance_id, &window);
  EXPECT_TRUE(apps::InstanceUpdate::Equals(state.get(), delta.get()));
}

TEST_F(InstanceUpdateTest, WindowIsUpdated) {
  aura::Window window1(nullptr);
  window1.Init(ui::LAYER_NOT_DRAWN);
  aura::Window window2(nullptr);
  window2.Init(ui::LAYER_NOT_DRAWN);
  base::UnguessableToken instance_id = base::UnguessableToken::Create();
  auto state = std::make_unique<apps::Instance>(app_id, instance_id, &window1);
  state->SetBrowserContext(&profile_);
  auto delta = std::make_unique<apps::Instance>(app_id, instance_id, &window2);
  EXPECT_FALSE(apps::InstanceUpdate::Equals(state.get(), delta.get()));
  apps::InstanceUpdate::Merge(state.get(), delta.get());
}
