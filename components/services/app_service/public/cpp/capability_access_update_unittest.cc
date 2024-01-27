// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/capability_access_update.h"

#include <optional>

#include "components/services/app_service/public/cpp/capability_access.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char app_id[] = "abcdefgh";
}  // namespace

class CapabilityAccessUpdateTest : public testing::Test {
 protected:
  std::optional<bool> expect_camera_;
  bool expect_camera_changed_;

  std::optional<bool> expect_microphone_;
  bool expect_microphone_changed_;

  AccountId account_id_ = AccountId::FromUserEmail("test@gmail.com");

  void ExpectNoChange() {
    expect_camera_changed_ = false;
    expect_microphone_changed_ = false;
  }

  void CheckExpects(const apps::CapabilityAccessUpdate& u) {
    EXPECT_EQ(expect_camera_, u.Camera());
    EXPECT_EQ(expect_camera_changed_, u.CameraChanged());

    EXPECT_EQ(expect_microphone_, u.Microphone());
    EXPECT_EQ(expect_microphone_changed_, u.MicrophoneChanged());

    EXPECT_EQ(account_id_, u.AccountId());
  }

  void TestCapabilityAccessUpdate(apps::CapabilityAccess* state,
                                  apps::CapabilityAccess* delta) {
    apps::CapabilityAccessUpdate u(state, delta, account_id_);

    EXPECT_EQ(app_id, u.AppId());
    EXPECT_EQ(state == nullptr, u.StateIsNull());

    ExpectNoChange();
    CheckExpects(u);

    // IsAccessingCamera tests.
    if (state) {
      state->camera = false;
      expect_camera_ = false;
      expect_camera_changed_ = false;
      CheckExpects(u);
    }

    if (delta) {
      delta->camera = true;
      expect_camera_ = true;
      expect_camera_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::CapabilityAccessUpdate::Merge(state, delta);
      ExpectNoChange();
      CheckExpects(u);
    }

    // IsAccessingMicrophone tests.
    if (state) {
      state->microphone = false;
      expect_microphone_ = false;
      expect_microphone_changed_ = false;
      CheckExpects(u);
    }

    if (delta) {
      delta->microphone = true;
      expect_microphone_ = true;
      expect_microphone_changed_ = true;
      CheckExpects(u);
    }

    if (state) {
      apps::CapabilityAccessUpdate::Merge(state, delta);
      ExpectNoChange();
      CheckExpects(u);
    }
  }
};

TEST_F(CapabilityAccessUpdateTest, StateIsNonNull) {
  apps::CapabilityAccessPtr state =
      std::make_unique<apps::CapabilityAccess>(app_id);
  TestCapabilityAccessUpdate(state.get(), nullptr);
}

TEST_F(CapabilityAccessUpdateTest, DeltaIsNonNull) {
  apps::CapabilityAccessPtr delta =
      std::make_unique<apps::CapabilityAccess>(app_id);
  TestCapabilityAccessUpdate(nullptr, delta.get());
}

TEST_F(CapabilityAccessUpdateTest, BothAreNonNull) {
  apps::CapabilityAccessPtr state =
      std::make_unique<apps::CapabilityAccess>(app_id);
  apps::CapabilityAccessPtr delta =
      std::make_unique<apps::CapabilityAccess>(app_id);
  TestCapabilityAccessUpdate(state.get(), delta.get());
}

TEST_F(CapabilityAccessUpdateTest, IsAccessingAnyCapability_Empty) {
  apps::CapabilityAccess state(app_id);
  apps::CapabilityAccessUpdate update(&state, nullptr, account_id_);

  ASSERT_FALSE(update.IsAccessingAnyCapability());
}

TEST_F(CapabilityAccessUpdateTest,
       IsAccessingAnyCapability_StateAccessingMicrophone) {
  apps::CapabilityAccess state(app_id);
  state.microphone = true;
  apps::CapabilityAccessUpdate update(&state, nullptr, account_id_);

  ASSERT_TRUE(update.IsAccessingAnyCapability());
}

TEST_F(CapabilityAccessUpdateTest,
       IsAccessingAnyCapability_StateAccessingCamera) {
  apps::CapabilityAccess state(app_id);
  state.camera = true;
  apps::CapabilityAccessUpdate update(&state, nullptr, account_id_);

  ASSERT_TRUE(update.IsAccessingAnyCapability());
}

TEST_F(CapabilityAccessUpdateTest, IsAccessingAnyCapability_DeltaNoMicrophone) {
  apps::CapabilityAccess state(app_id);
  state.microphone = true;
  apps::CapabilityAccess delta(app_id);
  delta.microphone = false;

  apps::CapabilityAccessUpdate update(&state, &delta, account_id_);

  ASSERT_FALSE(update.IsAccessingAnyCapability());
}
