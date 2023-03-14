// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/attestation/attestation_features.h"

#include "base/logging.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chromeos/ash/components/dbus/attestation/attestation_ca.pb.h"
#include "chromeos/ash/components/dbus/attestation/attestation_client.h"
#include "chromeos/ash/components/dbus/attestation/interface.pb.h"
#include "chromeos/ash/components/dbus/constants/attestation_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::AtLeast;
using testing::DoDefault;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;
using testing::Sequence;
using testing::StrictMock;
using testing::WithArgs;

namespace ash::attestation {

class AttestationFeaturesTest : public testing::Test {
 public:
  AttestationFeaturesTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    AttestationClient::InitializeFake();
  }
  ~AttestationFeaturesTest() override { AttestationClient::Shutdown(); }

  const AttestationFeatures* GetFeatures() {
    base::test::TestFuture<const AttestationFeatures*> future;
    AttestationFeatures::GetFeatures(future.GetCallback());
    return future.Get();
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(AttestationFeaturesTest, GetFeaturesSuccess) {
  ::attestation::GetFeaturesReply* reply =
      AttestationClient::Get()->GetTestInterface()->mutable_features_reply();
  reply->set_is_available(true);
  reply->add_supported_key_types(::attestation::KEY_TYPE_RSA);
  reply->add_supported_key_types(::attestation::KEY_TYPE_ECC);

  AttestationFeatures::Initialize();

  const AttestationFeatures* features = GetFeatures();
  ASSERT_NE(features, nullptr);
  EXPECT_EQ(features->IsAttestationAvailable(), true);
  EXPECT_EQ(features->IsEccSupported(), true);
  EXPECT_EQ(features->IsRsaSupported(), true);

  AttestationFeatures::Shutdown();
}

TEST_F(AttestationFeaturesTest, GetFeaturesNoSupportedKey) {
  ::attestation::GetFeaturesReply* reply =
      AttestationClient::Get()->GetTestInterface()->mutable_features_reply();
  reply->set_is_available(true);

  AttestationFeatures::Initialize();

  const AttestationFeatures* features = GetFeatures();
  ASSERT_NE(features, nullptr);
  EXPECT_EQ(features->IsAttestationAvailable(), true);
  EXPECT_EQ(features->IsEccSupported(), false);
  EXPECT_EQ(features->IsRsaSupported(), false);

  AttestationFeatures::Shutdown();
}

TEST_F(AttestationFeaturesTest, GetFeaturesAttestationNoAvailable) {
  ::attestation::GetFeaturesReply* reply =
      AttestationClient::Get()->GetTestInterface()->mutable_features_reply();
  reply->set_is_available(false);

  AttestationFeatures::Initialize();

  const AttestationFeatures* features = GetFeatures();
  ASSERT_NE(features, nullptr);
  EXPECT_EQ(features->IsAttestationAvailable(), false);
  EXPECT_EQ(features->IsEccSupported(), false);
  EXPECT_EQ(features->IsRsaSupported(), false);

  AttestationFeatures::Shutdown();
}

TEST_F(AttestationFeaturesTest, InitializeGetFeaturesFailure) {
  ::attestation::GetFeaturesReply* reply =
      AttestationClient::Get()->GetTestInterface()->mutable_features_reply();
  reply->set_status(::attestation::STATUS_UNEXPECTED_DEVICE_ERROR);

  AttestationFeatures::Initialize();

  const AttestationFeatures* features = GetFeatures();
  EXPECT_EQ(features, nullptr);

  AttestationFeatures::Shutdown();
}

}  // namespace ash::attestation
