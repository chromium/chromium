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
#include "build/buildflag.h"
#include "chrome/browser/ui/autofill/autofill_popup_view.h"
#include "chrome/browser/ui/autofill/mock_autofill_popup_controller.h"
#include "chrome/browser/ui/views/autofill/popup/popup_pixel_test.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_view_utils.h"
#include "chrome/browser/ui/views/autofill/popup/popup_view_views_test_api.h"
#include "components/autofill/core/browser/filling/filling_product.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/plus_addresses/core/browser/fake_plus_address_allocator.h"
#include "components/plus_addresses/core/browser/fake_plus_address_service.h"
#include "components/plus_addresses/core/browser/plus_address_test_utils.h"
#include "components/plus_addresses/core/browser/settings/fake_plus_address_setting_service.h"
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

// Creates the typical structure of credential subpopup suggestions. The result
// is not supposed to mimic the exact structure of the credential subpopup
// suggestions, but the goal is to be close enough.
std::vector<Suggestion> CreateTypicalPasswordChildSuggestions() {
  std::vector<Suggestion> suggestions;

  // Fill name fields child suggestions
  Suggestion fill_username(u"username",
                           SuggestionType::kPasswordFieldByFieldFilling);
  suggestions.push_back(std::move(fill_username));

  Suggestion fill_password(
      l10n_util::GetStringUTF16(
          IDS_PASSWORD_MANAGER_MANUAL_FALLBACK_FILL_PASSWORD_ENTRY),
      SuggestionType::kFillPassword);
  suggestions.emplace_back(std::move(fill_password));

  suggestions.emplace_back(SuggestionType::kSeparator);

  Suggestion view_password_details(
      l10n_util::GetStringUTF16(
          IDS_PASSWORD_MANAGER_MANUAL_FALLBACK_VIEW_DETAILS_ENTRY),
      SuggestionType::kViewPasswordDetails);
  suggestions.emplace_back(std::move(view_password_details));

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

  Suggestion settings(l10n_util::GetStringUTF16(IDS_AUTOFILL_MANAGE_ADDRESSES),
                      SuggestionType::kManageAddress);
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
      l10n_util::GetStringUTF16(IDS_AUTOFILL_MANAGE_PAYMENT_METHODS),
      SuggestionType::kManageCreditCard);
  settings.icon = Suggestion::Icon::kSettings;
  suggestions.push_back(std::move(settings));

  return suggestions;
}

std::vector<Suggestion> CreateLoyaltyCardSuggestions() {
  std::vector<Suggestion> suggestions;
  suggestions.emplace_back("37262999281", "Ticket Maester ",
                           Suggestion::Icon::kNoIcon,
                           SuggestionType::kLoyaltyCardEntry);
  suggestions.back().custom_icon = Suggestion::LetterMonochromeIcon(u"T");
  suggestions.emplace_back("987654321987654321", "CVS Pharmacy",
                           Suggestion::Icon::kNoIcon,
                           SuggestionType::kLoyaltyCardEntry);
  suggestions.back().custom_icon = Suggestion::LetterMonochromeIcon(u"C");
  suggestions.emplace_back(SuggestionType::kSeparator);
  suggestions.emplace_back(
      l10n_util::GetStringUTF8(IDS_AUTOFILL_MANAGE_LOYALTY_CARDS), "",
      Suggestion::Icon::kSettings, SuggestionType::kManageLoyaltyCard);
  return suggestions;
}

std::vector<Suggestion> CreatePasswordSuggestions(
    Suggestion::Acceptability acceptability =
        Suggestion::Acceptability::kAcceptable) {
  std::vector<Suggestion> suggestions;
  suggestions.emplace_back(u"Title suggestion", SuggestionType::kTitle);
  suggestions.back().acceptability = acceptability;

  suggestions.emplace_back(u"Password main text",
                           SuggestionType::kPasswordEntry);
  suggestions.back().labels = {
      {Suggestion::Text(u"example.username@gmail.com")}};
  suggestions.back().icon = Suggestion::Icon::kGlobe;
  suggestions.back().acceptability = acceptability;

  suggestions.emplace_back(autofill::SuggestionType::kSeparator);

  suggestions.emplace_back(
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_MANAGE_PASSWORDS),
      SuggestionType::kAllSavedPasswordsEntry);
  suggestions.back().icon = Suggestion::Icon::kSettings;
  suggestions.back().trailing_icon = Suggestion::Icon::kGooglePasswordManager;
  suggestions.back().acceptability = acceptability;

  return suggestions;
}

