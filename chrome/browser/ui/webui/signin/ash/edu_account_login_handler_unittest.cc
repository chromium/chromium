// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/ash/edu_account_login_handler.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/json/json_writer.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/shill/shill_clients.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "components/image_fetcher/core/mock_image_fetcher.h"
#include "components/image_fetcher/core/request_metadata.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/supervised_user/core/browser/proto/kidsmanagement_messages.pb.h"
#include "components/supervised_user/core/browser/proto_fetcher_status.h"
#include "content/public/test/test_web_ui.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/chromeos/resources/grit/ui_chromeos_resources.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/image/image_unittest_util.h"

using testing::_;

namespace ash {

namespace {

constexpr char kFakeParentGaiaId[] = "someObfuscatedGaiaId";
constexpr char kFakeParentGaiaId2[] = "anotherObfuscatedGaiaId";
constexpr char kFakeParentCredential[] = "someParentCredential";
constexpr char kFakeAccessToken[] = "someAccessToken";

kidsmanagement::ListMembersResponse GetFakeFamilyMembers() {
  kidsmanagement::ListMembersResponse members;

  kidsmanagement::FamilyMember* homer = members.add_members();
  homer->set_role(kidsmanagement::HEAD_OF_HOUSEHOLD);
  homer->set_user_id(kFakeParentGaiaId);
  homer->mutable_profile()->set_display_name("Homer Simpson");
  homer->mutable_profile()->set_email("homer@simpson.com");
  homer->mutable_profile()->set_profile_url("http://profile.url/homer");
  homer->mutable_profile()->set_profile_image_url(
      "http://profile.url/homer/image");

  kidsmanagement::FamilyMember* marge = members.add_members();
  marge->set_role(kidsmanagement::PARENT);
  marge->set_user_id(kFakeParentGaiaId2);
  marge->mutable_profile()->set_display_name("Marge Simpson");
  marge->mutable_profile()->set_profile_url("http://profile.url/marge");

  kidsmanagement::FamilyMember* lisa = members.add_members();
  lisa->set_role(kidsmanagement::CHILD);
  lisa->set_user_id("obfuscatedGaiaId3");
  lisa->mutable_profile()->set_display_name("Lisa Simpson");
  lisa->mutable_profile()->set_email("lisa@gmail.com");
  lisa->mutable_profile()->set_profile_image_url(
      "http://profile.url/lisa/image");

  kidsmanagement::FamilyMember* bart = members.add_members();
  bart->set_role(kidsmanagement::CHILD);
  bart->set_user_id("obfuscatedGaiaId4");
  bart->mutable_profile()->set_display_name("Bart Simpson");
  bart->mutable_profile()->set_email("bart@bart.bart");

  kidsmanagement::FamilyMember* member = members.add_members();
  member->set_role(kidsmanagement::MEMBER);
  member->set_user_id("obfuscatedGaiaId5");
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

base::Value::List GetFakeParentsWithoutImage() {
  base::Value::List parents;

  base::Value::Dict parent1;
  parent1.Set("email", "homer@simpson.com");
  parent1.Set("displayName", "Homer Simpson");
  parent1.Set("obfuscatedGaiaId", kFakeParentGaiaId);
  parents.Append(std::move(parent1));

  base::Value::Dict parent2;
  parent2.Set("email", std::string());
  parent2.Set("displayName", "Marge Simpson");
  parent2.Set("obfuscatedGaiaId", kFakeParentGaiaId2);
  parents.Append(std::move(parent2));

  return parents;
}

base::Value::List GetFakeParentsWithImage() {
  base::Value::List parents = GetFakeParentsWithoutImage();
  std::map<std::string, gfx::Image> profile_images = GetFakeProfileImageMap();

  for (auto& parent : parents) {
    const std::string* obfuscated_gaia_id =
        parent.GetDict().FindString("obfuscatedGaiaId");
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
    parent.GetDict().Set("profileImage", profile_image);
  }

  return parents;
}

base::Value::Dict GetFakeParent() {
  base::Value::Dict parent;
  parent.Set("email", "homer@simpson.com");
  parent.Set("displayName", "Homer Simpson");
  parent.Set("profileImageUrl", "http://profile.url/homer/image");
  parent.Set("obfuscatedGaiaId", kFakeParentGaiaId);
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
              (base::Value::List parents,
               (std::map<std::string, GURL> profile_image_urls)),
              (override));
};
}  // namespace

class EduAccountLoginHandlerTest : public testing::Test {
 public:
  EduAccountLoginHandlerTest() = default;

