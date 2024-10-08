// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_view_views.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "build/buildflag.h"
#include "chrome/browser/ui/autofill/autofill_popup_view.h"
#include "chrome/browser/ui/autofill/mock_autofill_popup_controller.h"
#include "chrome/browser/ui/views/autofill/popup/popup_pixel_test.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_view_utils.h"
#include "chrome/browser/ui/views/autofill/popup/popup_view_views_test_api.h"
#include "components/autofill/core/browser/filling_product.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/ui/suggestion_type.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/render_text.h"

namespace autofill {
namespace {

using ::testing::Bool;
using ::testing::Combine;
using ::testing::Return;
using CellIndex = PopupViewViews::CellIndex;
using CellType = PopupRowView::CellType;

// Creates the typical structure of a Autofill address profile children
// suggestions. This is not suppose to represent perfectly all the suggestions
// added to the submenu. but the goal is to be close enough.
std::vector<Suggestion> CreateTypicalAutofillProfileChildSuggestions() {
  std::vector<Suggestion> suggestions;

  // Fill name fields child suggestions
  Suggestion fill_full_name(
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_FILL_NAME_GROUP_POPUP_OPTION_SELECTED),
      SuggestionType::kFillFullName);
  fill_full_name.main_text.is_primary = Suggestion::Text::IsPrimary(false);
  suggestions.push_back(std::move(fill_full_name));
  suggestions.emplace_back(u"Charles",
                           SuggestionType::kAddressFieldByFieldFilling);
  suggestions.emplace_back(u"Stewart",
                           SuggestionType::kAddressFieldByFieldFilling);
  suggestions.emplace_back(SuggestionType::kSeparator);

  // Fill address fields child suggestions
  Suggestion fill_full_address(
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_FILL_ADDRESS_GROUP_POPUP_OPTION_SELECTED),
      SuggestionType::kFillFullAddress);
  fill_full_address.main_text.is_primary = Suggestion::Text::IsPrimary(false);
  suggestions.push_back(std::move(fill_full_address));
  // Also add another child suggestions layer.
  Suggestion street_address(u"123 Apple St.",
                            SuggestionType::kAddressFieldByFieldFilling);
  Suggestion street_name(u"Apple St.",
                         SuggestionType::kAddressFieldByFieldFilling);
  street_name.labels = {{Suggestion::Text(l10n_util::GetStringUTF16(
      IDS_AUTOFILL_HOUSE_NUMBER_SUGGESTION_SECONDARY_TEXT))}};
  street_address.children = {std::move(street_name)};

  suggestions.push_back(std::move(street_address));
  suggestions.emplace_back(u"Munich",
                           SuggestionType::kAddressFieldByFieldFilling);
  suggestions.emplace_back(u"8951",
                           SuggestionType::kAddressFieldByFieldFilling);
  suggestions.emplace_back(SuggestionType::kSeparator);

  Suggestion edit_profile_suggestion(
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_EDIT_ADDRESS_PROFILE_POPUP_OPTION_SELECTED),
      SuggestionType::kDeleteAddressProfile);
  edit_profile_suggestion.icon = Suggestion::Icon::kDelete;
  suggestions.push_back(std::move(edit_profile_suggestion));

  Suggestion delete_profile_suggestion(
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_DELETE_ADDRESS_PROFILE_POPUP_OPTION_SELECTED),
      SuggestionType::kDeleteAddressProfile);
  delete_profile_suggestion.icon = Suggestion::Icon::kDelete;
  suggestions.push_back(std::move(delete_profile_suggestion));

  return suggestions;
}

std::vector<Suggestion> CreateAutofillProfileSuggestions() {
  std::vector<Suggestion> suggestions;
  suggestions.emplace_back("123 Apple St.", "Charles Stewart",
                           Suggestion::Icon::kAccount,
                           SuggestionType::kAddressEntry);
  suggestions.emplace_back("3734 Elvis Presley Blvd.", "Elvis",
                           Suggestion::Icon::kAccount,
                           SuggestionType::kAddressEntry);

  suggestions.emplace_back(SuggestionType::kSeparator);

  Suggestion settings(l10n_util::GetStringUTF16(IDS_AUTOFILL_MANAGE_ADDRESSES));
  settings.type = SuggestionType::kManageAddress;
  settings.icon = Suggestion::Icon::kSettings;
  suggestions.push_back(std::move(settings));

  return suggestions;
}

