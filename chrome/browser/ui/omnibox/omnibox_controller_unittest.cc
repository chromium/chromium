// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_controller.h"

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_state_manager.h"
#include "chrome/browser/ui/omnibox/test_omnibox_view.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/fake_autocomplete_controller.h"
#include "components/omnibox/browser/test_omnibox_client.h"
#include "components/open_from_clipboard/fake_clipboard_recent_content.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
class OmniboxControllerTest : public testing::Test {
 protected:
  OmniboxControllerTest() {
    // AutocompleteController needs an instance of ClipboardRecentContent to
    // create the ClipboardProvider. The instance can be nullptr in iOS tests.
    ClipboardRecentContent::SetInstance(
        std::make_unique<FakeClipboardRecentContent>());

    omnibox_controller_ = std::make_unique<OmniboxController>(
        std::make_unique<TestOmniboxClient>());
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<OmniboxController> omnibox_controller_;
};

// Tests that the list of autocomplete providers created by the
// OmniboxController matches the expectations.
TEST_F(OmniboxControllerTest, CheckDefaultAutocompleteProviders) {
  int expected_providers = AutocompleteClassifier::DefaultOmniboxProviders();

  for (const auto& provider :
       omnibox_controller_->autocomplete_controller()->providers()) {
    // Ensure this is a provider we wanted.
    int type = static_cast<int>(provider->type());
    ASSERT_TRUE(expected_providers & type);
    // Remove it from expectations so we fail if it's there twice.
    expected_providers &= ~type;
  }

  // Ensure we saw all the providers we expected.
  ASSERT_EQ(0, expected_providers);
}

class OmniboxControllerPopupVisibilityTest : public testing::Test {
 protected:
  OmniboxControllerPopupVisibilityTest() {
    ClipboardRecentContent::SetInstance(
        std::make_unique<FakeClipboardRecentContent>());
    auto client = std::make_unique<TestOmniboxClient>();
    ON_CALL(*client, OnPopupVisibilityChanged)
        .WillByDefault([this](bool popup_is_open) {
          reported_visibility_.push_back(popup_is_open);
        });
    controller_ = std::make_unique<OmniboxController>(std::move(client));
    auto autocomplete_controller =
        std::make_unique<FakeAutocompleteController>(&task_environment_);
    autocomplete_controller_ = autocomplete_controller.get();
    controller_->SetAutocompleteControllerForTesting(
        std::move(autocomplete_controller));
    // Like the location bar's omnibox view, this makes the controller observe
    // autocomplete results.
    view_ = std::make_unique<TestOmniboxView>(controller_.get());
  }

  void SetPopupState(OmniboxPopupState state) {
    controller_->popup_state_manager()->SetPopupState(state);
  }

  void ShowSuggestions() {
    AutocompleteMatch match(/*provider=*/nullptr, /*relevance=*/1000,
                            /*deletable=*/false,
                            AutocompleteMatch::Type::kSearchSuggest);
    match.destination_url = GURL("https://example.com/");
    match.keyword = u"test";
    match.allowed_to_be_default_match = true;
    match.contents = u"test";
    match.contents_class = {{0, 0}};
    autocomplete_controller_->SimulateCleanAutocompletePass({match});
  }

  // Clears the results the way the Full popup does, e.g. on a tab switch.
  void ClearSuggestions() {
    controller_->StopAutocomplete(/*clear_result=*/true);
    autocomplete_controller_->ExpectOnResultChanged(
        0, AutocompleteController::UpdateType::kStop);
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::vector<bool> reported_visibility_;
  std::unique_ptr<OmniboxController> controller_;
  raw_ptr<FakeAutocompleteController> autocomplete_controller_ = nullptr;
  std::unique_ptr<TestOmniboxView> view_;
};

// The Full popup is open whenever the omnibox has focus, so it only counts as
// visible while it shows suggestions.
TEST_F(OmniboxControllerPopupVisibilityTest,
       FullPopupIsVisibleOnlyWithSuggestions) {
  SetPopupState(OmniboxPopupState::kFull);
  EXPECT_THAT(reported_visibility_, testing::IsEmpty());

  ShowSuggestions();
  EXPECT_THAT(reported_visibility_, testing::ElementsAre(true));

  // New results while suggestions show aren't reported again.
  ShowSuggestions();
  EXPECT_THAT(reported_visibility_, testing::ElementsAre(true));

  ClearSuggestions();
  EXPECT_THAT(reported_visibility_, testing::ElementsAre(true, false));

  SetPopupState(OmniboxPopupState::kNone);
  EXPECT_THAT(reported_visibility_, testing::ElementsAre(true, false));
}

// Closing the Full popup while it shows suggestions, e.g. on blur, reports it
// hidden even though the results remain.
TEST_F(OmniboxControllerPopupVisibilityTest,
       ClosingFullPopupWithSuggestionsReportsHidden) {
  SetPopupState(OmniboxPopupState::kFull);
  ShowSuggestions();
  SetPopupState(OmniboxPopupState::kNone);
  EXPECT_THAT(reported_visibility_, testing::ElementsAre(true, false));
}

// Results alone don't make a closed popup visible.
TEST_F(OmniboxControllerPopupVisibilityTest,
       SuggestionsWithClosedPopupAreNotVisible) {
  ShowSuggestions();
  ClearSuggestions();
  EXPECT_THAT(reported_visibility_, testing::IsEmpty());
}

TEST_F(OmniboxControllerPopupVisibilityTest, AiModeIsVisible) {
  SetPopupState(OmniboxPopupState::kAim);
  EXPECT_THAT(reported_visibility_, testing::ElementsAre(true));

  SetPopupState(OmniboxPopupState::kNone);
  EXPECT_THAT(reported_visibility_, testing::ElementsAre(true, false));
}

// AI Mode replacing open suggestions keeps the popup visible, and closing AI
// Mode then reports it hidden.
TEST_F(OmniboxControllerPopupVisibilityTest,
       AiModeReplacingSuggestionsThenClosingReportsHidden) {
  ShowSuggestions();
  SetPopupState(OmniboxPopupState::kClassic);
  EXPECT_THAT(reported_visibility_, testing::ElementsAre(true));

  SetPopupState(OmniboxPopupState::kAim);
  EXPECT_THAT(reported_visibility_, testing::ElementsAre(true));

  SetPopupState(OmniboxPopupState::kNone);
  EXPECT_THAT(reported_visibility_, testing::ElementsAre(true, false));
}

// The classic popup only opens with results, and is reported on open and
// close.
TEST_F(OmniboxControllerPopupVisibilityTest, ClassicPopupIsVisible) {
  ShowSuggestions();
  SetPopupState(OmniboxPopupState::kClassic);
  EXPECT_THAT(reported_visibility_, testing::ElementsAre(true));

  ClearSuggestions();
  SetPopupState(OmniboxPopupState::kNone);
  EXPECT_THAT(reported_visibility_, testing::ElementsAre(true, false));
}