  void SetUp() override {
    network_handler_test_helper_ = std::make_unique<NetworkHandlerTestHelper>();
    base::RunLoop().RunUntilIdle();
  }

  void SetupNetwork(bool online = true) {
    std::string state =
        online ? shill::kStateOnline : shill::kStateRedirectFound;
    network_handler_test_helper_->ResetDevicesAndServices();
    network_handler_test_helper_->ConfigureWiFi(state);

    mock_image_fetcher_ = std::make_unique<image_fetcher::MockImageFetcher>();
    handler_ = std::make_unique<MockEduAccountLoginHandler>(base::DoNothing());
    handler_->set_web_ui(web_ui());
  }

  void TearDown() override {
    handler_.reset();
    network_handler_test_helper_.reset();
  }

  void VerifyJavascriptCallbackResolved(
      const content::TestWebUI::CallData& data,
      const std::string& event_name,
      bool success = true) {
    EXPECT_EQ("cr.webUIResponse", data.function_name());

    ASSERT_TRUE(data.arg1()->is_string());
    EXPECT_EQ(event_name, data.arg1()->GetString());

    ASSERT_TRUE(data.arg2()->is_bool());
    EXPECT_EQ(success, data.arg2()->GetBool());
  }

  image_fetcher::MockImageFetcher* mock_image_fetcher() const {
    return mock_image_fetcher_.get();
  }

  MockEduAccountLoginHandler* handler() const { return handler_.get(); }

