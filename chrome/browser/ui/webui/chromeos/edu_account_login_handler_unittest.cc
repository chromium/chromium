// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/edu_account_login_handler_chromeos.h"

#include <memory>

#include "base/callback_helpers.h"
#include "base/json/json_writer.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chrome/browser/chromeos/net/network_portal_detector_test_impl.h"
#include "chromeos/dbus/shill/shill_clients.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state_handler.h"
#include "components/image_fetcher/core/mock_image_fetcher.h"
#include "components/image_fetcher/core/request_metadata.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/test_web_ui.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/chromeos/resources/grit/ui_chromeos_resources.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"

using testing::_;

namespace chromeos {

namespace {

constexpr char kFakeParentGaiaId[] = "someObfuscatedGaiaId";
constexpr char kFakeParentGaiaId2[] = "anotherObfuscatedGaiaId";
constexpr char kFakeParentCredential[] = "someParentCredential";
constexpr char kFakeAccessToken[] = "someAccessToken";

std::vector<FamilyInfoFetcher::FamilyMember> GetFakeFamilyMembers() {
  std::vector<FamilyInfoFetcher::FamilyMember> members;
  members.push_back(FamilyInfoFetcher::FamilyMember(
      kFakeParentGaiaId, FamilyInfoFetcher::HEAD_OF_HOUSEHOLD, "Homer Simpson",
      "homer@simpson.com", "http://profile.url/homer",
      "http://profile.url/homer/image"));
  members.push_back(FamilyInfoFetcher::FamilyMember(
      kFakeParentGaiaId2, FamilyInfoFetcher::PARENT, "Marge Simpson",
      std::string(), "http://profile.url/marge", std::string()));
  members.push_back(FamilyInfoFetcher::FamilyMember(
      "obfuscatedGaiaId3", FamilyInfoFetcher::CHILD, "Lisa Simpson",
      "lisa@gmail.com", std::string(), "http://profile.url/lisa/image"));
  members.push_back(FamilyInfoFetcher::FamilyMember(
      "obfuscatedGaiaId4", FamilyInfoFetcher::CHILD, "Bart Simpson",
      "bart@bart.bart", std::string(), std::string()));
  members.push_back(FamilyInfoFetcher::FamilyMember(
      "obfuscatedGaiaId5", FamilyInfoFetcher::MEMBER, std::string(),
      std::string(), std::string(), std::string()));
  return members;
}

std::map<std::string, GURL> GetFakeProfileImageUrlMap() {
  return {
      {kFakeParentGaiaId, GURL("http://profile.url/homer/image")},
      {kFakeParentGaiaId2, GURL()},
  };
}

gfx::Image GetFakeImage() {
  return ui::ResourceBundle::GetSharedInstance().GetImageNamed(
      IDR_LOGIN_DEFAULT_USER);
}

std::map<std::string, gfx::Image> GetFakeProfileImageMap() {
  return {
      {kFakeParentGaiaId, GetFakeImage()},
      {kFakeParentGaiaId2, gfx::Image()},
  };
}

base::ListValue GetFakeParentsWithoutImage() {
  base::ListValue parents;

  base::DictionaryValue parent1;
  parent1.SetStringKey("email", "homer@simpson.com");
  parent1.SetStringKey("displayName", "Homer Simpson");
  parent1.SetStringKey("obfuscatedGaiaId", kFakeParentGaiaId);
  parents.Append(std::move(parent1));

  base::DictionaryValue parent2;
  parent2.SetStringKey("email", std::string());
  parent2.SetStringKey("displayName", "Marge Simpson");
  parent2.SetStringKey("obfuscatedGaiaId", kFakeParentGaiaId2);
  parents.Append(std::move(parent2));

  return parents;
}

base::ListValue GetFakeParentsWithImage() {
  base::ListValue parents = GetFakeParentsWithoutImage();
  std::map<std::string, gfx::Image> profile_images = GetFakeProfileImageMap();

  for (auto& parent : parents.GetList()) {
    const std::string* obfuscated_gaia_id =
        parent.FindStringKey("obfuscatedGaiaId");
    DCHECK(obfuscated_gaia_id);
    std::string profile_image;
    if (profile_images[*obfuscated_gaia_id].IsEmpty()) {
      gfx::ImageSkia default_icon =
          *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
              IDR_LOGIN_DEFAULT_USER);

      profile_image = webui::GetBitmapDataUrl(
          default_icon.GetRepresentation(1.0f).GetBitmap());
    } else {
      profile_image = webui::GetBitmapDataUrl(
          profile_images[*obfuscated_gaia_id].AsBitmap());
    }
    parent.SetStringKey("profileImage", profile_image);
  }

