// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_row_grouped_view.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_view_views_test.h"
#include "chrome/browser/ui/views/omnibox/omnibox_result_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_row_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/fake_autocomplete_provider.h"
#include "components/omnibox/browser/suggestion_group_util.h"
#include "components/omnibox/common/omnibox_feature_configs.h"
#include "content/public/test/browser_test.h"
#include "ui/gfx/animation/slide_animation.h"

class OmniboxRowGroupedViewBrowserTest : public OmniboxPopupViewViewsTest {
 public:
  OmniboxRowGroupedViewBrowserTest() {
    feature_list_.InitAndEnableFeature(
        omnibox_feature_configs::ContextualSearch::
            kLoadingSuggestionsAnimation);
  }

  void SetUpOnMainThread() override {
    OmniboxPopupViewViewsTest::SetUpOnMainThread();
    provider_ = new FakeAutocompleteProvider(AutocompleteProvider::TYPE_SEARCH);
    controller()->autocomplete_controller()->providers_.push_back(provider_);
  }

 protected:
  AutocompleteMatch CreateMatch(const std::u16string& description,
                                omnibox::GroupId group_id,
                                bool is_contextual_search_suggestion = false,
                                int relevance = 100) {
    AutocompleteMatch match(nullptr, relevance, false,
                            AutocompleteMatchType::SEARCH_SUGGEST);
    match.contents = description;
    match.contents_class.push_back({0, ACMatchClassification::NONE});
    match.description = description;
    match.description_class.push_back({0, ACMatchClassification::NONE});
    match.suggestion_group_id = group_id;
    match.provider = provider_.get();
    match.keyword = u"foo";
    if (is_contextual_search_suggestion) {
      match.subtypes.insert(omnibox::SuggestSubtype::SUBTYPE_CONTEXTUAL_SEARCH);
    }
    return match;
  }

  void RunTestQuery() {
    edit_model()->SetUserText(u"foo");
    AutocompleteInput input(
        u"foo", metrics::OmniboxEventProto::BLANK,
        ChromeAutocompleteSchemeClassifier(browser()->profile()));
    input.set_omit_asynchronous_matches(true);
    controller()->autocomplete_controller()->Start(input);
    ASSERT_TRUE(controller()->IsPopupOpen());
  }

  scoped_refptr<FakeAutocompleteProvider> provider_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(OmniboxRowGroupedViewBrowserTest, AnimationAndGrouping) {
  ACMatches matches;
  matches.push_back(CreateMatch(u"regular match",
                                omnibox::GroupId::GROUP_CONTEXTUAL_SEARCH,
                                /*is_contextual_search_suggestion=*/false,
                                /*relevance=*/500));
  matches.push_back(CreateMatch(u"contextual match 1",
                                omnibox::GroupId::GROUP_CONTEXTUAL_SEARCH,
                                /*is_contextual_search_suggestion=*/true));
  matches.push_back(CreateMatch(u"contextual match 2",
                                omnibox::GroupId::GROUP_CONTEXTUAL_SEARCH,
                                /*is_contextual_search_suggestion=*/true));
  provider_->matches_ = matches;

  // Open popup and check animation starts and suggestions are grouped.
  RunTestQuery();
  OmniboxRowGroupedView* group_view = popup_view()->contextual_group_view_;
  ASSERT_TRUE(group_view);
  EXPECT_TRUE(group_view->GetVisible());
  EXPECT_TRUE(group_view->animation_for_testing()->is_animating());
  EXPECT_TRUE(group_view->has_animated_for_testing());

  const int initial_height = popup_view()->GetTargetBounds().height();

  // The group view should contain 2 children for the contextual suggestions.
  ASSERT_EQ(group_view->children().size(), 2u);

  // Stop animation to continue test.
  group_view->animation_for_testing()->End();
  EXPECT_LT(initial_height, popup_view()->GetTargetBounds().height());

  // Update results and check it doesn't animate again.
  provider_->matches_.push_back(CreateMatch(
      u"another contextual match", omnibox::GroupId::GROUP_CONTEXTUAL_SEARCH,
      /*is_contextual_search_suggestion=*/true));
  popup_view()->UpdatePopupAppearance();
  EXPECT_FALSE(group_view->animation_for_testing()->is_animating());

  // Close popup and check state is reset.
  controller()->autocomplete_controller()->Stop(
      AutocompleteStopReason::kClobbered);
  EXPECT_FALSE(group_view->has_animated_for_testing());

  // Reopen popup and check it animates again.
  matches.push_back(CreateMatch(u"contextual match 3",
                                omnibox::GroupId::GROUP_CONTEXTUAL_SEARCH,
                                /*is_contextual_search_suggestion=*/true));
  provider_->matches_ = matches;
  RunTestQuery();
  group_view = popup_view()->contextual_group_view_;
  ASSERT_TRUE(group_view);
  EXPECT_TRUE(group_view->GetVisible());
  EXPECT_TRUE(group_view->animation_for_testing()->is_animating());
  EXPECT_TRUE(group_view->has_animated_for_testing());
  ASSERT_EQ(group_view->children().size(), 3u);
}
