// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/omnibox_autofill_bubble_view.h"

#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/uuid.h"
#include "chrome/browser/autofill/autofill_uitest_util.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/autofill/payments/omnibox_autofill_bubble_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/views/autofill/payments/payments_view_util.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/strings/grit/components_strings.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/view_utils.h"

namespace autofill {

namespace {

std::vector<views::Button*> GetSuggestions(views::View* view) {
  std::vector<views::Button*> buttons;
  for (views::View* child : view->children()) {
    if (auto* button = views::AsViewClass<views::Button>(child)) {
      buttons.push_back(button);
    } else {
      auto child_buttons = GetSuggestions(child);
      buttons.insert(buttons.end(), child_buttons.begin(), child_buttons.end());
    }
  }
  return buttons;
}

class OmniboxAutofillBubbleViewBrowserTest : public InProcessBrowserTest {
 public:
  OmniboxAutofillBubbleViewBrowserTest() {
    feature_list_.InitAndEnableFeature(
        features::kAutofillEnableOmniboxAutofill);
  }
  ~OmniboxAutofillBubbleViewBrowserTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    // Wait for Personal Data Manager to be fully loaded to prevent that
    // spurious notifications deceive the tests.
    WaitForPersonalDataManagerToBeLoaded(browser()->GetProfile());
  }

  PersonalDataManager* personal_data_manager() {
    return PersonalDataManagerFactory::GetForBrowserContext(
        browser()->GetProfile());
  }

  OmniboxAutofillBubbleController* GetBubbleController() {
    tabs::TabInterface* tab = browser()->GetActiveTabInterface();
    if (!tab) {
      return nullptr;
    }
    return OmniboxAutofillBubbleController::From(*tab);
  }

