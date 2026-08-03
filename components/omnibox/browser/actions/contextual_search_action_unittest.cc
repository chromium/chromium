// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/actions/contextual_search_action.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/branding_buildflags.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/omnibox/browser/actions/omnibox_action.h"
#include "components/omnibox/browser/actions/omnibox_action_concepts.h"
#include "components/omnibox/browser/mock_autocomplete_provider_client.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/omnibox/common/omnibox_feature_configs.h"
#include "components/vector_icons/vector_icons.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_features.h"

class ContextualSearchActionTest : public testing::Test {
 public:
  ContextualSearchActionTest() = default;
};

TEST_F(ContextualSearchActionTest, RecordActionShown) {
  std::vector<std::pair<scoped_refptr<OmniboxAction>, std::string>> test_cases =
      {{base::MakeRefCounted<ContextualSearchOpenLensAction>(),
        "ContextualSearchOpenLensAction"}};

  for (const auto& entry : test_cases) {
    {
      SCOPED_TRACE(entry.second + ", shown but not used");
      base::HistogramTester histograms;
      entry.first->RecordActionShown(1, false);
      histograms.ExpectUniqueSample("Omnibox." + entry.second + ".Ctr", false,
                                    1);
    }
    {
      SCOPED_TRACE(entry.second + ", shown and used");
      base::HistogramTester histograms;
      entry.first->RecordActionShown(1, true);
      histograms.ExpectUniqueSample("Omnibox." + entry.second + ".Ctr", true,
                                    1);
    }
  }
}

TEST_F(ContextualSearchActionTest, Execute_RoutesToCoBrowse) {
  using ::testing::_;
  using ::testing::Return;

  MockAutocompleteProviderClient client;
  OmniboxAction::ExecutionContext context(
      client, OmniboxAction::ExecutionContext::OpenUrlCallback(),
      base::TimeTicks(), WindowOpenDisposition::IGNORE_ACTION);

  auto action = base::MakeRefCounted<ContextualSearchOpenLensAction>();

  // Case 1: ShouldOpenCoBrowsePanel is true -> Opens CoBrowse, bypasses Lens
  EXPECT_CALL(client, ShouldOpenComposeboxForAskG())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(client, ShouldOpenCoBrowsePanel()).WillOnce(Return(true));
  EXPECT_CALL(client, OpenCoBrowsePanel()).Times(1);
  EXPECT_CALL(client, OpenLensOverlay(_)).Times(0);
  action->Execute(context);

  testing::Mock::VerifyAndClearExpectations(&client);

  // Case 2: ShouldOpenCoBrowsePanel is false -> Opens Lens Overlay, bypasses
  // CoBrowse
  EXPECT_CALL(client, ShouldOpenComposeboxForAskG())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(client, ShouldOpenCoBrowsePanel()).WillOnce(Return(false));
  EXPECT_CALL(client, OpenCoBrowsePanel()).Times(0);
  EXPECT_CALL(client, OpenLensOverlay(true)).Times(1);
  action->Execute(context);
}

TEST_F(ContextualSearchActionTest, Execute_RoutesToComposeBoxForAskG) {
  using ::testing::_;
  using ::testing::Return;

  MockAutocompleteProviderClient client;
  OmniboxAction::ExecutionContext context(
      client, OmniboxAction::ExecutionContext::OpenUrlCallback(),
      base::TimeTicks(), WindowOpenDisposition::IGNORE_ACTION);

  auto action = base::MakeRefCounted<ContextualSearchOpenLensAction>();

  // Case 1: ShouldOpenComposeboxForAskG is true -> Opens Composebox, bypasses
  // CoBrowse and Lens
  EXPECT_CALL(client, ShouldOpenComposeboxForAskG()).WillOnce(Return(true));
  EXPECT_CALL(client, OpenComposeboxForAskG()).Times(1);
  EXPECT_CALL(client, OpenCoBrowsePanel()).Times(0);
  EXPECT_CALL(client, OpenLensOverlay(_)).Times(0);
  action->Execute(context);

  testing::Mock::VerifyAndClearExpectations(&client);

  // Case 2: ShouldOpenComposeboxForAskG is false -> falls back to checking
  // CoBrowse/Lens (handled by existing test)
}

