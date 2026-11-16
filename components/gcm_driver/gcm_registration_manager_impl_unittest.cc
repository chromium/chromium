// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/gcm_registration_manager_impl.h"

#include <string>

#include "google_apis/gcm/engine/fake_gcm_store.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gcm {
namespace {

using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

constexpr char kTestAppId[] = "test_app_id";
constexpr char kTestAppId2[] = "test_app_id_2";
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

TEST_F(GCMRegistrationManagerImplTest, AddAndGetInstanceIDData) {
  registration_manager().AddInstanceIDData(kTestAppId, kInstanceId, kExtraData);

  EXPECT_THAT(registration_manager().GetInstanceIDData(kTestAppId),
              Pair(kInstanceId, kExtraData));
  EXPECT_THAT(gcm_store().instance_id_data(),
              UnorderedElementsAre(Pair(kTestAppId, "iid_1,extra_1")));
}

TEST_F(GCMRegistrationManagerImplTest, RemoveInstanceIDData) {
  registration_manager().AddInstanceIDData(kTestAppId, kInstanceId, kExtraData);
  registration_manager().AddInstanceIDData(kTestAppId2, "iid_2", "extra_2");

  ASSERT_THAT(registration_manager().GetInstanceIDData(kTestAppId),
              Pair(kInstanceId, kExtraData));

  registration_manager().RemoveInstanceIDData(kTestAppId);
  registration_manager().RemoveInstanceIDData("non_existent_app");

  EXPECT_THAT(registration_manager().GetInstanceIDData(kTestAppId),
              Pair(IsEmpty(), IsEmpty()));
  EXPECT_THAT(registration_manager().GetInstanceIDData(kTestAppId2),
              Pair("iid_2", "extra_2"));
  EXPECT_THAT(gcm_store().instance_id_data(),
              UnorderedElementsAre(Pair(kTestAppId2, "iid_2,extra_2")));
}

TEST_F(GCMRegistrationManagerImplTest, OnRegistrationDataLoaded) {
  registration_manager().AddInstanceIDData(kTestAppId, "old_iid", "old_extra");

  GCMStore::LoadResult load_result;
  load_result.instance_id_data[kTestAppId] = "iid_1,extra,with,comma";
  load_result.instance_id_data[kTestAppId2] = "iid_2,extra_2";
  // Malformed entries without a comma or with empty fields should be ignored.
  load_result.instance_id_data["malformed"] = "malformed_without_comma";
  load_result.instance_id_data["empty_iid"] = ",extra";
  load_result.instance_id_data["empty_extra"] = "iid,";
  load_result.instance_id_data["both_empty"] = ",";
  load_result.instance_id_data["empty_string"] = "";

  registration_manager().OnRegistrationDataLoaded(load_result);

  EXPECT_THAT(registration_manager().GetInstanceIDData(kTestAppId),
              Pair(kInstanceId, "extra,with,comma"));
  EXPECT_THAT(registration_manager().GetInstanceIDData(kTestAppId2),
              Pair("iid_2", "extra_2"));
  EXPECT_THAT(registration_manager().GetInstanceIDData("malformed"),
              Pair(IsEmpty(), IsEmpty()));
  EXPECT_THAT(registration_manager().GetInstanceIDData("empty_iid"),
              Pair(IsEmpty(), IsEmpty()));
  EXPECT_THAT(registration_manager().GetInstanceIDData("empty_extra"),
              Pair(IsEmpty(), IsEmpty()));
  EXPECT_THAT(registration_manager().GetInstanceIDData("both_empty"),
              Pair(IsEmpty(), IsEmpty()));
  EXPECT_THAT(registration_manager().GetInstanceIDData("empty_string"),
              Pair(IsEmpty(), IsEmpty()));
}

}  // namespace
}  // namespace gcm