  OmniboxAutofillBubbleView* GetBubbleView() {
    OmniboxAutofillBubbleController* controller = GetBubbleController();
    return controller ? static_cast<OmniboxAutofillBubbleView*>(
                            controller->GetBubbleView())
                      : nullptr;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(OmniboxAutofillBubbleViewBrowserTest, ShowBubble) {
  OmniboxAutofillBubbleController* controller = GetBubbleController();
  ASSERT_TRUE(controller);

  EXPECT_EQ(controller->GetBubbleView(), nullptr);

  std::vector<Suggestion> suggestions;
  suggestions.emplace_back(u"Visa •••• 1111", SuggestionType::kCreditCardEntry);

  base::MockRepeatingCallback<void(base::span<const Suggestion>)>
      on_suggestions_shown_callback;

  controller->Initialize(suggestions, on_suggestions_shown_callback.Get(),
                         base::DoNothing(), base::DoNothing(),
                         base::DoNothing(), base::DoNothing());

  EXPECT_CALL(on_suggestions_shown_callback, Run(testing::SizeIs(1)));

  controller->QueueOrShowBubble();

  EXPECT_NE(controller->GetBubbleView(), nullptr);
}

IN_PROC_BROWSER_TEST_F(OmniboxAutofillBubbleViewBrowserTest,
                       LocalCardSuggestionDoesNotShowGPayLogo) {
  auto* controller = GetBubbleController();
  ASSERT_TRUE(controller);

  // Add local card.
  CreditCard local_card = test::GetCreditCard();
  AddTestCreditCard(browser()->GetProfile(), local_card);

  // Add local card suggestion.
  std::vector<Suggestion> suggestions;
  Suggestion suggestion(u"Visa •••• 1111", SuggestionType::kCreditCardEntry);
  suggestion.payload = Suggestion::Guid(local_card.guid());
  suggestions.emplace_back(suggestion);

  controller->Initialize(suggestions, base::DoNothing(), base::DoNothing(),
                         base::DoNothing(), base::DoNothing(),
                         base::DoNothing());

  controller->QueueOrShowBubble();

  auto* bubble_view = GetBubbleView();
  ASSERT_TRUE(bubble_view);

  // Verify one suggestion is shown.
  EXPECT_EQ(GetSuggestions(bubble_view).size(), 1u);

  // Verify the GPay logo is not shown.
  views::View* title_view = bubble_view->GetBubbleFrameView()->title();
  EXPECT_TRUE(views::AsViewClass<views::Label>(title_view));
  EXPECT_FALSE(views::AsViewClass<TitleWithIconAfterLabelView>(title_view));
}

IN_PROC_BROWSER_TEST_F(OmniboxAutofillBubbleViewBrowserTest,
                       ServerCardSuggestionsShowsGPayLogo) {
  auto* controller = GetBubbleController();
  ASSERT_TRUE(controller);

  // Add server card.
  personal_data_manager()->payments_data_manager().SetSyncingForTest(true);
  CreditCard server_card = test::GetMaskedServerCard();
  AddTestServerCreditCard(browser()->GetProfile(), server_card);
  const CreditCard* loaded_card =
      personal_data_manager()->payments_data_manager().GetCreditCardByServerId(
          server_card.server_id());
  ASSERT_TRUE(loaded_card);

  // Add server card suggestion.
  std::vector<Suggestion> suggestions;
  Suggestion suggestion(u"Visa •••• 1111", SuggestionType::kCreditCardEntry);
  suggestion.payload = Suggestion::Guid(loaded_card->guid());
  suggestions.emplace_back(suggestion);

  controller->Initialize(suggestions, base::DoNothing(), base::DoNothing(),
                         base::DoNothing(), base::DoNothing(),
                         base::DoNothing());

  controller->QueueOrShowBubble();

  auto* bubble_view = GetBubbleView();
  ASSERT_TRUE(bubble_view);

  // Verify one suggestion is shown.
  EXPECT_EQ(GetSuggestions(bubble_view).size(), 1u);

  // Verify the GPay logo is shown.
  views::View* title_view = bubble_view->GetBubbleFrameView()->title();
  EXPECT_TRUE(views::AsViewClass<TitleWithIconAfterLabelView>(title_view));
}

IN_PROC_BROWSER_TEST_F(OmniboxAutofillBubbleViewBrowserTest,
                       CloseBubbleTriggersOnSuggestionsHidden) {
  OmniboxAutofillBubbleController* controller = GetBubbleController();
  ASSERT_TRUE(controller);

  std::vector<Suggestion> suggestions;
  suggestions.emplace_back(u"Visa •••• 1111", SuggestionType::kCreditCardEntry);

  base::MockRepeatingCallback<void(SuggestionHidingReason)>
      on_suggestions_hidden_callback;

  controller->Initialize(
      suggestions, base::DoNothing(), on_suggestions_hidden_callback.Get(),
      base::DoNothing(), base::DoNothing(), base::DoNothing());

  controller->QueueOrShowBubble();

  auto* bubble_view = GetBubbleView();
  ASSERT_TRUE(bubble_view);

  EXPECT_CALL(on_suggestions_hidden_callback,
              Run(SuggestionHidingReason::kUserAborted));

  bubble_view->Hide();
}

IN_PROC_BROWSER_TEST_F(OmniboxAutofillBubbleViewBrowserTest,
                       DeselectSuggestionTriggersCallback) {
  auto* controller = GetBubbleController();
  ASSERT_TRUE(controller);

  // Add one suggestion.
  std::vector<Suggestion> suggestions;
  suggestions.emplace_back(u"Visa •••• 1111", SuggestionType::kCreditCardEntry);

  base::MockRepeatingClosure did_deselect_suggestion_callback;

  controller->Initialize(
      suggestions, base::DoNothing(), base::DoNothing(), base::DoNothing(),
      did_deselect_suggestion_callback.Get(), base::DoNothing());

  controller->QueueOrShowBubble();

  auto* bubble_view = GetBubbleView();
  ASSERT_TRUE(bubble_view);

  // Verify one suggestion is shown.
  std::vector<views::Button*> buttons = GetSuggestions(bubble_view);
  ASSERT_EQ(buttons.size(), 1u);

  // Select the suggestion.
  buttons[0]->RequestFocus();
  ASSERT_TRUE(buttons[0]->HasFocus());

  // Expect the callback to run once when the bubble is hidden.
  EXPECT_CALL(did_deselect_suggestion_callback, Run()).Times(1);

  bubble_view->Hide();
}

IN_PROC_BROWSER_TEST_F(OmniboxAutofillBubbleViewBrowserTest,
                       ActionItemUpdatedWithBubbleVisibility) {
  auto* controller = GetBubbleController();
  ASSERT_TRUE(controller);

  actions::ActionItem* action = actions::ActionManager::Get().FindAction(
      kActionAutofillPayment, browser()->GetActions()->root_action_item());
  ASSERT_NE(action, nullptr);
  EXPECT_FALSE(action->GetIsShowingBubble());

  // Initialize and show bubble.
  controller->Initialize(
      {Suggestion(u"Visa •••• 1111", SuggestionType::kCreditCardEntry)},
      base::DoNothing(), base::DoNothing(), base::DoNothing(),
      base::DoNothing(), base::DoNothing());
  controller->QueueOrShowBubble();

  auto* bubble_view = GetBubbleView();
  ASSERT_NE(bubble_view, nullptr);
  EXPECT_TRUE(action->GetIsShowingBubble());

  // Close bubble.
  bubble_view->Hide();
  EXPECT_FALSE(action->GetIsShowingBubble());
}

}  // namespace

}  // namespace autofill
