// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/companion/companion_url_builder.h"

#include "base/base64.h"
#include "base/logging.h"
#include "chrome/browser/ui/webui/side_panel/companion/constants.h"
#include "chrome/browser/ui/webui/side_panel/companion/proto/companion_url_params.pb.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/unified_consent/pref_names.h"
#include "net/base/url_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ContainsRegex;
using ::testing::MatchesRegex;

namespace companion {
namespace {
constexpr char kUrl[] = "https://foo.com/";
constexpr char kOrigin[] = "chrome-untrusted://companion-side-panel.top-chrome";
}  // namespace

class CompanionUrlBuilderTest : public testing::Test {
 public:
  CompanionUrlBuilderTest() = default;
  ~CompanionUrlBuilderTest() override = default;

  void SetUp() override {
    pref_service_.registry()->RegisterBooleanPref(
        unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
        false);

    pref_service_.registry()->RegisterIntegerPref(kSigninPromoDeclinedPref, 0);
    pref_service_.registry()->RegisterIntegerPref(kMsbbPromoDeclinedPref, 0);
    pref_service_.registry()->RegisterIntegerPref(kLabsPromoDeclinedPref, 0);

    pref_service_.SetUserPref(
        unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
        base::Value(true));
    pref_service_.SetUserPref(kSigninPromoDeclinedPref, base::Value(1));
    url_builder_ = std::make_unique<CompanionUrlBuilder>(&pref_service_);
  }

 protected:
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<CompanionUrlBuilder> url_builder_;
};

TEST_F(CompanionUrlBuilderTest, MsbbOff) {
  pref_service_.SetUserPref(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
      base::Value(false));
  pref_service_.SetUserPref(kSigninPromoDeclinedPref, base::Value(1));
  EXPECT_FALSE(url_builder_->IsMsbbEnabled());

  GURL page_url(kUrl);
  GURL companion_url = url_builder_->BuildCompanionURL(page_url);

  std::string value;
  EXPECT_FALSE(net::GetValueForKeyInQuery(companion_url, "url", &value));

  EXPECT_TRUE(net::GetValueForKeyInQuery(companion_url, "origin", &value));
  EXPECT_EQ(value, kOrigin);

  // Deserialize the query param into protobuf.
  companion::proto::QueryParams proto;
  std::string url_param;
  EXPECT_TRUE(net::GetValueForKeyInQuery(companion_url, "query", &url_param));
  auto base64_decoded = base::Base64Decode(url_param);
  auto serialized_proto =
      std::string(base64_decoded.value().begin(), base64_decoded.value().end());
  EXPECT_TRUE(proto.ParseFromString(serialized_proto));

  // URL shouldn't be sent when MSBB is off.
  EXPECT_EQ(proto.page_url(), std::string());
  EXPECT_FALSE(proto.has_msbb_enabled());
}

TEST_F(CompanionUrlBuilderTest, MsbbOn) {
  GURL page_url(kUrl);
  GURL companion_url = url_builder_->BuildCompanionURL(page_url);
  EXPECT_TRUE(url_builder_->IsMsbbEnabled());

  std::string value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(companion_url, "url", &value));
  EXPECT_EQ(value, page_url.spec());

  EXPECT_TRUE(net::GetValueForKeyInQuery(companion_url, "origin", &value));
  EXPECT_EQ(value, kOrigin);

  // Deserialize the query param into protobuf.
  companion::proto::QueryParams proto;
  std::string url_param;
  EXPECT_TRUE(net::GetValueForKeyInQuery(companion_url, "query", &url_param));
  auto base64_decoded = base::Base64Decode(url_param);
  auto serialized_proto =
      std::string(base64_decoded.value().begin(), base64_decoded.value().end());
  EXPECT_TRUE(proto.ParseFromString(serialized_proto));

  // Verify fields inside protobuf.
  EXPECT_EQ(proto.page_url(), page_url.spec());
  EXPECT_TRUE(proto.has_msbb_enabled());

  // TODO(b/273652233): Uncomment.
  // Verify promo state.
  // EXPECT_TRUE(proto.has_promo_state());
  // EXPECT_EQ(1, proto.promo_state().signin_promo_denial_count());
  // EXPECT_EQ(0, proto.promo_state().msbb_promo_denial_count());
  // EXPECT_EQ(0, proto.promo_state().labs_promo_denial_count());
}

TEST_F(CompanionUrlBuilderTest, NonProtobufParams) {
  GURL page_url(kUrl);
  GURL companion_url = url_builder_->BuildCompanionURL(page_url);

  std::string value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(companion_url, "url", &value));
  EXPECT_EQ(value, page_url.spec());

  EXPECT_TRUE(net::GetValueForKeyInQuery(companion_url, "origin", &value));
  EXPECT_EQ(value, kOrigin);
}

}  // namespace companion