  return parents;
}

base::DictionaryValue GetFakeParent() {
  base::DictionaryValue parent;
  parent.SetStringKey("email", "homer@simpson.com");
  parent.SetStringKey("displayName", "Homer Simpson");
  parent.SetStringKey("profileImageUrl", "http://profile.url/homer/image");
  parent.SetStringKey("obfuscatedGaiaId", kFakeParentGaiaId);
  return parent;
}

class MockEduAccountLoginHandler : public EduAccountLoginHandler {
 public:
  explicit MockEduAccountLoginHandler(
      const base::RepeatingClosure& close_dialog_closure)
      : EduAccountLoginHandler(close_dialog_closure) {}
  using EduAccountLoginHandler::set_web_ui;

  MOCK_METHOD(void, FetchFamilyMembers, (), (override));
  MOCK_METHOD(void,
              FetchAccessToken,
              (const std::string& obfuscated_gaia_id,
               const std::string& password),
              (override));
  MOCK_METHOD(void,
              FetchReAuthProofTokenForParent,
              (const std::string& child_oauth_access_token,
               const std::string& parent_obfuscated_gaia_id,
               const std::string& parent_credential),
              (override));
  MOCK_METHOD(void,
              FetchParentImages,
              (base::ListValue parents,
               (std::map<std::string, GURL> profile_image_urls)),
              (override));
};
}  // namespace

class EduAccountLoginHandlerTest : public testing::Test {
 public:
  EduAccountLoginHandlerTest() = default;

  void SetUp() override {
    shill_clients::InitializeFakes();
    NetworkHandler::Initialize();
    base::RunLoop().RunUntilIdle();
  }

  void SetupNetwork(bool network_status_online = true) {
    const NetworkState* default_network =
        NetworkHandler::Get()->network_state_handler()->DefaultNetwork();
    const std::string guid =
        default_network ? default_network->guid() : std::string();

    network_portal_detector::InitializeForTesting(&network_portal_detector_);
    network_portal_detector_.SetDefaultNetworkForTesting(guid);
    NetworkPortalDetector::CaptivePortalStatus status;
    int response_code = -1;
    if (network_status_online) {
      status = NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE;
      response_code = 204;  // No content
    } else {
      status = NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL;
      response_code = 200;  // OK
    }
    if (!guid.empty()) {
      network_portal_detector_.SetDetectionResultsForTesting(guid, status,
                                                             response_code);
    }

    mock_image_fetcher_ = std::make_unique<image_fetcher::MockImageFetcher>();
    handler_ = std::make_unique<MockEduAccountLoginHandler>(base::DoNothing());
    handler_->set_web_ui(web_ui());
  }

  void TearDown() override {
    handler_.reset();
    network_portal_detector::InitializeForTesting(nullptr);
    chromeos::NetworkHandler::Shutdown();
    chromeos::shill_clients::Shutdown();
  }

  void VerifyJavascriptCallbackResolved(
      const content::TestWebUI::CallData& data,
      const std::string& event_name,
      bool success = true) {
    EXPECT_EQ("cr.webUIResponse", data.function_name());

    std::string callback_id;
    ASSERT_TRUE(data.arg1()->GetAsString(&callback_id));
    EXPECT_EQ(event_name, callback_id);

    bool callback_success = false;
    ASSERT_TRUE(data.arg2()->GetAsBoolean(&callback_success));
    EXPECT_EQ(success, callback_success);
  }

  image_fetcher::MockImageFetcher* mock_image_fetcher() const {
    return mock_image_fetcher_.get();
  }

  MockEduAccountLoginHandler* handler() const { return handler_.get(); }

  content::TestWebUI* web_ui() { return &web_ui_; }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  NetworkPortalDetectorTestImpl network_portal_detector_;
  std::unique_ptr<image_fetcher::MockImageFetcher> mock_image_fetcher_;
  std::unique_ptr<MockEduAccountLoginHandler> handler_;
  content::TestWebUI web_ui_;
};