std::vector<Suggestion> CreateWebAuthnSuggestions(
    Suggestion::Acceptability acceptability =
        Suggestion::Acceptability::kAcceptable) {
  std::vector<Suggestion> suggestions;
  suggestions.push_back(Suggestion(
      "cool passkey",
      {{Suggestion::Text(
          l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_USE_GENERIC_DEVICE))}},
      Suggestion::Icon::kGlobe, SuggestionType::kWebauthnCredential));
  suggestions.back().acceptability = acceptability;

  suggestions.push_back(Suggestion(
      "coolest passkey",
      {{Suggestion::Text(l10n_util::GetStringUTF16(
          IDS_PASSWORD_MANAGER_PASSKEY_FROM_GOOGLE_PASSWORD_MANAGER))}},
      Suggestion::Icon::kGlobe, SuggestionType::kWebauthnCredential));
  suggestions.back().acceptability = acceptability;

  suggestions.emplace_back(
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_USE_DIFFERENT_PASSKEY),
      SuggestionType::kWebauthnSignInWithAnotherDevice);
  suggestions.back().acceptability = acceptability;
  suggestions.emplace_back(
      l10n_util::GetStringUTF16(
          IDS_PASSWORD_MANAGER_MANAGE_PASSWORDS_AND_PASSKEYS),
      SuggestionType::kAllSavedPasswordsEntry);
  suggestions.back().acceptability = acceptability;
  suggestions.back().icon = Suggestion::Icon::kSettings;
  suggestions.back().trailing_icon = Suggestion::Icon::kGooglePasswordManager;

  return suggestions;
}

