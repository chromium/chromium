// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/supervised_user_error_page.h"

#include "base/memory/raw_ptr_exclusion.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/grit/components_resources.h"
#include "components/strings/grit/components_strings.h"
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

struct BuildHtmlTestParameter {
  bool allow_access_requests;
  const std::string profile_image_url;
  const std::string profile_image_url2;
  const std::string custodian;
  const std::string custodian_email;
  const std::string second_custodian;
  const std::string second_custodian_email;
  FilteringBehaviorReason reason;
  bool has_two_parents;
};

class SupervisedUserErrorPageTest_BuildHtml
    : public ::testing::TestWithParam<BuildHtmlTestParameter> {};

TEST_P(SupervisedUserErrorPageTest_BuildHtml, BuildHtml) {
  BuildHtmlTestParameter param = GetParam();
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitWithFeatures(
      /* enabled_features */ {supervised_user::kLocalWebApprovals},
      /* disabled_features */ {});

  std::string result = BuildErrorPageHtml(
      param.allow_access_requests, param.profile_image_url,
      param.profile_image_url2, param.custodian, param.custodian_email,
      param.second_custodian, param.second_custodian_email, param.reason,
      /*app_locale=*/"",
      /*already_sent_request=*/false, /*is_main_frame=*/true,
      /*show_banner=*/true);
  // The result should contain the original HTML (with $i18n{} replacements)
  // plus scripts that plug values into it. The test can't easily check that the
  // scripts are correct, but can check that the output contains the expected
  // values.
  EXPECT_THAT(result, testing::HasSubstr(param.profile_image_url));
  EXPECT_THAT(result, testing::HasSubstr(param.profile_image_url2));
  EXPECT_THAT(result, testing::HasSubstr(param.custodian));
  EXPECT_THAT(result, testing::HasSubstr(param.custodian_email));
  if (param.has_two_parents) {
    EXPECT_THAT(result, testing::HasSubstr(param.second_custodian));
    EXPECT_THAT(result, testing::HasSubstr(param.second_custodian_email));
  }

  // Messages containing parameters aren't tested since they get modified before
  // they are added to the result.
  if (param.allow_access_requests) {
    EXPECT_THAT(result, testing::HasSubstr(l10n_util::GetStringUTF8(
                            IDS_CHILD_BLOCK_INTERSTITIAL_HEADER)));
    EXPECT_THAT(result, testing::HasSubstr(l10n_util::GetStringUTF8(
                            IDS_CHILD_BLOCK_INTERSTITIAL_MESSAGE_V2)));
    // Ensure that HTML contains a block message that is specific to the
    // number of parents who can approve and the reason that the site is
    // blocked. DEFAULT indicates that the parent(s) required the child
    // request permission for all sites, and MANUAL indicates that the
    // parent(s) specifically blocked this site.
    if (param.reason == FilteringBehaviorReason::DEFAULT) {
      if (param.has_two_parents) {
        EXPECT_THAT(result, testing::HasSubstr(l10n_util::GetStringUTF8(
                                IDS_CHILD_BLOCK_MESSAGE_DEFAULT_MULTI_PARENT)));
      } else {
        EXPECT_THAT(result,
                    testing::HasSubstr(l10n_util::GetStringUTF8(
                        IDS_CHILD_BLOCK_MESSAGE_DEFAULT_SINGLE_PARENT)));
      }
      if (param.reason == FilteringBehaviorReason::MANUAL) {
        if (param.has_two_parents) {
          EXPECT_THAT(result,
                      testing::HasSubstr(l10n_util::GetStringUTF8(
                          IDS_CHILD_BLOCK_MESSAGE_MANUAL_MULTI_PARENT)));
        } else {
          EXPECT_THAT(result,
                      testing::HasSubstr(l10n_util::GetStringUTF8(
                          IDS_CHILD_BLOCK_MESSAGE_MANUAL_SINGLE_PARENT)));
        }
      }
    }
    EXPECT_THAT(result,
                testing::Not(testing::HasSubstr(l10n_util::GetStringUTF8(
                    IDS_BLOCK_INTERSTITIAL_HEADER_ACCESS_REQUESTS_DISABLED))));
    // This string is used for a button that is always present in the DOM, but
    // only visible when local web approvals is enabled.
    EXPECT_THAT(result, testing::HasSubstr(l10n_util::GetStringUTF8(
                            IDS_BLOCK_INTERSTITIAL_ASK_IN_PERSON_BUTTON)));
    EXPECT_THAT(result, testing::HasSubstr(l10n_util::GetStringUTF8(
                            IDS_BLOCK_INTERSTITIAL_ASK_IN_A_MESSAGE_BUTTON)));
    EXPECT_THAT(result, testing::HasSubstr(
                            l10n_util::GetStringUTF8(IDS_REQUEST_SENT_OK)));

  } else {
    EXPECT_THAT(result,
                testing::Not(testing::HasSubstr(l10n_util::GetStringUTF8(
                    IDS_CHILD_BLOCK_INTERSTITIAL_HEADER))));
    EXPECT_THAT(result,
                testing::Not(testing::HasSubstr(l10n_util::GetStringUTF8(
                    IDS_CHILD_BLOCK_INTERSTITIAL_MESSAGE_V2))));
    EXPECT_THAT(result,
                testing::HasSubstr(l10n_util::GetStringUTF8(
                    IDS_BLOCK_INTERSTITIAL_HEADER_ACCESS_REQUESTS_DISABLED)));
  }
    EXPECT_THAT(result,
                testing::HasSubstr(l10n_util::GetStringUTF8(
                    IDS_CHILD_BLOCK_INTERSTITIAL_WAITING_APPROVAL_MESSAGE)));
    if (param.has_two_parents) {
      EXPECT_THAT(
          result,
          testing::HasSubstr(l10n_util::GetStringUTF8(
              IDS_CHILD_BLOCK_INTERSTITIAL_WAITING_APPROVAL_DESCRIPTION_MULTI_PARENT)));
    } else {
      EXPECT_THAT(
          result,
          testing::HasSubstr(l10n_util::GetStringUTF8(
              IDS_CHILD_BLOCK_INTERSTITIAL_WAITING_APPROVAL_DESCRIPTION_SINGLE_PARENT)));
    }
}

BuildHtmlTestParameter build_html_test_parameter[] = {
    {true, "url1", "url2", "custodian", "custodian_email", "", "",
     FilteringBehaviorReason::DEFAULT, false},
    {true, "url1", "url2", "custodian", "custodian_email", "custodian2",
     "custodian2_email", FilteringBehaviorReason::DEFAULT, true},
    {false, "url1", "url2", "custodian", "custodian_email", "custodian2",
     "custodian2_email", FilteringBehaviorReason::DEFAULT, true},
    {false, "url1", "url2", "custodian", "custodian_email", "custodian2",
     "custodian2_email", FilteringBehaviorReason::DEFAULT, true},
    {true, "url1", "url2", "custodian", "custodian_email", "custodian2",
     "custodian2_email", FilteringBehaviorReason::DEFAULT, true},
    {true, "url1", "url2", "custodian", "custodian_email", "custodian2",
     "custodian2_email", FilteringBehaviorReason::ASYNC_CHECKER, true},
};

INSTANTIATE_TEST_SUITE_P(GetBlockMessageIDParameterized,
                         SupervisedUserErrorPageTest_BuildHtml,
                         ::testing::ValuesIn(build_html_test_parameter));

}  // namespace supervised_user
