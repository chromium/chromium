// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/gcm_registration_manager_impl.h"

#include <optional>
#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/fcm/engine/checkin_info.h"
#include "components/fcm/engine/chrome_build_info.h"
#include "components/fcm/engine/fcm_stats_recorder_impl.h"
#include "components/gcm_driver/registration_info.h"
#include "google_apis/gcm/base/gcm_features.h"
#include "google_apis/gcm/engine/fake_gcm_store.h"
#include "google_apis/gcm/engine/gservices_settings.h"
#include "google_apis/gcm/engine/registration_request.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gcm {
namespace {

using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::Optional;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

constexpr uint64_t kDeviceAndroidId = 54321;
constexpr uint64_t kDeviceSecurityToken = 12345;
constexpr char kChromeVersion[] = "45.0.0.1";
constexpr char kProductCategoryForSubtypes[] = "com.chrome.macosx";
// Valid 32-character [a-p] Chrome extension ID (passes
// `crx_file::id_util::IdIsValid()`, which skips subtypes for Instance ID).
constexpr char kExtensionAppId[] = "abcdefghijklmnopabcdefghijklmnop";
constexpr char kTestAppId[] = "test_app_id";
constexpr char kTestAppId2[] = "test_app_id_2";
constexpr char kDeprecatedAppId[] = "com.google.android.gms";
constexpr char kSenderId[] = "sender_1";
constexpr char kSenderId2[] = "sender_2";
constexpr char kScope[] = "GCM";
constexpr char kRegistrationId[] = "reg_id_1";
constexpr char kInstanceId[] = "iid_1";
constexpr char kExtraData[] = "extra_1";
constexpr char kRegisterUrl[] =
    "https://android.clients.google.com/c2dm/register3";

fcm::ChromeBuildInfo CreateChromeBuildInfo() {
  fcm::ChromeBuildInfo info;
  info.version = kChromeVersion;
  info.product_category_for_subtypes = kProductCategoryForSubtypes;
  return info;
}

scoped_refptr<GCMRegistrationInfo> CreateGCMRegistrationInfo(
    const std::string& app_id,
    const std::vector<std::string>& sender_ids) {
  auto info = base::MakeRefCounted<GCMRegistrationInfo>();
  info->app_id = app_id;
  info->sender_ids = sender_ids;
  return info;
}

scoped_refptr<InstanceIDTokenInfo> CreateInstanceIDTokenInfo(
    const std::string& app_id,
    const std::string& authorized_entity,
    const std::string& scope) {
  auto info = base::MakeRefCounted<InstanceIDTokenInfo>();
  info->app_id = app_id;
  info->authorized_entity = authorized_entity;
  info->scope = scope;
  return info;
}

class GCMRegistrationManagerImplTest : public testing::Test {
 public:
  GCMRegistrationManagerImplTest() {
    clock_.SetNow(base::Time::Now());
    registration_manager_.SetDeviceCheckinInfo(
        fcm::CheckinInfo(kDeviceAndroidId, kDeviceSecurityToken));
  }

