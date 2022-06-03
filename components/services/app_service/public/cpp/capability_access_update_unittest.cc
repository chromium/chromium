// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/capability_access_update.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char app_id[] = "abcdefgh";
}  // namespace

class CapabilityAccessUpdateTest : public testing::Test {
 protected:
  apps::mojom::OptionalBool expect_camera_;
  bool expect_camera_changed_;

  apps::mojom::OptionalBool expect_microphone_;
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

  void TestCapabilityAccessUpdate(apps::mojom::CapabilityAccess* state,
                                  apps::mojom::CapabilityAccess* delta) {
    apps::CapabilityAccessUpdate u(state, delta, account_id_);

    EXPECT_EQ(app_id, u.AppId());
    EXPECT_EQ(state == nullptr, u.StateIsNull());

    expect_camera_ = apps::mojom::OptionalBool::kUnknown;
    expect_microphone_ = apps::mojom::OptionalBool::kUnknown;

    ExpectNoChange();
    CheckExpects(u);

    // IsAccessingCamera tests.
    if (state) {
      state->camera = apps::mojom::OptionalBool::kFalse;
      expect_camera_ = apps::mojom::OptionalBool::kFalse;
      expect_camera_changed_ = false;
      CheckExpects(u);
    }

    if (delta) {
      delta->camera = apps::mojom::OptionalBool::kTrue;
      expect_camera_ = apps::mojom::OptionalBool::kTrue;
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
      state->microphone = apps::mojom::OptionalBool::kFalse;
      expect_microphone_ = apps::mojom::OptionalBool::kFalse;
      expect_microphone_changed_ = false;
      CheckExpects(u);
    }

    if (delta) {
      delta->microphone = apps::mojom::OptionalBool::kTrue;
      expect_microphone_ = apps::mojom::OptionalBool::kTrue;
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
  apps::mojom::CapabilityAccessPtr state = apps::mojom::CapabilityAccess::New();
  state->app_id = app_id;

  TestCapabilityAccessUpdate(state.get(), nullptr);
}

TEST_F(CapabilityAccessUpdateTest, DeltaIsNonNull) {
  apps::mojom::CapabilityAccessPtr delta = apps::mojom::CapabilityAccess::New();
  delta->app_id = app_id;

  TestCapabilityAccessUpdate(nullptr, delta.get());
}

TEST_F(CapabilityAccessUpdateTest, BothAreNonNull) {
  apps::mojom::CapabilityAccessPtr state = apps::mojom::CapabilityAccess::New();
  state->app_id = app_id;

  apps::mojom::CapabilityAccessPtr delta = apps::mojom::CapabilityAccess::New();
  delta->app_id = app_id;

  TestCapabilityAccessUpdate(state.get(), delta.get());
}
