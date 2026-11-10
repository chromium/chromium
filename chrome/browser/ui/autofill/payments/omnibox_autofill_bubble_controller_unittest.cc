// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/omnibox_autofill_bubble_controller.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback_helpers.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/autofill/payments/omnibox_autofill_page_action_controller.h"
#include "chrome/browser/ui/page_action/test_support/mock_page_action_controller.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/data_manager/test_personal_data_manager.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_hiding_reason.h"
#include "components/autofill/core/browser/test_utils/autofill_test_util.h"
#include "components/autofill/core/browser/ui/payments/payments_ui_closed_reasons.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

namespace autofill {

namespace {

using ::testing::IsEmpty;
using ::testing::Return;
using ::testing::ReturnRef;

std::unique_ptr<KeyedService> BuildTestPersonalDataManager(
    content::BrowserContext* context) {
  auto personal_data_manager = std::make_unique<TestPersonalDataManager>();
  personal_data_manager->test_payments_data_manager()
      .SetAutofillPaymentMethodsEnabled(true);
  return personal_data_manager;
}

class OmniboxAutofillBubbleControllerTest
    : public ChromeRenderViewHostTestHarness {
 public:
  OmniboxAutofillBubbleControllerTest() = default;
  ~OmniboxAutofillBubbleControllerTest() override = default;

  TestingProfile::TestingFactories GetTestingFactories() const override {
    return {TestingProfile::TestingFactory{
        PersonalDataManagerFactory::GetInstance(),
        base::BindRepeating(&BuildTestPersonalDataManager)}};
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    NavigateAndCommit(GURL("about:blank"));

    ON_CALL(mock_tab_interface_, GetBrowserWindowInterface())
        .WillByDefault(Return(nullptr));
    ON_CALL(mock_tab_interface_, GetContents())
        .WillByDefault(Return(web_contents()));
    ON_CALL(mock_tab_interface_, GetUnownedUserDataHost())
        .WillByDefault(ReturnRef(tab_unowned_user_data_host_));
    ON_CALL(mock_tab_interface_, RegisterWillDiscardContents)
        .WillByDefault(
            [this](tabs::TabInterface::WillDiscardContentsCallback callback) {
              return will_discard_contents_callbacks_.Add(std::move(callback));
            });

    controller_ =
        std::make_unique<OmniboxAutofillBubbleController>(&mock_tab_interface_);
  }

  void TearDown() override {
    controller_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  TestPersonalDataManager* personal_data_manager() {
    return static_cast<TestPersonalDataManager*>(
        PersonalDataManagerFactory::GetForBrowserContext(profile()));
  }

  void DiscardContents() {
    will_discard_contents_callbacks_.Notify(&mock_tab_interface_,
                                            web_contents(), nullptr);
  }

 protected:
  tabs::MockTabInterface mock_tab_interface_;
  ui::UnownedUserDataHost tab_unowned_user_data_host_;
  std::unique_ptr<OmniboxAutofillBubbleController> controller_;

 private:
  base::RepeatingCallbackList<
      tabs::TabInterface::WillDiscardContentsCallback::RunType>
      will_discard_contents_callbacks_;
};

// `ShouldShowGooglePayLogo()` returns `false` for local cards.
TEST_F(OmniboxAutofillBubbleControllerTest, ShouldShowGooglePayLogo_LocalCard) {
  std::string guid = "local_card_guid";
  CreditCard local_card = test::GetCreditCard();
  local_card.set_guid(guid);
  personal_data_manager()->test_payments_data_manager().AddCreditCard(
      local_card);

  std::vector<Suggestion> suggestions = {
      Suggestion(u"Card", SuggestionType::kCreditCardEntry)};
  suggestions[0].payload = Suggestion::Guid(guid);

  controller_->Initialize(suggestions, base::DoNothing(), base::DoNothing(),
                          base::DoNothing(), base::DoNothing(),
                          base::DoNothing());

  EXPECT_FALSE(controller_->ShouldShowGooglePayLogo());
}

// `ShouldShowGooglePayLogo()` returns `true` for server cards.
TEST_F(OmniboxAutofillBubbleControllerTest,
       ShouldShowGooglePayLogo_ServerCard) {
  std::string guid = "server_card_guid";
  CreditCard server_card = test::GetMaskedServerCard();
  server_card.set_guid(guid);
  personal_data_manager()->test_payments_data_manager().AddCreditCard(
      server_card);

  std::vector<Suggestion> suggestions = {
      Suggestion(u"Card", SuggestionType::kCreditCardEntry)};
  suggestions[0].payload = Suggestion::Guid(guid);

  controller_->Initialize(suggestions, base::DoNothing(), base::DoNothing(),
                          base::DoNothing(), base::DoNothing(),
                          base::DoNothing());

  EXPECT_TRUE(controller_->ShouldShowGooglePayLogo());
}

// `ShouldShowGooglePayLogo()` remains `true` once shown, even if the card GUID
// is no longer found in PaymentsDataManager (e.g. after a card fill).
TEST_F(OmniboxAutofillBubbleControllerTest,
       ShouldShowGooglePayLogo_RemainsTrueAfterCardFillOrRemoval) {
  std::string guid = "server_card_guid";
  CreditCard server_card = test::GetMaskedServerCard();
  server_card.set_guid(guid);
  personal_data_manager()->test_payments_data_manager().AddCreditCard(
      server_card);

  std::vector<Suggestion> suggestions = {
      Suggestion(u"Card", SuggestionType::kCreditCardEntry)};
  suggestions[0].payload = Suggestion::Guid(guid);

  controller_->Initialize(suggestions, base::DoNothing(), base::DoNothing(),
                          base::DoNothing(), base::DoNothing(),
                          base::DoNothing());

  EXPECT_TRUE(controller_->ShouldShowGooglePayLogo());

  // Simulate card GUID refresh.
  personal_data_manager()->test_payments_data_manager().ClearCreditCards();

  // Logo visibility should remain true once set for the flow.
  EXPECT_TRUE(controller_->ShouldShowGooglePayLogo());

  // Re-initializing for a new flow without server cards resets logo visibility.
  controller_->Initialize(suggestions, base::DoNothing(), base::DoNothing(),
                          base::DoNothing(), base::DoNothing(),
                          base::DoNothing());
  EXPECT_FALSE(controller_->ShouldShowGooglePayLogo());
}

TEST_F(OmniboxAutofillBubbleControllerTest, OnSuggestionsShown) {
  std::vector<Suggestion> suggestions = {
      Suggestion(u"Card", SuggestionType::kCreditCardEntry)};

  base::MockRepeatingCallback<void(base::span<const Suggestion>)>
      on_suggestions_shown_callback;

  controller_->Initialize(suggestions, on_suggestions_shown_callback.Get(),
                          base::DoNothing(), base::DoNothing(),
                          base::DoNothing(), base::DoNothing());

  EXPECT_CALL(on_suggestions_shown_callback,
              Run(testing::ElementsAre(testing::Field(
                  &Suggestion::type, SuggestionType::kCreditCardEntry))));
  controller_->OnSuggestionsShown();
}

TEST_F(OmniboxAutofillBubbleControllerTest, OnSuggestionSelected) {
  std::vector<Suggestion> suggestions = {
      Suggestion(u"Card", SuggestionType::kCreditCardEntry)};

  base::MockRepeatingCallback<void(const Suggestion&)>
      did_select_suggestion_callback;

  controller_->Initialize(suggestions, base::DoNothing(), base::DoNothing(),
                          did_select_suggestion_callback.Get(),
                          base::DoNothing(), base::DoNothing());

  EXPECT_CALL(
      did_select_suggestion_callback,
      Run(testing::Field(&Suggestion::type, SuggestionType::kCreditCardEntry)));
  controller_->OnSuggestionSelected(suggestions[0]);
}

TEST_F(OmniboxAutofillBubbleControllerTest, OnSuggestionDeselected) {
  base::MockRepeatingCallback<void()> did_deselect_suggestion_callback;

  controller_->Initialize(/*suggestions=*/{}, base::DoNothing(),
                          base::DoNothing(), base::DoNothing(),
                          did_deselect_suggestion_callback.Get(),
                          base::DoNothing());

  EXPECT_CALL(did_deselect_suggestion_callback, Run());
  controller_->OnSuggestionDeselected();
}

TEST_F(OmniboxAutofillBubbleControllerTest, OnSuggestionAccepted) {
  std::vector<Suggestion> suggestions = {
      Suggestion(u"Card", SuggestionType::kCreditCardEntry)};

  base::MockRepeatingCallback<void(
      const Suggestion&, const AutofillSuggestionDelegate::SuggestionMetadata&)>
      did_accept_suggestion_callback;

  controller_->Initialize(suggestions, base::DoNothing(), base::DoNothing(),
                          base::DoNothing(), base::DoNothing(),
                          did_accept_suggestion_callback.Get());

  size_t expected_row = 2;
  EXPECT_CALL(
      did_accept_suggestion_callback,
      Run(testing::Field(&Suggestion::type, SuggestionType::kCreditCardEntry),
          testing::Property(
              &AutofillSuggestionDelegate::SuggestionMetadata::row,
              expected_row)));
  controller_->OnSuggestionAccepted(suggestions[0], expected_row);
}

struct ClosedReasonMapping {
  PaymentsUiClosedReason closed_reason;
  SuggestionHidingReason hiding_reason;
};

class OmniboxAutofillBubbleControllerClosedReasonTest
    : public OmniboxAutofillBubbleControllerTest,
      public ::testing::WithParamInterface<ClosedReasonMapping> {};

TEST_P(OmniboxAutofillBubbleControllerClosedReasonTest, OnBubbleClosed) {
  ClosedReasonMapping mapping = GetParam();

  base::MockRepeatingCallback<void(SuggestionHidingReason)>
      on_suggestions_hidden_callback;

  controller_->Initialize(/*suggestions=*/{}, base::DoNothing(),
                          on_suggestions_hidden_callback.Get(),
                          base::DoNothing(), base::DoNothing(),
                          base::DoNothing());

  EXPECT_CALL(on_suggestions_hidden_callback, Run(mapping.hiding_reason));
  controller_->OnBubbleClosed(mapping.closed_reason);
}

TEST_F(OmniboxAutofillBubbleControllerTest,
       OnBubbleClosed_SuggestionNotAccepted_CollapsesChip) {
  page_actions::MockPageActionController mock_page_action_controller;
  OmniboxAutofillPageActionController page_action_controller(
      mock_tab_interface_, mock_page_action_controller);

  EXPECT_CALL(mock_page_action_controller, Show(kActionAutofillPayment))
      .Times(1);
  EXPECT_CALL(mock_page_action_controller,
              HideSuggestionChip(kActionAutofillPayment))
      .Times(1);

  controller_->OnBubbleClosed(PaymentsUiClosedReason::kClosed);
}

TEST_F(OmniboxAutofillBubbleControllerTest,
       OnBubbleClosed_SuggestionAccepted_DoesNotCollapseChip) {
  page_actions::MockPageActionController mock_page_action_controller;
  OmniboxAutofillPageActionController page_action_controller(
      mock_tab_interface_, mock_page_action_controller);

  EXPECT_CALL(mock_page_action_controller, Show(kActionAutofillPayment))
      .Times(0);
  EXPECT_CALL(mock_page_action_controller,
              HideSuggestionChip(kActionAutofillPayment))
      .Times(0);

  controller_->OnBubbleClosed(PaymentsUiClosedReason::kAccepted);
}

// Tests that discarding tab contents clears suggestions, callbacks, and logo
// state and hides the omnibox chip.
TEST_F(OmniboxAutofillBubbleControllerTest,
       WillDiscardContents_ClearsStateAndHidesChip) {
  const std::string guid = "server_card_guid";
  CreditCard server_card = test::GetMaskedServerCard();
  server_card.set_guid(guid);
  personal_data_manager()->test_payments_data_manager().AddCreditCard(
      server_card);

  std::vector<Suggestion> suggestions = {
      Suggestion(u"Card", SuggestionType::kCreditCardEntry)};
  suggestions[0].payload = Suggestion::Guid(guid);

  base::MockRepeatingCallback<void(base::span<const Suggestion>)>
      on_suggestions_shown_callback;
  base::MockRepeatingCallback<void(SuggestionHidingReason)>
      on_suggestions_hidden_callback;
  base::MockRepeatingCallback<void(const Suggestion&)>
      did_select_suggestion_callback;
  base::MockRepeatingCallback<void()> did_deselect_suggestion_callback;
  base::MockRepeatingCallback<void(
      const Suggestion&, const AutofillSuggestionDelegate::SuggestionMetadata&)>
      did_accept_suggestion_callback;

  controller_->Initialize(suggestions, on_suggestions_shown_callback.Get(),
                          on_suggestions_hidden_callback.Get(),
                          did_select_suggestion_callback.Get(),
                          did_deselect_suggestion_callback.Get(),
                          did_accept_suggestion_callback.Get());
  ASSERT_FALSE(controller_->GetSuggestions().empty());
  ASSERT_TRUE(controller_->ShouldShowGooglePayLogo());

  page_actions::MockPageActionController mock_page_action_controller;
  OmniboxAutofillPageActionController page_action_controller(
      mock_tab_interface_, mock_page_action_controller);

  EXPECT_CALL(mock_page_action_controller,
              HideSuggestionChip(kActionAutofillPayment));
  EXPECT_CALL(mock_page_action_controller, Hide(kActionAutofillPayment));
  DiscardContents();

  EXPECT_THAT(controller_->GetSuggestions(), IsEmpty());
  EXPECT_FALSE(controller_->ShouldShowGooglePayLogo());

  EXPECT_CALL(on_suggestions_shown_callback, Run).Times(0);
  EXPECT_CALL(on_suggestions_hidden_callback, Run).Times(0);
  EXPECT_CALL(did_select_suggestion_callback, Run).Times(0);
  EXPECT_CALL(did_deselect_suggestion_callback, Run).Times(0);
  EXPECT_CALL(did_accept_suggestion_callback, Run).Times(0);
  controller_->OnSuggestionsShown();
  controller_->OnSuggestionSelected(suggestions[0]);
  controller_->OnSuggestionDeselected();
  controller_->OnSuggestionAccepted(suggestions[0], /*row=*/0);
  controller_->OnBubbleClosed(PaymentsUiClosedReason::kAccepted);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    OmniboxAutofillBubbleControllerClosedReasonTest,
    ::testing::Values(
        ClosedReasonMapping{PaymentsUiClosedReason::kAccepted,
                            SuggestionHidingReason::kAcceptSuggestion},
        ClosedReasonMapping{PaymentsUiClosedReason::kCancelled,
                            SuggestionHidingReason::kUserAborted},
        ClosedReasonMapping{PaymentsUiClosedReason::kClosed,
                            SuggestionHidingReason::kUserAborted},
        ClosedReasonMapping{PaymentsUiClosedReason::kLostFocus,
                            SuggestionHidingReason::kFocusChanged},
        ClosedReasonMapping{PaymentsUiClosedReason::kNotInteracted,
                            SuggestionHidingReason::kUserAborted},
        ClosedReasonMapping{PaymentsUiClosedReason::kUnknown,
                            SuggestionHidingReason::kUserAborted}));

}  // namespace

}  // namespace autofill