  FakeGCMStore& gcm_store() { return gcm_store_; }
  base::SimpleTestClock& clock() { return clock_; }
  network::TestURLLoaderFactory& url_loader_factory() {
    return test_url_loader_factory_;
  }
  GCMRegistrationManagerImpl& registration_manager() {
    return registration_manager_;
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  FakeGCMStore gcm_store_;
  base::SimpleTestClock clock_;
  fcm::FcmStatsRecorderImpl recorder_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  GCMRegistrationManagerImpl registration_manager_{
      gcm_store_,
      clock_,
      CreateChromeBuildInfo(),
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory_),
      base::SingleThreadTaskRunner::GetCurrentDefault(),
      recorder_};
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

TEST_F(GCMRegistrationManagerImplTest, OnRegisterCompletedSuccess) {
  scoped_refptr<GCMRegistrationInfo> gcm_info =
      CreateGCMRegistrationInfo(kTestAppId, {kSenderId});
  registration_manager().AddPendingRegistrationRequest(gcm_info,
                                                       /*request=*/nullptr);

  base::test::TestFuture<const std::string&, GCMClient::Result> future;
  registration_manager().OnRegisterCompleted(gcm_info, future.GetCallback(),
                                             RegistrationRequest::SUCCESS,
                                             kRegistrationId);

  const auto& [registration_id, result] = future.Get();
  EXPECT_EQ(registration_id, kRegistrationId);
  EXPECT_EQ(result, GCMClient::SUCCESS);
  EXPECT_TRUE(
      registration_manager().ValidateRegistration(gcm_info, kRegistrationId));
  EXPECT_THAT(registration_manager().GetCachedRegistrationId(gcm_info),
              Optional(std::string(kRegistrationId)));
  EXPECT_THAT(gcm_store().registrations(),
              UnorderedElementsAre(
                  Pair(gcm_info->GetSerializedKey(),
                       gcm_info->GetSerializedValue(kRegistrationId))));
}

TEST_F(GCMRegistrationManagerImplTest, OnRegisterCompletedReplacesSenderIds) {
  scoped_refptr<GCMRegistrationInfo> initial_info =
      CreateGCMRegistrationInfo(kTestAppId, {kSenderId});
  registration_manager().AddPendingRegistrationRequest(initial_info,
                                                       /*request=*/nullptr);
  base::test::TestFuture<const std::string&, GCMClient::Result> initial_future;
  registration_manager().OnRegisterCompleted(
      initial_info, initial_future.GetCallback(), RegistrationRequest::SUCCESS,
      kRegistrationId);
  const auto& [initial_id, initial_result] = initial_future.Get();
  ASSERT_EQ(initial_result, GCMClient::SUCCESS);

  // Re-registering the same `app_id` with a new sender list must replace the
  // cached map key so `sender_ids` is updated.
  scoped_refptr<GCMRegistrationInfo> updated_info =
      CreateGCMRegistrationInfo(kTestAppId, {kSenderId2});
  registration_manager().AddPendingRegistrationRequest(updated_info,
                                                       /*request=*/nullptr);
  base::test::TestFuture<const std::string&, GCMClient::Result> updated_future;
  registration_manager().OnRegisterCompleted(
      updated_info, updated_future.GetCallback(), RegistrationRequest::SUCCESS,
      "reg_id_2");

  const auto& [registration_id, result] = updated_future.Get();
  EXPECT_EQ(registration_id, "reg_id_2");
  EXPECT_EQ(result, GCMClient::SUCCESS);
  EXPECT_FALSE(registration_manager().ValidateRegistration(initial_info,
                                                           kRegistrationId));
  EXPECT_EQ(std::nullopt,
            registration_manager().GetCachedRegistrationId(initial_info));
  EXPECT_TRUE(
      registration_manager().ValidateRegistration(updated_info, "reg_id_2"));
  EXPECT_THAT(registration_manager().GetCachedRegistrationId(updated_info),
              Optional(std::string("reg_id_2")));
}

TEST_F(GCMRegistrationManagerImplTest, OnRegisterCompletedInvalidSender) {
  scoped_refptr<GCMRegistrationInfo> gcm_info =
      CreateGCMRegistrationInfo(kTestAppId, {kSenderId});
  registration_manager().AddPendingRegistrationRequest(gcm_info,
                                                       /*request=*/nullptr);

  base::test::TestFuture<const std::string&, GCMClient::Result> future;
  registration_manager().OnRegisterCompleted(
      gcm_info, future.GetCallback(), RegistrationRequest::INVALID_SENDER,
      std::string());

  const auto& [registration_id, result] = future.Get();
  EXPECT_THAT(registration_id, IsEmpty());
  EXPECT_EQ(result, GCMClient::INVALID_PARAMETER);
  EXPECT_THAT(registration_manager().GetRegisteredAppIds(), IsEmpty());
  EXPECT_THAT(gcm_store().registrations(), IsEmpty());
}

TEST_F(GCMRegistrationManagerImplTest, OnRegisterCompletedServerError) {
  scoped_refptr<GCMRegistrationInfo> gcm_info =
      CreateGCMRegistrationInfo(kTestAppId, {kSenderId});
  registration_manager().AddPendingRegistrationRequest(gcm_info,
                                                       /*request=*/nullptr);

  base::test::TestFuture<const std::string&, GCMClient::Result> future;
  registration_manager().OnRegisterCompleted(
      gcm_info, future.GetCallback(),
      RegistrationRequest::INTERNAL_SERVER_ERROR, std::string());

  const auto& [registration_id, result] = future.Get();
  EXPECT_THAT(registration_id, IsEmpty());
  EXPECT_EQ(result, GCMClient::SERVER_ERROR);
  EXPECT_THAT(registration_manager().GetRegisteredAppIds(), IsEmpty());
  EXPECT_THAT(gcm_store().registrations(), IsEmpty());
}

TEST_F(GCMRegistrationManagerImplTest, CancelPendingRequests) {
  scoped_refptr<GCMRegistrationInfo> gcm_info =
      CreateGCMRegistrationInfo(kTestAppId, {kSenderId});
  registration_manager().AddPendingRegistrationRequest(gcm_info,
                                                       /*request=*/nullptr);

  base::test::TestFuture<const std::string&, GCMClient::Result> future;
  RegistrationRequest::RegistrationCallback request_callback =
      base::BindOnce(&GCMRegistrationManagerImpl::OnRegisterCompleted,
                     registration_manager().GetPendingRequestWeakPtr(),
                     gcm_info, future.GetCallback());

  registration_manager().CancelPendingRequests();
  std::move(request_callback)
      .Run(RegistrationRequest::SUCCESS, kRegistrationId);

  EXPECT_FALSE(future.IsReady());
  EXPECT_THAT(registration_manager().GetRegisteredAppIds(), IsEmpty());
  EXPECT_THAT(gcm_store().registrations(), IsEmpty());
}

TEST_F(GCMRegistrationManagerImplTest, OnRegisterCompletedUntrackedRequest) {
  scoped_refptr<GCMRegistrationInfo> gcm_info =
      CreateGCMRegistrationInfo(kTestAppId, {kSenderId});

  base::test::TestFuture<const std::string&, GCMClient::Result> future;
  registration_manager().OnRegisterCompleted(gcm_info, future.GetCallback(),
                                             RegistrationRequest::SUCCESS,
                                             kRegistrationId);

  const auto& [registration_id, result] = future.Get();
  EXPECT_THAT(registration_id, IsEmpty());
  EXPECT_EQ(result, GCMClient::UNKNOWN_ERROR);
  EXPECT_THAT(registration_manager().GetRegisteredAppIds(), IsEmpty());
  EXPECT_THAT(gcm_store().registrations(), IsEmpty());
}

TEST_F(GCMRegistrationManagerImplTest, GetToken) {
  registration_manager().AddInstanceIDData(kTestAppId, kInstanceId, kExtraData);
  scoped_refptr<InstanceIDTokenInfo> token_info =
      CreateInstanceIDTokenInfo(kTestAppId, kSenderId, kScope);

  base::test::TestFuture<const std::string&, GCMClient::Result> future;
  registration_manager().GetToken(token_info, future.GetCallback());
  ASSERT_EQ(1, url_loader_factory().NumPending());
  ASSERT_TRUE(url_loader_factory().SimulateResponseForPendingRequest(
      kRegisterUrl, "token=token_1"));

  const auto& [token, result] = future.Get();
  EXPECT_EQ(token, "token_1");
  EXPECT_EQ(result, GCMClient::SUCCESS);
  EXPECT_TRUE(
      registration_manager().ValidateRegistration(token_info, "token_1"));
  EXPECT_THAT(
      gcm_store().registrations(),
      UnorderedElementsAre(Pair(token_info->GetSerializedKey(),
                                token_info->GetSerializedValue("token_1"))));
}

TEST_F(GCMRegistrationManagerImplTest, GetTokenFromCache) {
  registration_manager().AddInstanceIDData(kTestAppId, kInstanceId, kExtraData);
  scoped_refptr<InstanceIDTokenInfo> token_info =
      CreateInstanceIDTokenInfo(kTestAppId, kSenderId, kScope);

  base::test::TestFuture<const std::string&, GCMClient::Result> future;
  registration_manager().GetToken(token_info, future.GetCallback());
  ASSERT_TRUE(url_loader_factory().SimulateResponseForPendingRequest(
      kRegisterUrl, "token=token_1"));
  const auto& [initial_token, initial_result] = future.Get();
  ASSERT_EQ(initial_result, GCMClient::SUCCESS);

  // Subsequent `GetToken()` while fresh returns the cached token without a
  // network request.
  base::test::TestFuture<const std::string&, GCMClient::Result> cached_future;
  registration_manager().GetToken(token_info, cached_future.GetCallback());
  EXPECT_EQ(0, url_loader_factory().NumPending());
  const auto& [cached_token, cached_result] = cached_future.Get();
  EXPECT_EQ(cached_token, "token_1");
  EXPECT_EQ(cached_result, GCMClient::SUCCESS);
}

TEST_F(GCMRegistrationManagerImplTest, GetTokenUsesSubtypeForNonExtensionApp) {
  registration_manager().AddInstanceIDData(kTestAppId, kInstanceId, kExtraData);
  scoped_refptr<InstanceIDTokenInfo> token_info =
      CreateInstanceIDTokenInfo(kTestAppId, kSenderId, kScope);

  base::test::TestFuture<const std::string&, GCMClient::Result> future;
  registration_manager().GetToken(token_info, future.GetCallback());
  ASSERT_EQ(1, url_loader_factory().NumPending());
  std::string upload_body = network::GetUploadData(
      url_loader_factory().GetPendingRequest(0)->request);
  EXPECT_THAT(upload_body,
              HasSubstr(base::StrCat({"app=", kProductCategoryForSubtypes})));
  EXPECT_THAT(upload_body, HasSubstr(base::StrCat({"X-subtype=", kTestAppId})));
  ASSERT_TRUE(url_loader_factory().SimulateResponseForPendingRequest(
      kRegisterUrl, "token=token_subtype"));

  const auto& [token, result] = future.Get();
  EXPECT_EQ(token, "token_subtype");
  EXPECT_EQ(result, GCMClient::SUCCESS);
  EXPECT_TRUE(
      registration_manager().ValidateRegistration(token_info, "token_subtype"));
}

TEST_F(GCMRegistrationManagerImplTest, GetTokenOmitsSubtypeForExtensionApp) {
  registration_manager().AddInstanceIDData(kExtensionAppId, kInstanceId,
                                           kExtraData);
  scoped_refptr<InstanceIDTokenInfo> token_info =
      CreateInstanceIDTokenInfo(kExtensionAppId, kSenderId, kScope);

  base::test::TestFuture<const std::string&, GCMClient::Result> future;
  registration_manager().GetToken(token_info, future.GetCallback());
  ASSERT_EQ(1, url_loader_factory().NumPending());
  std::string upload_body = network::GetUploadData(
      url_loader_factory().GetPendingRequest(0)->request);
  EXPECT_THAT(upload_body, HasSubstr(base::StrCat({"app=", kExtensionAppId})));
  EXPECT_THAT(upload_body, Not(HasSubstr("X-subtype=")));
  ASSERT_TRUE(url_loader_factory().SimulateResponseForPendingRequest(
      kRegisterUrl, "token=token_ext"));

  const auto& [token, result] = future.Get();
  EXPECT_EQ(token, "token_ext");
  EXPECT_EQ(result, GCMClient::SUCCESS);
  EXPECT_TRUE(
      registration_manager().ValidateRegistration(token_info, "token_ext"));
}

TEST_F(GCMRegistrationManagerImplTest,
       GetTokenUsesRegistrationUrlFromGServicesSettings) {
  constexpr char kCustomRegisterUrl[] = "http://alternative.url/registration";
  GCMStore::LoadResult load_result;
  load_result.gservices_settings["gcm_registration_url"] = kCustomRegisterUrl;
  load_result.gservices_digest =
      GServicesSettings::CalculateDigest(load_result.gservices_settings);
  GServicesSettings gservices_settings;
  gservices_settings.UpdateFromLoadResult(load_result);
  registration_manager().SetGServicesSettings(gservices_settings);

  registration_manager().AddInstanceIDData(kTestAppId, kInstanceId, kExtraData);
  scoped_refptr<InstanceIDTokenInfo> token_info =
      CreateInstanceIDTokenInfo(kTestAppId, kSenderId, kScope);

  base::test::TestFuture<const std::string&, GCMClient::Result> future;
  registration_manager().GetToken(token_info, future.GetCallback());
  ASSERT_EQ(1, url_loader_factory().NumPending());
  ASSERT_TRUE(url_loader_factory().SimulateResponseForPendingRequest(
      kCustomRegisterUrl, "token=token_custom"));

  const auto& [token, result] = future.Get();
  EXPECT_EQ(token, "token_custom");
  EXPECT_EQ(result, GCMClient::SUCCESS);
}

}  // namespace
}  // namespace gcm
