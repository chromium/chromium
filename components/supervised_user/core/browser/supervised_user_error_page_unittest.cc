// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/supervised_user_error_page.h"

#include <optional>

#include "base/memory/raw_ptr_exclusion.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/grit/components_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#include "components/supervised_user/core/browser/supervised_user_utils.h"
#include "components/supervised_user/core/common/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest-param-test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace supervised_user {

struct BlockMessageIDTestParameter {
  FilteringBehaviorReason reason;
  bool single_parent;
  int expected_result;
};

class SupervisedUserErrorPageTest_GetBlockMessageID
    : public ::testing::TestWithParam<BlockMessageIDTestParameter> {};

TEST_P(SupervisedUserErrorPageTest_GetBlockMessageID, GetBlockMessageID) {
  BlockMessageIDTestParameter param = GetParam();
  EXPECT_EQ(param.expected_result,
            GetBlockMessageID(param.reason, param.single_parent))
      << "reason = " << int(param.reason)
      << " single parent = " << param.single_parent;
}

BlockMessageIDTestParameter block_message_id_test_params[] = {
    {FilteringBehaviorReason::DEFAULT, true,
     IDS_CHILD_BLOCK_MESSAGE_DEFAULT_SINGLE_PARENT},
    {FilteringBehaviorReason::DEFAULT, false,
     IDS_CHILD_BLOCK_MESSAGE_DEFAULT_MULTI_PARENT},
    // SafeSites is not enabled for supervised users.
    {FilteringBehaviorReason::ASYNC_CHECKER, true,
     IDS_SUPERVISED_USER_BLOCK_MESSAGE_SAFE_SITES},
    {FilteringBehaviorReason::ASYNC_CHECKER, false,
     IDS_SUPERVISED_USER_BLOCK_MESSAGE_SAFE_SITES},
    {FilteringBehaviorReason::MANUAL, true,
     IDS_CHILD_BLOCK_MESSAGE_MANUAL_SINGLE_PARENT},
    {FilteringBehaviorReason::MANUAL, false,
     IDS_CHILD_BLOCK_MESSAGE_MANUAL_MULTI_PARENT},
};

INSTANTIATE_TEST_SUITE_P(GetBlockMessageIDParameterized,
                         SupervisedUserErrorPageTest_GetBlockMessageID,
                         ::testing::ValuesIn(block_message_id_test_params));

struct InterstitialMessageIDTestParameter {
  FilteringBehaviorReason reason;
  bool is_interstitial_v3_enabled;
  int expected_result;
};

class SupervisedUserErrorPageTest_GetInterstitialMessageID
    : public ::testing::TestWithParam<InterstitialMessageIDTestParameter> {};

TEST_P(SupervisedUserErrorPageTest_GetInterstitialMessageID,
       GetInterstitialMessageID) {
  base::test::ScopedFeatureList scoped_feature_list_;
  InterstitialMessageIDTestParameter param = GetParam();
  if (param.is_interstitial_v3_enabled) {
    scoped_feature_list_.InitAndEnableFeature(
        supervised_user::kSupervisedUserBlockInterstitialV3);
  } else {
    scoped_feature_list_.InitAndDisableFeature(
        supervised_user::kSupervisedUserBlockInterstitialV3);
  }
  EXPECT_EQ(param.expected_result, GetInterstitialMessageID(param.reason))
      << "reason = " << int(param.reason);
}

InterstitialMessageIDTestParameter interstitial_message_id_test_params[] = {
    {FilteringBehaviorReason::DEFAULT, true,
     IDS_SUPERVISED_USER_INTERSTITIAL_MESSAGE_BLOCK_ALL},
    {FilteringBehaviorReason::ASYNC_CHECKER, true,
     IDS_SUPERVISED_USER_INTERSTITIAL_MESSAGE_SAFE_SITES},
    {FilteringBehaviorReason::MANUAL, true,
     IDS_SUPERVISED_USER_INTERSTITIAL_MESSAGE_MANUAL},
    {FilteringBehaviorReason::DEFAULT, false,
     IDS_CHILD_BLOCK_INTERSTITIAL_MESSAGE_V2},
    {FilteringBehaviorReason::ASYNC_CHECKER, false,
     IDS_CHILD_BLOCK_INTERSTITIAL_MESSAGE_V2},
    {FilteringBehaviorReason::MANUAL, false,
     IDS_CHILD_BLOCK_INTERSTITIAL_MESSAGE_V2},
};

INSTANTIATE_TEST_SUITE_P(
    GetInterstitialMessageIDParameterized,
    SupervisedUserErrorPageTest_GetInterstitialMessageID,
    ::testing::ValuesIn(interstitial_message_id_test_params));

struct BuildHtmlTestParameter {
  bool allow_access_requests;
  std::optional<Custodian> custodian;
  std::optional<Custodian> second_custodian;
  FilteringBehaviorReason reason;
  bool is_interstitial_v3_enabled;
};

