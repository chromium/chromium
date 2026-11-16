// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/gcm_registration_manager_impl.h"

#include "google_apis/gcm/engine/fake_gcm_store.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gcm {
namespace {

using ::testing::Pair;
using ::testing::UnorderedElementsAre;

constexpr char kTestAppId[] = "test_app_id";
constexpr char kInstanceId[] = "iid_1";
constexpr char kExtraData[] = "extra_1";

class GCMRegistrationManagerImplTest : public testing::Test {
 public:
  FakeGCMStore& gcm_store() { return gcm_store_; }
  GCMRegistrationManagerImpl& registration_manager() {
    return registration_manager_;
  }

 private:
  FakeGCMStore gcm_store_;
  GCMRegistrationManagerImpl registration_manager_{gcm_store_};
};

TEST_F(GCMRegistrationManagerImplTest, AddInstanceIDData) {
  registration_manager().AddInstanceIDData(kTestAppId, kInstanceId, kExtraData);

  EXPECT_THAT(gcm_store().instance_id_data(),
              UnorderedElementsAre(Pair(kTestAppId, "iid_1,extra_1")));
}

}  // namespace
}  // namespace gcm