TEST_F(EduAccountLoginHandlerTest, HandleGetParentsSuccess) {
  SetupNetwork();
  constexpr char callback_id[] = "handle-get-parents-callback";
  base::ListValue list_args;
  list_args.AppendString(callback_id);

  EXPECT_CALL(*handler(), FetchFamilyMembers());
  handler()->HandleGetParents(&list_args);

  EXPECT_CALL(*handler(), FetchParentImages(_, GetFakeProfileImageUrlMap()));
  // Simulate successful fetching of family members -> expect FetchParentImages
  // to be called.
  handler()->OnGetFamilyMembersSuccess(GetFakeFamilyMembers());

  // Simulate successful fetching of the images -> expect JavascriptCallack to
  // be resolved.
  handler()->OnParentProfileImagesFetched(GetFakeParentsWithoutImage(),
                                          GetFakeProfileImageMap());

  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  VerifyJavascriptCallbackResolved(data, callback_id);

  ASSERT_EQ(GetFakeParentsWithImage(), *data.arg3());
}

TEST_F(EduAccountLoginHandlerTest, HandleGetParentsFailure) {
  SetupNetwork();
  constexpr char callback_id[] = "handle-get-parents-callback";
  base::ListValue list_args;
  list_args.AppendString(callback_id);

  EXPECT_CALL(*handler(), FetchFamilyMembers());
  handler()->HandleGetParents(&list_args);

  // Simulate failed fetching of family members.
  handler()->OnFailure(FamilyInfoFetcher::ErrorCode::kNetworkError);
  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  VerifyJavascriptCallbackResolved(data, callback_id, false /*success*/);

  ASSERT_EQ(base::ListValue(), *data.arg3());
}

TEST_F(EduAccountLoginHandlerTest, HandleParentSigninSuccess) {
  SetupNetwork();
  handler()->AllowJavascriptForTesting();

  constexpr char callback_id[] = "handle-parent-signin-callback";
  base::ListValue list_args;
  list_args.AppendString(callback_id);
  list_args.Append(GetFakeParent());
  list_args.Append(kFakeParentCredential);

  EXPECT_CALL(*handler(),
              FetchAccessToken(kFakeParentGaiaId, kFakeParentCredential));
  handler()->HandleParentSignin(&list_args);

  EXPECT_CALL(*handler(),
              FetchReAuthProofTokenForParent(
                  kFakeAccessToken, kFakeParentGaiaId, kFakeParentCredential));
  handler()->CreateReAuthProofTokenForParent(
      kFakeParentGaiaId, kFakeParentCredential,
      GoogleServiceAuthError(GoogleServiceAuthError::NONE),
      signin::AccessTokenInfo(kFakeAccessToken,
                              base::Time::Now() + base::TimeDelta::FromHours(1),
                              "id_token"));

  constexpr char fake_rapt[] = "fakeReauthProofToken";
  // Simulate successful fetching of ReAuthProofToken.
  handler()->OnReAuthProofTokenSuccess(fake_rapt);
  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  VerifyJavascriptCallbackResolved(data, callback_id);

  ASSERT_EQ(base::Value(fake_rapt), *data.arg3());
}

TEST_F(EduAccountLoginHandlerTest, HandleParentSigninAccessTokenFailure) {
  SetupNetwork();
  handler()->AllowJavascriptForTesting();

  constexpr char callback_id[] = "handle-parent-signin-callback";
  base::ListValue list_args;
  list_args.AppendString(callback_id);
  list_args.Append(GetFakeParent());
  list_args.Append(kFakeParentCredential);

  EXPECT_CALL(*handler(),
              FetchAccessToken(kFakeParentGaiaId, kFakeParentCredential));
  handler()->HandleParentSignin(&list_args);

  handler()->CreateReAuthProofTokenForParent(
      kFakeParentGaiaId, kFakeParentCredential,
      GoogleServiceAuthError(GoogleServiceAuthError::SERVICE_ERROR),
      signin::AccessTokenInfo());
  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  VerifyJavascriptCallbackResolved(data, callback_id, false /*success*/);

  base::DictionaryValue result;
  result.SetBoolKey("isWrongPassword", false);
  ASSERT_EQ(result, *data.arg3());
}