class SupervisedUserErrorPageTest_BuildHtml
    : public ::testing::TestWithParam<BuildHtmlTestParameter> {
 protected:
  void VerifyCustodianInfo() {
    // The custodian info will also be available for the V3 interstitial
    // page but it will not be immediately visible.
    EXPECT_THAT(html_error_page_,
                testing::HasSubstr(GetParam().custodian->GetProfileImageUrl()));
    EXPECT_THAT(html_error_page_,
                testing::HasSubstr(GetParam().custodian->GetName()));
    EXPECT_THAT(html_error_page_,
                testing::HasSubstr(GetParam().custodian->GetEmailAddress()));
    if (GetParam().second_custodian.has_value()) {
      EXPECT_THAT(html_error_page_,
                  testing::HasSubstr(
                      GetParam().second_custodian->GetProfileImageUrl()));
      EXPECT_THAT(html_error_page_,
                  testing::HasSubstr(GetParam().second_custodian->GetName()));
      EXPECT_THAT(
          html_error_page_,
          testing::HasSubstr(GetParam().second_custodian->GetEmailAddress()));
    }
  }

  void VerifyBlockReasonStrings() {
    if (GetParam().is_interstitial_v3_enabled) {
      // The block reason components is not available for the V3 interstitial.
      EXPECT_THAT(html_error_page_,
                  testing::Not(testing::HasSubstr(l10n_util::GetStringUTF8(
                      IDS_GENERIC_SITE_BLOCK_HEADER))));
    } else {
      // Verify that the HTML contains a block message that is specific to the
      // number of parents who can approve and the reason that the site is
      // blocked.
      EXPECT_THAT(html_error_page_, testing::HasSubstr(l10n_util::GetStringUTF8(
                                        IDS_GENERIC_SITE_BLOCK_HEADER)));
      EXPECT_THAT(
          html_error_page_,
          testing::HasSubstr(l10n_util::GetStringUTF8(GetBlockMessageID(
              GetParam().reason,
              /*single_parent=*/!GetParam().second_custodian.has_value()))));
    }
  }

  void VerifyInterstitialButtons() {
    // These strings are used for button always present in the DOM, but only
    // visible when local web approvals is enabled or when the request is
    // sent.
    EXPECT_THAT(html_error_page_,
                testing::HasSubstr(l10n_util::GetStringUTF8(
                    IDS_BLOCK_INTERSTITIAL_ASK_IN_PERSON_BUTTON)));
    EXPECT_THAT(html_error_page_,
                testing::HasSubstr(l10n_util::GetStringUTF8(
                    IDS_BLOCK_INTERSTITIAL_ASK_IN_A_MESSAGE_BUTTON)));
    EXPECT_THAT(
        html_error_page_,
        testing::HasSubstr(l10n_util::GetStringUTF8(IDS_REQUEST_SENT_OK)));
  }

  std::string html_error_page_;
};