std::vector<Suggestion> CreateCreditCardSuggestions() {
  std::vector<Suggestion> suggestions;
  suggestions.emplace_back("Credit card main text", "Credit card minor text",
                           Suggestion::Icon::kCardUnionPay,
                           SuggestionType::kCreditCardEntry);
  suggestions.emplace_back("Credit card main text", "Credit card minor text",
                           Suggestion::Icon::kCardVisa,
                           SuggestionType::kCreditCardEntry);
  suggestions.emplace_back(SuggestionType::kSeparator);

  Suggestion settings(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_MANAGE_PAYMENT_METHODS));
  settings.type = SuggestionType::kManageCreditCard;
  settings.icon = Suggestion::Icon::kSettings;
  suggestions.push_back(std::move(settings));

  return suggestions;
}

std::vector<Suggestion> CreatePasswordSuggestions(bool is_deactivated = false) {
  std::vector<Suggestion> suggestions;
  suggestions.emplace_back(u"Title suggestion", SuggestionType::kTitle);
  suggestions.back().apply_deactivated_style = is_deactivated;

  suggestions.emplace_back(u"Password main text",
                           SuggestionType::kPasswordEntry);
  suggestions.back().labels = {
      {Suggestion::Text(u"example.username@gmail.com")}};
  suggestions.back().icon = Suggestion::Icon::kGlobe;
  suggestions.back().apply_deactivated_style = is_deactivated;

  suggestions.emplace_back(autofill::SuggestionType::kSeparator);

  suggestions.emplace_back(
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_MANAGE_PASSWORDS),
      SuggestionType::kAllSavedPasswordsEntry);
  suggestions.back().icon = Suggestion::Icon::kSettings;
  suggestions.back().trailing_icon = Suggestion::Icon::kGooglePasswordManager;
  suggestions.back().apply_deactivated_style = is_deactivated;

  return suggestions;
}

std::vector<Suggestion> CreateWebAuthnSuggestions(bool is_deactivated = false) {
  std::vector<Suggestion> suggestions;
  suggestions.push_back(Suggestion(
      "cool passkey",
      {{Suggestion::Text(
          l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_USE_GENERIC_DEVICE))}},
      Suggestion::Icon::kGlobe, SuggestionType::kWebauthnCredential));
  suggestions.back().apply_deactivated_style = is_deactivated;

  suggestions.push_back(Suggestion(
      "coolest passkey",
      {{Suggestion::Text(l10n_util::GetStringUTF16(
          IDS_PASSWORD_MANAGER_PASSKEY_FROM_GOOGLE_PASSWORD_MANAGER))}},
      Suggestion::Icon::kGlobe, SuggestionType::kWebauthnCredential));
  suggestions.back().apply_deactivated_style = is_deactivated;

  suggestions.emplace_back(
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_USE_DIFFERENT_PASSKEY),
      SuggestionType::kWebauthnSignInWithAnotherDevice);
  suggestions.back().apply_deactivated_style = is_deactivated;
  suggestions.emplace_back(
      l10n_util::GetStringUTF16(
          IDS_PASSWORD_MANAGER_MANAGE_PASSWORDS_AND_PASSKEYS),
      SuggestionType::kAllSavedPasswordsEntry);
  suggestions.back().apply_deactivated_style = is_deactivated;
  suggestions.back().icon = Suggestion::Icon::kSettings;
  suggestions.back().trailing_icon = Suggestion::Icon::kGooglePasswordManager;

  return suggestions;
}

std::vector<Suggestion> CreatePasswordAndWebAuthnSuggestions(
    bool is_deactivated = false) {
  std::vector<Suggestion> suggestions =
      CreatePasswordSuggestions(is_deactivated);
  suggestions.pop_back();
  std::vector<Suggestion> webauthn_suggestions =
      CreateWebAuthnSuggestions(is_deactivated);
  suggestions.insert(suggestions.end(), webauthn_suggestions.begin(),
                     webauthn_suggestions.end());
  return suggestions;
}

std::vector<Suggestion> CreateAutocompleteSuggestions() {
  return {Suggestion("Autocomplete entry 1", "", Suggestion::Icon::kNoIcon,
                     SuggestionType::kAutocompleteEntry),
          Suggestion("Autocomplete entry 2", "", Suggestion::Icon::kNoIcon,
                     SuggestionType::kAutocompleteEntry)};
}