TEST_F(EduAccountLoginHandlerTest, HandleParentSigninReAuthProofTokenFailure) {
  SetupNetwork();
  handler()->AllowJavascriptForTesting();

  constexpr char callback_id[] = "handle-parent-signin-callback";
  base::ListValue list_args;
  list_args.AppendString(callback_id);
  list_args.Append(GetFakeParent());
  list_args.Append(kFakeParentCredential);

  EXPECT_CALL(*handler(),
              FetchAccessToken(kFakeParentGaiaId, kFakeParentCredential));
  handler()->HandleParentSignin(&list_args);

  EXPECT_CALL(*handler(),
              FetchReAuthProofTokenForParent(
                  kFakeAccessToken, kFakeParentGaiaId, kFakeParentCredential));
  handler()->CreateReAuthProofTokenForParent(
      kFakeParentGaiaId, kFakeParentCredential,
      GoogleServiceAuthError(GoogleServiceAuthError::NONE),
      signin::AccessTokenInfo(kFakeAccessToken,
                              base::Time::Now() + base::TimeDelta::FromHours(1),
                              "id_token"));

  // Simulate failed fetching of ReAuthProofToken.
  handler()->OnReAuthProofTokenFailure(
      GaiaAuthConsumer::ReAuthProofTokenStatus::kInvalidGrant);
  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  VerifyJavascriptCallbackResolved(data, callback_id, false);

  base::DictionaryValue result;
  result.SetBoolKey("isWrongPassword", true);
  ASSERT_EQ(result, *data.arg3());
}

TEST_F(EduAccountLoginHandlerTest, ProfileImageFetcherTest) {
  SetupNetwork();
  std::map<std::string, gfx::Image> expected_profile_images =
      GetFakeProfileImageMap();

  // Expect callback to be called with all images in GetFakeProfileImageMap.
  auto callback = base::BindLambdaForTesting(
      [&](std::map<std::string, gfx::Image> profile_images) {
        EXPECT_EQ(expected_profile_images.size(), profile_images.size());

        for (const auto& profile_image_pair : profile_images) {
          gfx::Image expected_image =
              expected_profile_images[profile_image_pair.first];
          EXPECT_TRUE(gfx::test::AreImagesEqual(expected_image,
                                                profile_image_pair.second));
        }
      });

  // Expect to be called 1 time (only for image with URL). For profile with
  // empty image URL - default gfx::Image() should be returned.
  EXPECT_CALL(*mock_image_fetcher(), FetchImageAndData_(_, _, _, _)).Times(1);

  auto profile_image_fetcher =
      std::make_unique<EduAccountLoginHandler::ProfileImageFetcher>(
          mock_image_fetcher(), GetFakeProfileImageUrlMap(), callback);
  profile_image_fetcher->FetchProfileImages();

  // Simulate successful image fetching (for image with URL) -> expect the
  // callback to be called.
  profile_image_fetcher->OnImageFetched(kFakeParentGaiaId, GetFakeImage(),
                                        image_fetcher::RequestMetadata());
}

TEST_F(EduAccountLoginHandlerTest, HandleIsNetworkReadyOffline) {
  SetupNetwork(/*network_status_online=*/false);
  constexpr char callback_id[] = "is-network-ready-callback";
  base::ListValue list_args;
  list_args.AppendString(callback_id);

  handler()->HandleIsNetworkReady(&list_args);

  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  VerifyJavascriptCallbackResolved(data, callback_id);

  bool result = false;
  ASSERT_TRUE(data.arg3()->GetAsBoolean(&result));
  // IsNetworkReady should return false.
  ASSERT_FALSE(result);
}

TEST_F(EduAccountLoginHandlerTest, HandleIsNetworkReadyOnline) {
  SetupNetwork(/*network_status_online=*/true);
  constexpr char callback_id[] = "is-network-ready-callback";
  base::ListValue list_args;
  list_args.AppendString(callback_id);

  handler()->HandleIsNetworkReady(&list_args);

  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  VerifyJavascriptCallbackResolved(data, callback_id);

  bool result = false;
  ASSERT_TRUE(data.arg3()->GetAsBoolean(&result));
  // IsNetworkReady should return true.
  ASSERT_TRUE(result);
}

}  // namespace chromeos
