// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/gcm_registration_manager_impl.h"

#include <optional>
#include <string>

#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "components/gcm_driver/registration_info.h"
#include "google_apis/gcm/base/gcm_features.h"
#include "google_apis/gcm/engine/fake_gcm_store.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gcm {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Optional;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

constexpr char kTestAppId[] = "test_app_id";
constexpr char kTestAppId2[] = "test_app_id_2";
constexpr char kDeprecatedAppId[] = "com.google.android.gms";
constexpr char kSenderId[] = "sender_1";
constexpr char kSenderId2[] = "sender_2";
constexpr char kRegistrationId[] = "reg_id_1";
constexpr char kInstanceId[] = "iid_1";
constexpr char kExtraData[] = "extra_1";

scoped_refptr<GCMRegistrationInfo> CreateGCMRegistrationInfo(
    const std::string& app_id,
    const std::vector<std::string>& sender_ids) {
  auto info = base::MakeRefCounted<GCMRegistrationInfo>();
  info->app_id = app_id;
  info->sender_ids = sender_ids;
  return info;
}

class GCMRegistrationManagerImplTest : public testing::Test {
 public:
  GCMRegistrationManagerImplTest() { clock_.SetNow(base::Time::Now()); }

  FakeGCMStore& gcm_store() { return gcm_store_; }
  base::SimpleTestClock& clock() { return clock_; }
  GCMRegistrationManagerImpl& registration_manager() {
    return registration_manager_;
  }

 private:
  FakeGCMStore gcm_store_;
  base::SimpleTestClock clock_;
  GCMRegistrationManagerImpl registration_manager_{gcm_store_, clock_};
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

  scoped_refptr<GCMRegistrationInfo> gcm_info =
      CreateGCMRegistrationInfo(kTestAppId, {kSenderId});
  GCMStore::LoadResult load_result;
  load_result.registrations[gcm_info->GetSerializedKey()] =
      gcm_info->GetSerializedValue(kRegistrationId);
  load_result.instance_id_data[kTestAppId] = "iid_1,extra,with,comma";
  load_result.instance_id_data[kTestAppId2] = "iid_2,extra_2";
  // Malformed entries without a comma or with empty fields should be ignored.
  load_result.instance_id_data["malformed"] = "malformed_without_comma";
  load_result.instance_id_data["empty_iid"] = ",extra";
  load_result.instance_id_data["empty_extra"] = "iid,";
  load_result.instance_id_data["both_empty"] = ",";
  load_result.instance_id_data["empty_string"] = "";

  ASSERT_FALSE(registration_manager().HasStandaloneRegisteredApp());
  ASSERT_THAT(registration_manager().GetRegisteredAppIds(), IsEmpty());

  registration_manager().OnRegistrationDataLoaded(load_result);

  EXPECT_TRUE(registration_manager().HasStandaloneRegisteredApp());
  EXPECT_THAT(registration_manager().GetRegisteredAppIds(),
              ElementsAre(kTestAppId));

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

TEST_F(GCMRegistrationManagerImplTest, ValidateRegistration) {
  scoped_refptr<GCMRegistrationInfo> gcm_info =
      CreateGCMRegistrationInfo(kTestAppId, {kSenderId});
  GCMStore::LoadResult load_result;
  load_result.registrations[gcm_info->GetSerializedKey()] =
      gcm_info->GetSerializedValue(kRegistrationId);
  registration_manager().OnRegistrationDataLoaded(load_result);

  EXPECT_TRUE(
      registration_manager().ValidateRegistration(gcm_info, kRegistrationId));
  EXPECT_FALSE(registration_manager().ValidateRegistration(gcm_info, "wrong"));
  EXPECT_FALSE(registration_manager().ValidateRegistration(
      CreateGCMRegistrationInfo(kTestAppId, {kSenderId2}), kRegistrationId));
  EXPECT_FALSE(registration_manager().ValidateRegistration(
      CreateGCMRegistrationInfo(kTestAppId2, {kSenderId}), kRegistrationId));
}

TEST_F(GCMRegistrationManagerImplTest,
       OnRegistrationDataLoadedRemovesDeprecatedData) {
  base::test::ScopedFeatureList feature_list(
      features::kGCMRemoveDeprecatedStoreData);
  scoped_refptr<GCMRegistrationInfo> deprecated_info =
      CreateGCMRegistrationInfo(kDeprecatedAppId, {kSenderId});
  gcm_store().AddRegistration(
      deprecated_info->GetSerializedKey(),
      deprecated_info->GetSerializedValue(kRegistrationId), base::DoNothing());

  GCMStore::LoadResult load_result;
  load_result.registrations = gcm_store().registrations();
  registration_manager().OnRegistrationDataLoaded(load_result);

  EXPECT_FALSE(registration_manager().HasStandaloneRegisteredApp());
  EXPECT_THAT(registration_manager().GetRegisteredAppIds(), IsEmpty());
  EXPECT_THAT(gcm_store().registrations(), IsEmpty());
}

TEST_F(GCMRegistrationManagerImplTest,
       OnRegistrationDataLoadedRetainsDeprecatedDataWhenFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kGCMRemoveDeprecatedStoreData);
  scoped_refptr<GCMRegistrationInfo> deprecated_info =
      CreateGCMRegistrationInfo(kDeprecatedAppId, {kSenderId});
  gcm_store().AddRegistration(
      deprecated_info->GetSerializedKey(),
      deprecated_info->GetSerializedValue(kRegistrationId), base::DoNothing());

  GCMStore::LoadResult load_result;
  load_result.registrations = gcm_store().registrations();
  registration_manager().OnRegistrationDataLoaded(load_result);

  EXPECT_TRUE(registration_manager().HasStandaloneRegisteredApp());
  EXPECT_THAT(registration_manager().GetRegisteredAppIds(),
              ElementsAre(kDeprecatedAppId));
  EXPECT_THAT(gcm_store().registrations(),
              UnorderedElementsAre(
                  Pair(deprecated_info->GetSerializedKey(),
                       deprecated_info->GetSerializedValue(kRegistrationId))));
}

TEST_F(GCMRegistrationManagerImplTest, GetCachedRegistrationId) {
  scoped_refptr<GCMRegistrationInfo> gcm_info =
      CreateGCMRegistrationInfo(kTestAppId, {kSenderId});
  gcm_info->last_validated = clock().Now();

  EXPECT_EQ(std::nullopt,
            registration_manager().GetCachedRegistrationId(gcm_info));

  GCMStore::LoadResult load_result;
  load_result.registrations[gcm_info->GetSerializedKey()] =
      gcm_info->GetSerializedValue(kRegistrationId);
  registration_manager().OnRegistrationDataLoaded(load_result);

  clock().Advance(base::Days(1));
  EXPECT_THAT(registration_manager().GetCachedRegistrationId(gcm_info),
              Optional(std::string(kRegistrationId)));

  // Mismatched sender IDs should not return the cached registration ID.
  EXPECT_EQ(std::nullopt,
            registration_manager().GetCachedRegistrationId(
                CreateGCMRegistrationInfo(kTestAppId, {kSenderId2})));

  // Stale registration (>= 7 days old) should return nullopt.
  clock().Advance(base::Days(7));
  EXPECT_EQ(std::nullopt,
            registration_manager().GetCachedRegistrationId(gcm_info));
}

}  // namespace
}  // namespace gcm
