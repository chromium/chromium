// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/omnibox_autofill_bubble_controller.h"

#include <memory>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/data_manager/test_personal_data_manager.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_hiding_reason.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/browser/ui/payments/payments_ui_closed_reasons.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

namespace autofill {

namespace {

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
        .WillByDefault(testing::Return(nullptr));
    ON_CALL(mock_tab_interface_, GetUnownedUserDataHost())
        .WillByDefault(testing::ReturnRef(tab_unowned_user_data_host_));

    controller_ = std::make_unique<OmniboxAutofillBubbleController>(
        mock_tab_interface_, web_contents());
  }

  void TearDown() override {
    controller_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  TestPersonalDataManager* personal_data_manager() {
    return static_cast<TestPersonalDataManager*>(
        PersonalDataManagerFactory::GetForBrowserContext(profile()));
  }

 protected:
  tabs::MockTabInterface mock_tab_interface_;
  ui::UnownedUserDataHost tab_unowned_user_data_host_;
  std::unique_ptr<OmniboxAutofillBubbleController> controller_;
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