#if defined(SUPPORT_PEDALS_VECTOR_ICONS)
TEST_F(ContextualSearchActionTest, GetVectorIcon) {
  auto open_lens_action = base::MakeRefCounted<ContextualSearchOpenLensAction>();
  auto fulfillment_action = base::MakeRefCounted<ContextualSearchFulfillmentAction>(
      GURL("https://google.com"), AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED,
      /*is_zero_prefix_suggestion=*/false);


  base::test::ScopedFeatureList scoped_feature_list;

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Case 1: Tweaks enabled -> Should return kGoogleLensLogoIcon
  {
    scoped_feature_list.Reset();
    omnibox_feature_configs::ScopedConfigForTesting<
        omnibox_feature_configs::ContextualSearch>
        scoped_config;
    scoped_config.Get().open_lens_action_ui_tweaks = true;

    EXPECT_EQ(&open_lens_action->GetVectorIcon(),
              &vector_icons::kGoogleLensLogoIcon);
  }

  // Case 2a: Tweaks disabled, AskG flag enabled, SwapIcon enabled -> Should
  // return kSearchSparkIcon (if rounded icons enabled) or kSearchSparkOldIcon (if disabled)
  {
    // Sub-case: Rounded icons enabled
    {
      scoped_feature_list.Reset();
      scoped_feature_list.InitWithFeaturesAndParameters(
          /*enabled_features=*/
          {{omnibox::kWebUIOmniboxAskGAboutThisPage,
            {{"Omnibox_AskGSwapIcon", "true"}}},
           {features::kRoundedIcons, {}}},
          /*disabled_features=*/{});
      omnibox_feature_configs::ScopedConfigForTesting<
          omnibox_feature_configs::ContextualSearch>
          scoped_config;
      scoped_config.Get().open_lens_action_ui_tweaks = false;

      EXPECT_EQ(&open_lens_action->GetVectorIcon(), &omnibox::kSearchSparkIcon);
    }

    // Sub-case: Rounded icons disabled
    {
      scoped_feature_list.Reset();
      scoped_feature_list.InitWithFeaturesAndParameters(
          /*enabled_features=*/
          {{omnibox::kWebUIOmniboxAskGAboutThisPage,
            {{"Omnibox_AskGSwapIcon", "true"}}}},
          /*disabled_features=*/{features::kRoundedIcons});
      omnibox_feature_configs::ScopedConfigForTesting<
          omnibox_feature_configs::ContextualSearch>
          scoped_config;
      scoped_config.Get().open_lens_action_ui_tweaks = false;

      EXPECT_EQ(&open_lens_action->GetVectorIcon(),
                &omnibox::kSearchSparkOldIcon);
    }
  }

  // Case 2b: Tweaks disabled, AskG flag enabled, SwapIcon disabled (default) -> Should return kGoogleLensMonochromeLogoIcon
  {
    scoped_feature_list.Reset();
    scoped_feature_list.InitAndEnableFeature(
        omnibox::kWebUIOmniboxAskGAboutThisPage);
    omnibox_feature_configs::ScopedConfigForTesting<
        omnibox_feature_configs::ContextualSearch>
        scoped_config;
    scoped_config.Get().open_lens_action_ui_tweaks = false;

    EXPECT_EQ(&open_lens_action->GetVectorIcon(),
              &vector_icons::kGoogleLensMonochromeLogoIcon);
  }


  // Case 3: Both disabled -> Should return kGoogleLensMonochromeLogoIcon
  {
    scoped_feature_list.Reset();
    scoped_feature_list.InitAndDisableFeature(
        omnibox::kWebUIOmniboxAskGAboutThisPage);
    omnibox_feature_configs::ScopedConfigForTesting<
        omnibox_feature_configs::ContextualSearch>
        scoped_config;
    scoped_config.Get().open_lens_action_ui_tweaks = false;

    EXPECT_EQ(&open_lens_action->GetVectorIcon(),
              &vector_icons::kGoogleLensMonochromeLogoIcon);
  }
#else
  // Non-branded builds should return search icons.
  {
    scoped_feature_list.Reset();
    scoped_feature_list.InitAndEnableFeature(features::kRoundedIcons);
    EXPECT_EQ(&open_lens_action->GetVectorIcon(), &vector_icons::kSearchIcon);
    EXPECT_EQ(&fulfillment_action->GetVectorIcon(), &vector_icons::kSearchIcon);
  }
  {
    scoped_feature_list.Reset();
    scoped_feature_list.InitAndDisableFeature(features::kRoundedIcons);
    EXPECT_EQ(&open_lens_action->GetVectorIcon(),
              &vector_icons::kSearchChromeRefreshOldIcon);
    EXPECT_EQ(&fulfillment_action->GetVectorIcon(),
              &vector_icons::kSearchChromeRefreshOldIcon);
  }
#endif
}
#endif