  content::TestWebUI* web_ui() { return &web_ui_; }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<image_fetcher::MockImageFetcher> mock_image_fetcher_;
  std::unique_ptr<MockEduAccountLoginHandler> handler_;
  content::TestWebUI web_ui_;
  std::unique_ptr<NetworkHandlerTestHelper> network_handler_test_helper_;
};

TEST_F(EduAccountLoginHandlerTest, HandleGetParentsSuccess) {
  SetupNetwork();
  constexpr char callback_id[] = "handle-get-parents-callback";
  base::Value::List list_args;
  list_args.Append(callback_id);

  EXPECT_CALL(*handler(), FetchFamilyMembers());
  handler()->HandleGetParents(list_args);

  EXPECT_CALL(*handler(), FetchParentImages(_, GetFakeProfileImageUrlMap()));
  // Simulate successful fetching of family members -> expect FetchParentImages
  // to be called.
  handler()->OnListFamilyMembersSuccess(GetFakeFamilyMembers());

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
  base::Value::List list_args;
  list_args.Append(callback_id);

  EXPECT_CALL(*handler(), FetchFamilyMembers());
  handler()->HandleGetParents(list_args);

  // Simulate failed fetching of family members.
  handler()->OnListFamilyMembersFailure(
      supervised_user::ProtoFetcherStatus::HttpStatusOrNetError(
          net::ERR_IO_PENDING));
  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  VerifyJavascriptCallbackResolved(data, callback_id, /*success=*/false);

  ASSERT_EQ(base::Value::List(), *data.arg3());
}

TEST_F(EduAccountLoginHandlerTest, HandleParentSigninSuccess) {
  SetupNetwork();
  handler()->AllowJavascriptForTesting();

  constexpr char callback_id[] = "handle-parent-signin-callback";
  base::Value::List list_args;
  list_args.Append(callback_id);
  list_args.Append(GetFakeParent());
  list_args.Append(kFakeParentCredential);

  EXPECT_CALL(*handler(),
              FetchAccessToken(kFakeParentGaiaId, kFakeParentCredential));
  handler()->HandleParentSignin(list_args);

  EXPECT_CALL(*handler(),
              FetchReAuthProofTokenForParent(
                  kFakeAccessToken, kFakeParentGaiaId, kFakeParentCredential));
  handler()->CreateReAuthProofTokenForParent(
      kFakeParentGaiaId, kFakeParentCredential,
      GoogleServiceAuthError(GoogleServiceAuthError::NONE),
      signin::AccessTokenInfo(kFakeAccessToken,
                              base::Time::Now() + base::Hours(1), "id_token"));

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
  base::Value::List list_args;
  list_args.Append(callback_id);
  list_args.Append(GetFakeParent());
  list_args.Append(kFakeParentCredential);

  EXPECT_CALL(*handler(),
              FetchAccessToken(kFakeParentGaiaId, kFakeParentCredential));
  handler()->HandleParentSignin(list_args);

  handler()->CreateReAuthProofTokenForParent(
      kFakeParentGaiaId, kFakeParentCredential,
      GoogleServiceAuthError(GoogleServiceAuthError::SERVICE_ERROR),
      signin::AccessTokenInfo());
  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  VerifyJavascriptCallbackResolved(data, callback_id, false /*success*/);

  base::Value::Dict result;
  result.Set("isWrongPassword", false);
  ASSERT_EQ(result, *data.arg3());
}

TEST_F(EduAccountLoginHandlerTest, HandleParentSigninReAuthProofTokenFailure) {
  SetupNetwork();
  handler()->AllowJavascriptForTesting();

  constexpr char callback_id[] = "handle-parent-signin-callback";
  base::Value::List list_args;
  list_args.Append(callback_id);
  list_args.Append(GetFakeParent());
  list_args.Append(kFakeParentCredential);

  EXPECT_CALL(*handler(),
              FetchAccessToken(kFakeParentGaiaId, kFakeParentCredential));
  handler()->HandleParentSignin(list_args);

  EXPECT_CALL(*handler(),
              FetchReAuthProofTokenForParent(
                  kFakeAccessToken, kFakeParentGaiaId, kFakeParentCredential));
  handler()->CreateReAuthProofTokenForParent(
      kFakeParentGaiaId, kFakeParentCredential,
      GoogleServiceAuthError(GoogleServiceAuthError::NONE),
      signin::AccessTokenInfo(kFakeAccessToken,
                              base::Time::Now() + base::Hours(1), "id_token"));

  // Simulate failed fetching of ReAuthProofToken.
  handler()->OnReAuthProofTokenFailure(
      GaiaAuthConsumer::ReAuthProofTokenStatus::kInvalidGrant);
  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  VerifyJavascriptCallbackResolved(data, callback_id, false);

  base::Value::Dict result;
  result.Set("isWrongPassword", true);
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
  base::Value::List list_args;
  list_args.Append(callback_id);

  handler()->HandleIsNetworkReady(list_args);

  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  VerifyJavascriptCallbackResolved(data, callback_id);

  ASSERT_TRUE(data.arg3()->is_bool());
  // IsNetworkReady should return false.
  ASSERT_FALSE(data.arg3()->GetBool());
}

TEST_F(EduAccountLoginHandlerTest, HandleIsNetworkReadyOnline) {
  SetupNetwork(/*network_status_online=*/true);
  constexpr char callback_id[] = "is-network-ready-callback";
  base::Value::List list_args;
  list_args.Append(callback_id);

  handler()->HandleIsNetworkReady(list_args);

  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  VerifyJavascriptCallbackResolved(data, callback_id);

  ASSERT_TRUE(data.arg3()->is_bool());
  // IsNetworkReady should return true.
  ASSERT_TRUE(data.arg3()->GetBool());
}

}  // namespace ash