TEST_P(SupervisedUserErrorPageTest_BuildHtml, BuildHtml) {
  BuildHtmlTestParameter param = GetParam();
  base::test::ScopedFeatureList scoped_feature_list_;

  std::vector<base::test::FeatureRef> enabled_features = {
      supervised_user::kLocalWebApprovals};
  std::vector<base::test::FeatureRef> disabled_features;
  if (param.is_interstitial_v3_enabled) {
    enabled_features.push_back(
        supervised_user::kSupervisedUserBlockInterstitialV3);
  } else {
    disabled_features.push_back(
        supervised_user::kSupervisedUserBlockInterstitialV3);
  }
  scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);

  // BuildErrorPageHtml should returns the original HTML (with $i18n{}
  // replacements) plus scripts that plug values into it. The test can't
  // easily check that the scripts are correct, but can check that the output
  // contains the expected values.
  html_error_page_ = BuildErrorPageHtml(
      param.allow_access_requests, param.custodian, param.second_custodian,
      param.reason,
      /*app_locale=*/"",
      /*already_sent_remote_request=*/false, /*is_main_frame=*/true,
      /*ios_font_size_multiplier=*/std::nullopt);

  VerifyCustodianInfo();

  VerifyInterstitialButtons();

  // Messages containing parameters aren't tested since they get modified
  // before they are added to the result.
  if (param.allow_access_requests) {
    VerifyBlockReasonStrings();
    EXPECT_THAT(html_error_page_, testing::HasSubstr(l10n_util::GetStringUTF8(
                                      IDS_CHILD_BLOCK_INTERSTITIAL_HEADER)));
    EXPECT_THAT(html_error_page_,
                testing::HasSubstr(l10n_util::GetStringUTF8(
                    param.is_interstitial_v3_enabled
                        ? GetInterstitialMessageID(param.reason)
                        : IDS_CHILD_BLOCK_INTERSTITIAL_MESSAGE_V2)));
    EXPECT_THAT(html_error_page_,
                testing::Not(testing::HasSubstr(l10n_util::GetStringUTF8(
                    IDS_BLOCK_INTERSTITIAL_HEADER_ACCESS_REQUESTS_DISABLED))));
  } else {
    EXPECT_THAT(html_error_page_,
                testing::Not(testing::HasSubstr(l10n_util::GetStringUTF8(
                    IDS_CHILD_BLOCK_INTERSTITIAL_HEADER))));
    EXPECT_THAT(html_error_page_,
                testing::Not(testing::HasSubstr(l10n_util::GetStringUTF8(
                    IDS_CHILD_BLOCK_INTERSTITIAL_MESSAGE_V2))));
    EXPECT_THAT(html_error_page_,
                testing::HasSubstr(l10n_util::GetStringUTF8(
                    IDS_BLOCK_INTERSTITIAL_HEADER_ACCESS_REQUESTS_DISABLED)));
  }

  EXPECT_THAT(html_error_page_,
              testing::HasSubstr(l10n_util::GetStringUTF8(
                  IDS_CHILD_BLOCK_INTERSTITIAL_WAITING_APPROVAL_MESSAGE)));
  if (param.second_custodian.has_value()) {
    EXPECT_THAT(
        html_error_page_,
        testing::HasSubstr(l10n_util::GetStringUTF8(
            IDS_CHILD_BLOCK_INTERSTITIAL_WAITING_APPROVAL_DESCRIPTION_MULTI_PARENT)));
  } else {
    EXPECT_THAT(
        html_error_page_,
        testing::HasSubstr(l10n_util::GetStringUTF8(
            IDS_CHILD_BLOCK_INTERSTITIAL_WAITING_APPROVAL_DESCRIPTION_SINGLE_PARENT)));
  }
}

BuildHtmlTestParameter build_html_test_parameter[] = {
    {true, Custodian("custodian", "custodian_email", "url1"), std::nullopt,
     FilteringBehaviorReason::DEFAULT, false},
    {true, Custodian("custodian", "custodian_email", "url1"),
     Custodian("custodian2", "custodian2_email", "url2"),
     FilteringBehaviorReason::DEFAULT, false},
    {false, Custodian("custodian", "custodian_email", "url1"),
     Custodian("custodian2", "custodian2_email", "url2"),
     FilteringBehaviorReason::DEFAULT, false},
    {false, Custodian("custodian", "custodian_email", "url1"),
     Custodian("custodian2", "custodian2_email", "url2"),
     FilteringBehaviorReason::DEFAULT, false},
    {true, Custodian("custodian", "custodian_email", "url1"),
     Custodian("custodian2", "custodian2_email", "url2"),
     FilteringBehaviorReason::DEFAULT, false},
    {true, Custodian("custodian", "custodian_email", "url1"),
     Custodian("custodian2", "custodian2_email", "url2"),
     FilteringBehaviorReason::ASYNC_CHECKER, false},
    {true, Custodian("custodian", "custodian_email", "url1"),
     Custodian("custodian2", "custodian2_email", "url2"),
     FilteringBehaviorReason::MANUAL, false},

    // Test cases with the v3 interstitial enabled.
    {true, Custodian("custodian", "custodian_email", "url1"), std::nullopt,
     FilteringBehaviorReason::DEFAULT, true},
    {true, Custodian("custodian", "custodian_email", "url1"),
     Custodian("custodian2", "custodian2_email", "url2"),
     FilteringBehaviorReason::DEFAULT, true},
    {false, Custodian("custodian", "custodian_email", "url1"),
     Custodian("custodian2", "custodian2_email", "url2"),
     FilteringBehaviorReason::DEFAULT, true},
    {false, Custodian("custodian", "custodian_email", "url1"),
     Custodian("custodian2", "custodian2_email", "url2"),
     FilteringBehaviorReason::DEFAULT, true},
    {true, Custodian("custodian", "custodian_email", "url1"),
     Custodian("custodian2", "custodian2_email", "url2"),
     FilteringBehaviorReason::DEFAULT, true},
    {true, Custodian("custodian", "custodian_email", "url1"),
     Custodian("custodian2", "custodian2_email", "url2"),
     FilteringBehaviorReason::ASYNC_CHECKER, true},
    {true, Custodian("custodian", "custodian_email", "url1"),
     Custodian("custodian2", "custodian2_email", "url2"),
     FilteringBehaviorReason::MANUAL, true},
};

INSTANTIATE_TEST_SUITE_P(GetBlockMessageIDParameterized,
                         SupervisedUserErrorPageTest_BuildHtml,
                         ::testing::ValuesIn(build_html_test_parameter));

}  // namespace supervised_user
