// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/companion/companion_url_builder.h"

#include "base/base64.h"
#include "base/logging.h"
#include "chrome/browser/ui/webui/side_panel/companion/constants.h"
#include "chrome/browser/ui/webui/side_panel/companion/msbb_delegate.h"
#include "chrome/browser/ui/webui/side_panel/companion/promo_handler.h"
#include "chrome/browser/ui/webui/side_panel/companion/proto/companion_url_params.pb.h"
#include "chrome/browser/ui/webui/side_panel/companion/signin_delegate.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "net/base/url_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ContainsRegex;
using ::testing::MatchesRegex;

namespace companion {
namespace {

constexpr char kValidUrl[] = "https://foo.com/";
constexpr char kOrigin[] = "chrome-untrusted://companion-side-panel.top-chrome";

class MockSigninDelegate : public SigninDelegate {
 public:
  MOCK_METHOD0(AllowedSignin, bool());
  MOCK_METHOD0(StartSigninFlow, void());
};

class MockMsbbDelegate : public MsbbDelegate {
 public:
  MOCK_METHOD1(EnableMsbb, void(bool));
  MOCK_METHOD0(IsMsbbEnabled, bool());
};

}  // namespace

class CompanionUrlBuilderTest : public testing::Test {
 public:
  CompanionUrlBuilderTest() = default;
  ~CompanionUrlBuilderTest() override = default;

  void SetUp() override {
    PromoHandler::RegisterProfilePrefs(pref_service_.registry());
    EXPECT_CALL(msbb_delegate_, IsMsbbEnabled())
        .WillRepeatedly(testing::Return(true));

    pref_service_.SetUserPref(kSigninPromoDeclinedCountPref, base::Value(1));
    EXPECT_CALL(signin_delegate_, AllowedSignin())
        .WillRepeatedly(testing::Return(false));
    url_builder_ = std::make_unique<CompanionUrlBuilder>(
        &pref_service_, &signin_delegate_, &msbb_delegate_);
  }

 protected:
  void VerifyPageUrlSent(GURL page_url, bool expect_was_sent) {
    GURL companion_url = url_builder_->BuildCompanionURL(page_url);

    // Deserialize the query param into protobuf.
    companion::proto::QueryParams proto =
        DeserializeCompanionRequest(companion_url);

    if (expect_was_sent) {
      EXPECT_EQ(proto.page_url(), page_url.spec());
    } else {
      EXPECT_EQ(proto.page_url(), std::string());
    }

    EXPECT_TRUE(proto.has_msbb_enabled());
  }
  // Deserialize the query param into proto::QueryParams.
  proto::QueryParams DeserializeCompanionRequest(GURL companion_url) {
    companion::proto::QueryParams proto;
    std::string url_param;
    EXPECT_TRUE(net::GetValueForKeyInQuery(companion_url, "query", &url_param));
    auto base64_decoded = base::Base64Decode(url_param);
    auto serialized_proto = std::string(base64_decoded.value().begin(),
                                        base64_decoded.value().end());
    EXPECT_TRUE(proto.ParseFromString(serialized_proto));
    return proto;
  }

  TestingPrefServiceSimple pref_service_;
  MockSigninDelegate signin_delegate_;
  MockMsbbDelegate msbb_delegate_;
  std::unique_ptr<CompanionUrlBuilder> url_builder_;
};

TEST_F(CompanionUrlBuilderTest, MsbbOff) {
  EXPECT_CALL(msbb_delegate_, IsMsbbEnabled())
      .WillRepeatedly(testing::Return(false));
  pref_service_.SetUserPref(kSigninPromoDeclinedCountPref, base::Value(1));

  GURL page_url(kValidUrl);
  GURL companion_url = url_builder_->BuildCompanionURL(page_url);

  std::string value;
  EXPECT_FALSE(net::GetValueForKeyInQuery(companion_url, "url", &value));

  EXPECT_TRUE(net::GetValueForKeyInQuery(companion_url, "origin", &value));
  EXPECT_EQ(value, kOrigin);

  // Deserialize the query param into protobuf.
  companion::proto::QueryParams proto =
      DeserializeCompanionRequest(companion_url);

  // URL shouldn't be sent when MSBB is off.
  EXPECT_EQ(proto.page_url(), std::string());
  EXPECT_FALSE(proto.signin_allowed_and_required());
  EXPECT_FALSE(proto.has_msbb_enabled());
}

TEST_F(CompanionUrlBuilderTest, MsbbOn) {
  EXPECT_CALL(signin_delegate_, AllowedSignin())
      .WillRepeatedly(testing::Return(true));
  GURL page_url(kValidUrl);
  EXPECT_CALL(msbb_delegate_, IsMsbbEnabled())
      .WillRepeatedly(testing::Return(true));
  GURL companion_url = url_builder_->BuildCompanionURL(page_url);

  std::string value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(companion_url, "url", &value));
  EXPECT_EQ(value, page_url.spec());

  EXPECT_TRUE(net::GetValueForKeyInQuery(companion_url, "origin", &value));
  EXPECT_EQ(value, kOrigin);

  // Deserialize the query param into protobuf.
  companion::proto::QueryParams proto =
      DeserializeCompanionRequest(companion_url);

  // Verify fields inside protobuf.
  EXPECT_EQ(proto.page_url(), page_url.spec());
  EXPECT_TRUE(proto.has_msbb_enabled());
  EXPECT_TRUE(proto.signin_allowed_and_required());

  // Verify promo state.
  EXPECT_TRUE(proto.has_promo_state());
  EXPECT_EQ(1, proto.promo_state().signin_promo_denial_count());
  EXPECT_EQ(0, proto.promo_state().msbb_promo_denial_count());
  EXPECT_EQ(0, proto.promo_state().labs_promo_denial_count());
}

TEST_F(CompanionUrlBuilderTest, NonProtobufParams) {
  GURL page_url(kValidUrl);
  GURL companion_url = url_builder_->BuildCompanionURL(page_url);

  std::string value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(companion_url, "url", &value));
  EXPECT_EQ(value, page_url.spec());

  EXPECT_TRUE(net::GetValueForKeyInQuery(companion_url, "origin", &value));
  EXPECT_EQ(value, kOrigin);
}

TEST_F(CompanionUrlBuilderTest, ValidPageUrls) {
  EXPECT_CALL(msbb_delegate_, IsMsbbEnabled())
      .WillRepeatedly(testing::Return(true));

  VerifyPageUrlSent(GURL(kValidUrl), true);
  VerifyPageUrlSent(GURL("chrome://new-tab"), false);
  VerifyPageUrlSent(GURL("https://192.168.0.1"), false);
  VerifyPageUrlSent(GURL("https://localhost:8888"), false);
}
}  // namespace companion