std::vector<Suggestion> CreatePasswordAndWebAuthnSuggestions(
    Suggestion::Acceptability acceptability =
        Suggestion::Acceptability::kAcceptable) {
  std::vector<Suggestion> suggestions =
      CreatePasswordSuggestions(acceptability);
  suggestions.pop_back();
  std::vector<Suggestion> webauthn_suggestions =
      CreateWebAuthnSuggestions(acceptability);
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

class PopupViewViewsBrowsertestNewFopOn : public PopupViewViewsBrowsertestBase {
 protected:
  base::test::ScopedFeatureList feature_list_{
      features::kAutofillEnableNewFopDisplayDesktop};
};

IN_PROC_BROWSER_TEST_P(PopupViewViewsBrowsertestNewFopOn, InvokeUi_CreditCard) {
  PrepareSuggestions(CreateCreditCardSuggestions());
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(PopupViewViewsBrowsertestNewFopOn,
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

INSTANTIATE_TEST_SUITE_P(All,
                         PopupViewViewsBrowsertestNewFopOn,
                         Combine(Bool(), Bool()),
                         PopupViewViewsBrowsertestBase::GetTestSuffix);

class PopupViewViewsBrowsertestNewFopOff
    : public PopupViewViewsBrowsertestBase {
 public:
  PopupViewViewsBrowsertestNewFopOff() {
    feature_list_.InitAndDisableFeature(
        features::kAutofillEnableNewFopDisplayDesktop);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(PopupViewViewsBrowsertestNewFopOff,
                       InvokeUi_CreditCard) {
  PrepareSuggestions(CreateCreditCardSuggestions());
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(PopupViewViewsBrowsertestNewFopOff,
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

INSTANTIATE_TEST_SUITE_P(All,
                         PopupViewViewsBrowsertestNewFopOff,
                         Combine(Bool(), Bool()),
                         PopupViewViewsBrowsertestBase::GetTestSuffix);

using PopupViewViewsBrowsertest = PopupViewViewsBrowsertestBase;

IN_PROC_BROWSER_TEST_P(PopupViewViewsBrowsertest, InvokeUi_Autocomplete) {
  PrepareSuggestions(CreateAutocompleteSuggestions());
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(PopupViewViewsBrowsertest,
                       InvokeUi_Autofill_LoyaltyCards) {
  PrepareSuggestions(CreateLoyaltyCardSuggestions());
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

IN_PROC_BROWSER_TEST_P(PopupViewViewsBrowsertest, InvokeUi_Passwords) {
  PrepareSuggestions(CreatePasswordSuggestions());
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(PopupViewViewsBrowsertest,
                       InvokeUi_Passwords_ChildSuggestions) {
  PrepareSuggestions(CreateTypicalPasswordChildSuggestions());
  PrepareSelectedCell(CellIndex{0, CellType::kContent});
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(PopupViewViewsBrowsertest,
                       InvokeUi_Passwords_And_WebAuthn) {
  PrepareSuggestions(CreatePasswordAndWebAuthnSuggestions());
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(PopupViewViewsBrowsertest,
                       InvokeUi_Passwords_And_WebAuthn_Deactivated) {
  PrepareSuggestions(CreatePasswordAndWebAuthnSuggestions(
      Suggestion::Acceptability::kUnacceptableWithDeactivatedStyle));
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(PopupViewViewsBrowsertest,
                       InvokeUi_Passwords_PasswordField) {
  // An account store entry.
  std::vector<Suggestion> suggestions;
  Suggestion entry1(u"User1", SuggestionType::kAccountStoragePasswordEntry);
  entry1.main_text.is_primary = Suggestion::Text::IsPrimary(true);
  entry1.labels = {{Suggestion::Text(
      std::u16string(10, gfx::RenderText::kPasswordReplacementChar))}};
  entry1.icon = Suggestion::Icon::kGlobe;
  entry1.trailing_icon = Suggestion::Icon::kGoogle;
  suggestions.push_back(std::move(entry1));

  // A profile store entry.
  Suggestion entry2(u"User2", SuggestionType::kPasswordEntry);
  entry2.main_text.is_primary = Suggestion::Text::IsPrimary(true);
  entry2.labels = {{Suggestion::Text(
      std::u16string(6, gfx::RenderText::kPasswordReplacementChar))}};
  entry2.icon = Suggestion::Icon::kGlobe;
  entry2.trailing_icon = Suggestion::Icon::kNoIcon;
  suggestions.push_back(std::move(entry2));

  suggestions.emplace_back(SuggestionType::kSeparator);

  // The entry to open settings.
  Suggestion settings(
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_MANAGE_PASSWORDS),
      SuggestionType::kAllSavedPasswordsEntry);
  settings.icon = Suggestion::Icon::kSettings;
  settings.trailing_icon = Suggestion::Icon::kGooglePasswordManager;
  suggestions.push_back(std::move(settings));

  PrepareSuggestions(std::move(suggestions));
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(PopupViewViewsBrowsertest,
                       InvokeUi_InsecureContext_PaymentDisabled) {
  Suggestion warning(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_WARNING_INSECURE_CONNECTION),
      SuggestionType::kInsecureContextPaymentDisabledMessage);
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

using PopupViewViewsBrowsertestShowAutocompleteDeleteButton =
    PopupViewViewsBrowsertestBase;

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

class PopupViewViewsPlusAddressSuggestionBrowsertest
    : public PopupViewViewsBrowsertestBase {
 public:
  PopupViewViewsPlusAddressSuggestionBrowsertest() {
    setting_service().set_is_plus_addresses_enabled(true);
  }

 protected:
  plus_addresses::FakePlusAddressAllocator& allocator() { return allocator_; }
  plus_addresses::FakePlusAddressSettingService& setting_service() {
    return setting_service_;
  }

  std::vector<Suggestion> GetPlusAddressSuggestion(
      const std::vector<std::string>& affiliated_plus_addresses) {
    return service_.GetSuggestionsFromPlusAddresses(affiliated_plus_addresses);
  }

 private:
  autofill::test::AutofillBrowserTestEnvironment autofill_env_;

  plus_addresses::FakePlusAddressAllocator allocator_;
  plus_addresses::FakePlusAddressSettingService setting_service_;
  plus_addresses::FakePlusAddressService service_;
};

IN_PROC_BROWSER_TEST_P(PopupViewViewsPlusAddressSuggestionBrowsertest,
                       Filling) {
  setting_service().set_has_accepted_notice(true);
  PrepareSuggestions(
      GetPlusAddressSuggestion({plus_addresses::test::kFakePlusAddress}));
  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(All,
                         PopupViewViewsPlusAddressSuggestionBrowsertest,
                         Combine(Bool(), Bool()),
                         PopupViewViewsBrowsertestBase::GetTestSuffix);

}  // namespace
}  // namespace autofill
