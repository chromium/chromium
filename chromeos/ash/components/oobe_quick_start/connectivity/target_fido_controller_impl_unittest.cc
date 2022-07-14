// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/oobe_quick_start/connectivity/target_fido_controller_impl.h"

#include "chromeos/ash/components/oobe_quick_start/connectivity/target_fido_controller_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
using TargetFidoController = ash::quick_start::TargetFidoController;
using TargetFidoControllerImpl = ash::quick_start::TargetFidoControllerImpl;
using NearbyConnectionsManager = ash::quick_start::NearbyConnectionsManager;

const std::string kChallengeBytes = "testchallenge";
}  // namespace

class TargetFidoControllerImplTest : public testing::Test {
 public:
  TargetFidoControllerImplTest() = default;
  TargetFidoControllerImplTest(TargetFidoControllerImplTest&) = delete;
  TargetFidoControllerImplTest& operator=(TargetFidoControllerImplTest&) =
      delete;
  ~TargetFidoControllerImplTest() override = default;

  // TODO(b/234655072): Pass in FakeNearbyConnectionsManager when available.
  void SetUp() override { CreateFidoController(nullptr); }

  void CreateFidoController(
      const NearbyConnectionsManager* nearby_connections_manager) {
    fido_controller_ = ash::quick_start::TargetFidoControllerFactory::Create(
        nearby_connections_manager);
  }

  void OnRequestAssertion(bool success) {
    request_assertion_callback_called_ = true;
    request_assertion_success_ = success;
  }

 protected:
  std::unique_ptr<TargetFidoController> fido_controller_;
  bool request_assertion_callback_called_ = false;
  bool request_assertion_success_ = false;
  base::WeakPtrFactory<TargetFidoControllerImplTest> weak_ptr_factory_{this};
};

TEST_F(TargetFidoControllerImplTest,
       StartGetAssertionFlow_NoNearbyConnectionsManager) {
  fido_controller_->RequestAssertion(
      kChallengeBytes,
      base::BindOnce(&TargetFidoControllerImplTest::OnRequestAssertion,
                     weak_ptr_factory_.GetWeakPtr()));
  EXPECT_TRUE(request_assertion_callback_called_);
  EXPECT_TRUE(request_assertion_success_);
}