class PopupViewViewsBrowsertestBase
    : public PopupPixelTest<PopupViewViews, MockAutofillPopupController> {
 public:
  PopupViewViewsBrowsertestBase() = default;
  ~PopupViewViewsBrowsertestBase() override = default;

  void TearDownOnMainThread() override {
    if (popup_has_parent_) {
      EXPECT_CALL(controller(), ViewDestroyed);
    }

    search_bar_config_ = std::nullopt;
    popup_has_parent_ = false;
    popup_parent_.reset();
    PopupPixelTest::TearDownOnMainThread();
  }

  void PrepareSuggestions(std::vector<Suggestion> suggestions) {
    ON_CALL(controller(), GetMainFillingProduct())
        .WillByDefault([&c = controller()] {
          return GetFillingProductFromSuggestionType(c.GetSuggestionAt(0).type);
        });
    controller().set_suggestions(std::move(suggestions));
  }

  void PrepareSelectedCell(CellIndex cell) { selected_cell_ = cell; }

  void ShowUi(const std::string& name) override {
    PopupPixelTest::ShowUi(name);
    view()->Show(AutoselectFirstSuggestion(false));
    if (selected_cell_) {
      view()->SetSelectedCell(selected_cell_,
                              PopupCellSelectionSource::kNonUserInput);
    }
  }

  void ShowAndVerifyUi(bool popup_has_parent = false,
                       std::optional<AutofillPopupView::SearchBarConfig>
                           search_bar_config = std::nullopt) {
    popup_has_parent_ = popup_has_parent;
    search_bar_config_ = std::move(search_bar_config);
    PopupPixelTest::ShowAndVerifyUi();
  }

 protected:
  PopupViewViews* CreateView(MockAutofillPopupController& controller) override {
    if (popup_has_parent_) {
      popup_parent_ = std::make_unique<PopupViewViews>(controller.GetWeakPtr());
      return new PopupViewViews(controller.GetWeakPtr(),
                                test_api(*popup_parent_).GetWeakPtr(),
                                popup_parent_->GetWidget());
    }
    return new PopupViewViews(controller.GetWeakPtr(), search_bar_config_);
  }

 private:
  // The index of the selected cell. No cell is selected by default.
  std::optional<CellIndex> selected_cell_;

  // Controls whether the view is created as a sub-popup (i.e. having a parent).
  bool popup_has_parent_ = false;
  std::optional<AutofillPopupView::SearchBarConfig> search_bar_config_;
  std::unique_ptr<PopupViewViews> popup_parent_;
};

class PopupViewViewsBrowsertest : public PopupViewViewsBrowsertestBase {
 public:
  ~PopupViewViewsBrowsertest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(PopupViewViewsBrowsertest, InvokeUi_Autocomplete) {
  PrepareSuggestions(CreateAutocompleteSuggestions());
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(PopupViewViewsBrowsertest, InvokeUi_AutofillProfile) {
  PrepareSuggestions(CreateAutofillProfileSuggestions());
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(PopupViewViewsBrowsertest,
                       InvokeUi_AutofillProfile_Selected_Profile) {
  PrepareSuggestions(CreateAutofillProfileSuggestions());
  PrepareSelectedCell(CellIndex{0, CellType::kContent});
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(PopupViewViewsBrowsertest,
                       InvokeUi_AutofillProfile_Selected_Content_WithSubpoup) {
  std::vector<Suggestion> suggestions = CreateAutofillProfileSuggestions();
  suggestions[0].children = CreateAutofillProfileSuggestions();

  PrepareSuggestions(std::move(suggestions));
  PrepareSelectedCell(CellIndex{0, CellType::kContent});
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(PopupViewViewsBrowsertest,
                       InvokeUi_AutofillProfile_Selected_Control_WithSubpoup) {
  std::vector<Suggestion> suggestions = CreateAutofillProfileSuggestions();
  suggestions[0].children = CreateAutofillProfileSuggestions();

  PrepareSuggestions(std::move(suggestions));
  PrepareSelectedCell(CellIndex{0, CellType::kControl});
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(PopupViewViewsBrowsertest,
                       InvokeUi_AutofillProfile_ChildSuggestions) {
  PrepareSuggestions(CreateTypicalAutofillProfileChildSuggestions());
  PrepareSelectedCell(CellIndex{0, CellType::kContent});
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(PopupViewViewsBrowsertest,
                       InvokeUi_AutofillProfile_Selected_Footer) {
  PrepareSuggestions(CreateAutofillProfileSuggestions());
  PrepareSelectedCell(CellIndex{3, CellType::kContent});
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(PopupViewViewsBrowsertest,
                       InvokeUi_AutofillProfile_MultipleLabels) {
  std::vector<std::vector<Suggestion::Text>> labels = {
      {Suggestion::Text(
           u"Fill full address - Main Second First Third Street 123"),
       Suggestion::Text(u"Alexander Joseph Ricardo Park")},
      {Suggestion::Text(u"Fill full address"), Suggestion::Text(u"Alex Park")}};
  Suggestion suggestion("Google", std::move(labels), Suggestion::Icon::kAccount,
                        SuggestionType::kAddressEntry);
  PrepareSuggestions({suggestion});
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(PopupViewViewsBrowsertest, InvokeUi_CreditCard) {
  PrepareSuggestions(CreateCreditCardSuggestions());
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(PopupViewViewsBrowsertest, InvokeUi_Passwords) {
  PrepareSuggestions(CreatePasswordSuggestions());
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(PopupViewViewsBrowsertest,
                       InvokeUi_Passwords_And_WebAuthn) {
  PrepareSuggestions(CreatePasswordAndWebAuthnSuggestions());
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(PopupViewViewsBrowsertest,
                       InvokeUi_Passwords_And_WebAuthn_Deactivated) {
  PrepareSuggestions(
      CreatePasswordAndWebAuthnSuggestions(/*is_deactivated=*/true));
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(PopupViewViewsBrowsertest,
                       InvokeUi_CreditCard_MultipleLabels) {
  Suggestion suggestion1(
      "Visa",
      {{Suggestion::Text(u"Filling credit card - your card for payments"),
        Suggestion::Text(u"Alexander Joseph Ricardo Park")},
       {Suggestion::Text(u"Full credit card"), Suggestion::Text(u"Alex Park")}},
      Suggestion::Icon::kCardVisa, SuggestionType::kCreditCardEntry);

  // Also create a 1 label line suggestion to make sure they work well together.
  Suggestion suggestion2(
      "Visa",
      {{Suggestion::Text(u"Filling credit card - your card for payments")}},
      Suggestion::Icon::kCardVisa, SuggestionType::kCreditCardEntry);
  PrepareSuggestions({suggestion1, suggestion2});
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(PopupViewViewsBrowsertest,
                       InvokeUi_Passwords_PasswordField) {
  // An account store entry.
  std::vector<Suggestion> suggestions;
  Suggestion entry1(u"User1");
  entry1.main_text.is_primary = Suggestion::Text::IsPrimary(true);
  entry1.labels = {{Suggestion::Text(
      std::u16string(10, gfx::RenderText::kPasswordReplacementChar))}};
  entry1.type = SuggestionType::kAccountStoragePasswordEntry;
  entry1.icon = Suggestion::Icon::kGlobe;
  entry1.trailing_icon = Suggestion::Icon::kGoogle;
  suggestions.push_back(std::move(entry1));

  // A profile store entry.
  Suggestion entry2(u"User2");
  entry2.main_text.is_primary = Suggestion::Text::IsPrimary(true);
  entry2.labels = {{Suggestion::Text(
      std::u16string(6, gfx::RenderText::kPasswordReplacementChar))}};
  entry2.type = SuggestionType::kPasswordEntry;
  entry2.icon = Suggestion::Icon::kGlobe;
  entry2.trailing_icon = Suggestion::Icon::kNoIcon;
  suggestions.push_back(std::move(entry2));

  suggestions.emplace_back(SuggestionType::kSeparator);

  // The entry to open settings.
  Suggestion settings(
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_MANAGE_PASSWORDS));
  settings.type = SuggestionType::kAllSavedPasswordsEntry;
  settings.icon = Suggestion::Icon::kSettings;
  settings.trailing_icon = Suggestion::Icon::kGooglePasswordManager;
  suggestions.push_back(std::move(settings));

  PrepareSuggestions(std::move(suggestions));
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(PopupViewViewsBrowsertest,
                       InvokeUi_InsecureContext_PaymentDisabled) {
  Suggestion warning(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_WARNING_INSECURE_CONNECTION));
  warning.type = SuggestionType::kInsecureContextPaymentDisabledMessage;
  PrepareSuggestions({std::move(warning)});
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(PopupViewViewsBrowsertest,
                       NoScrollingForNonExcessiveHeightRootPopup) {
  controller().set_suggestions(
      {SuggestionType::kAddressEntry, SuggestionType::kAddressEntry,
       SuggestionType::kSeparator, SuggestionType::kManageAddress});
  ShowAndVerifyUi(/*popup_has_parent=*/false);
}

IN_PROC_BROWSER_TEST_P(PopupViewViewsBrowsertest,
                       NoScrollingForNonExcessiveHeightNonRootPopup) {
  controller().set_suggestions(
      {SuggestionType::kAddressEntry, SuggestionType::kAddressEntry,
       SuggestionType::kSeparator, SuggestionType::kManageAddress});
  ShowAndVerifyUi(/*popup_has_parent=*/true);
}

IN_PROC_BROWSER_TEST_P(PopupViewViewsBrowsertest,
                       ScrollingInRootPopupStickyFooter) {
  // Create many suggestions that don't fit the height and activate scrolling.
  std::vector<SuggestionType> suggestions(20, SuggestionType::kAddressEntry);
  suggestions.push_back(SuggestionType::kSeparator);
  suggestions.push_back(SuggestionType::kManageAddress);
  controller().set_suggestions(std::move(suggestions));
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(PopupViewViewsBrowsertest,
                       ScrollingInNonRootPopupNonStickyFooter) {
  // Create many suggestions that don't fit the height and activate scrolling.
  std::vector<SuggestionType> suggestions(20, SuggestionType::kAddressEntry);
  suggestions.push_back(SuggestionType::kSeparator);
  suggestions.push_back(SuggestionType::kManageAddress);
  controller().set_suggestions(std::move(suggestions));
  ShowAndVerifyUi(/*popup_has_parent=*/true);
}

IN_PROC_BROWSER_TEST_P(PopupViewViewsBrowsertest, SearchBarViewProvided) {
  controller().set_suggestions({SuggestionType::kAddressEntry});
  ShowAndVerifyUi(
      /*popup_has_parent=*/false,
      AutofillPopupView::SearchBarConfig{.placeholder = u"Search",
                                         .no_results_message = u""});
}

IN_PROC_BROWSER_TEST_P(PopupViewViewsBrowsertest,
                       PopupHeightIsLimitedWithSearchBarView) {
  controller().set_suggestions({100, SuggestionType::kAddressEntry});
  ShowAndVerifyUi(
      /*popup_has_parent=*/false,
      AutofillPopupView::SearchBarConfig{.placeholder = u"Search",
                                         .no_results_message = u""});
}

IN_PROC_BROWSER_TEST_P(PopupViewViewsBrowsertest,
                       SearchBarViewNoSuggestionsFound) {
  // This set imitates empty search result, it contains footer suggestions only.
  controller().set_suggestions(
      {SuggestionType::kSeparator, SuggestionType::kManageAddress});
  ON_CALL(controller(), HasFilteredOutSuggestions).WillByDefault(Return(true));
  ShowAndVerifyUi(
      /*popup_has_parent=*/false,
      AutofillPopupView::SearchBarConfig{
          .placeholder = u"Search", .no_results_message = u"No suggestions"});
}

INSTANTIATE_TEST_SUITE_P(All,
                         PopupViewViewsBrowsertest,
                         Combine(Bool(), Bool()),
                         PopupViewViewsBrowsertestBase::GetTestSuffix);

class PopupViewViewsBrowsertestShowAutocompleteDeleteButton
    : public PopupViewViewsBrowsertestBase {
 public:
  PopupViewViewsBrowsertestShowAutocompleteDeleteButton() {
    feature_list_.InitAndDisableFeature(features::kAutofillMoreProminentPopup);
  }
  ~PopupViewViewsBrowsertestShowAutocompleteDeleteButton() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(PopupViewViewsBrowsertestShowAutocompleteDeleteButton,
                       InvokeUi_Autocomplete) {
  PrepareSuggestions(CreateAutocompleteSuggestions());
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(PopupViewViewsBrowsertestShowAutocompleteDeleteButton,
                       InvokeUi_AutocompleteWith_Selected_Content) {
  PrepareSuggestions(CreateAutocompleteSuggestions());
  PrepareSelectedCell(CellIndex{1, CellType::kContent});
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(PopupViewViewsBrowsertestShowAutocompleteDeleteButton,
                       InvokeUi_Autofill_Profile_Selected_Profile) {
  PrepareSuggestions(CreateAutofillProfileSuggestions());
  PrepareSelectedCell(CellIndex{0, CellType::kContent});
  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(All,
                         PopupViewViewsBrowsertestShowAutocompleteDeleteButton,
                         Combine(Bool(), Bool()),
                         PopupViewViewsBrowsertestBase::GetTestSuffix);

}  // namespace
}  // namespace autofill
