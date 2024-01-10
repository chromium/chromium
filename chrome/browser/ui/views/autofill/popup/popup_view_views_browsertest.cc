// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_view_views.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/autofill/mock_autofill_popup_controller.h"
#include "chrome/browser/ui/views/autofill/popup/popup_pixel_test.h"
#include "chrome/browser/ui/views/autofill/popup/popup_view_views_test_api.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/browser/filling_product.h"
#include "components/autofill/core/browser/ui/popup_types.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/strings/grit/components_strings.h"
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

std::vector<Suggestion> CreateAutofillProfileSuggestions() {
  std::vector<Suggestion> suggestions;
  suggestions.emplace_back("123 Apple St.", "Charles",
                           Suggestion::Icon::kAccount,
                           PopupItemId::kAddressEntry);
  suggestions.emplace_back("3734 Elvis Presley Blvd.", "Elvis",
                           Suggestion::Icon::kAccount,
                           PopupItemId::kAddressEntry);

  suggestions.emplace_back(PopupItemId::kSeparator);

  Suggestion settings(l10n_util::GetStringUTF16(IDS_AUTOFILL_MANAGE_ADDRESSES));
  settings.popup_item_id = PopupItemId::kAutofillOptions;
  settings.icon = Suggestion::Icon::kSettings;
  suggestions.push_back(std::move(settings));

  return suggestions;
}

std::vector<Suggestion> CreateAutocompleteSuggestions() {
  return {Suggestion("Autocomplete entry 1", "", Suggestion::Icon::kNoIcon,
                     PopupItemId::kAutocompleteEntry),
          Suggestion("Autocomplete entry 2", "", Suggestion::Icon::kNoIcon,
                     PopupItemId::kAutocompleteEntry)};
}

}  // namespace

class PopupViewViewsBrowsertestBase
    : public PopupPixelTest<PopupViewViews, MockAutofillPopupController> {
 public:
  PopupViewViewsBrowsertestBase() = default;
  ~PopupViewViewsBrowsertestBase() override = default;

  void TearDownOnMainThread() override {
    if (popup_has_parent_) {
      EXPECT_CALL(controller(), ViewDestroyed);
    }

    popup_has_parent_ = false;
    popup_parent_.reset();
    PopupPixelTest::TearDownOnMainThread();
  }

  // TODO(b/316859406): Remove popup type parameter.
  void PrepareSuggestions(std::vector<Suggestion> suggestions,
                          PopupType popup_type) {
    ON_CALL(controller(), GetMainFillingProduct())
        .WillByDefault([&c = controller()] {
          return GetFillingProductFromPopupItemId(
              c.GetSuggestionAt(0).popup_item_id);
        });
    ON_CALL(controller(), GetPopupType()).WillByDefault(Return(popup_type));
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

  void ShowAndVerifyUi(bool popup_has_parent = false) {
    popup_has_parent_ = popup_has_parent;
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
    return new PopupViewViews(controller.GetWeakPtr());
  }

 private:
  // The index of the selected cell. No cell is selected by default.
  std::optional<CellIndex> selected_cell_;

  // Controls whether the view is created as a sub-popup (i.e. having a parent).
  bool popup_has_parent_ = false;
  std::unique_ptr<PopupViewViews> popup_parent_;
};

class PopupViewViewsBrowsertest : public PopupViewViewsBrowsertestBase {
 public:
  PopupViewViewsBrowsertest() {
    feature_list_.InitAndDisableFeature(
        features::kAutofillShowAutocompleteDeleteButton);
  }
  ~PopupViewViewsBrowsertest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(PopupViewViewsBrowsertest, InvokeUi_Autocomplete) {
  PrepareSuggestions(CreateAutocompleteSuggestions(), PopupType::kAutocomplete);
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(PopupViewViewsBrowsertest, InvokeUi_Autofill_Profile) {
  PrepareSuggestions(CreateAutofillProfileSuggestions(), PopupType::kAddresses);
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(PopupViewViewsBrowsertest,
                       InvokeUi_Autofill_Profile_Selected_Profile) {
  PrepareSuggestions(CreateAutofillProfileSuggestions(), PopupType::kAddresses);
  PrepareSelectedCell(CellIndex{0, CellType::kContent});
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(PopupViewViewsBrowsertest,
                       InvokeUi_Autofill_Profile_Selected_Content_WithSubpoup) {
  std::vector<Suggestion> suggestions = CreateAutofillProfileSuggestions();
  suggestions[0].children = CreateAutofillProfileSuggestions();

  PrepareSuggestions(std::move(suggestions), PopupType::kAddresses);
  PrepareSelectedCell(CellIndex{0, CellType::kContent});
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(PopupViewViewsBrowsertest,
                       InvokeUi_Autofill_Profile_Selected_Control_WithSubpoup) {
  std::vector<Suggestion> suggestions = CreateAutofillProfileSuggestions();
  suggestions[0].children = CreateAutofillProfileSuggestions();

  PrepareSuggestions(std::move(suggestions), PopupType::kAddresses);
  PrepareSelectedCell(CellIndex{0, CellType::kControl});
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(PopupViewViewsBrowsertest,
                       InvokeUi_Autofill_Profile_Selected_Footer) {
  PrepareSuggestions(CreateAutofillProfileSuggestions(), PopupType::kAddresses);
  PrepareSelectedCell(CellIndex{3, CellType::kContent});
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(PopupViewViewsBrowsertest,
                       InvokeUi_Autofill_MultipleLabels) {
  std::vector<std::vector<Suggestion::Text>> labels = {
      {Suggestion::Text(
           u"Fill full address - Main Second First Third Street 123"),
       Suggestion::Text(u"Alexander Joseph Ricardo Park")},
      {Suggestion::Text(u"Fill full address"), Suggestion::Text(u"Alex Park")}};
  Suggestion suggestion("Google", std::move(labels), Suggestion::Icon::kAccount,
                        PopupItemId::kAddressEntry);
  PrepareSuggestions({suggestion}, PopupType::kAddresses);
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(PopupViewViewsBrowsertest,
                       InvokeUi_Passwords_PasswordField) {
  // An account store entry.
  std::vector<Suggestion> suggestions;
  Suggestion entry1(u"User1");
  entry1.main_text.is_primary = Suggestion::Text::IsPrimary(true);
  entry1.additional_label =
      std::u16string(10, gfx::RenderText::kPasswordReplacementChar);
  entry1.popup_item_id = PopupItemId::kAccountStoragePasswordEntry;
  entry1.icon = Suggestion::Icon::kGlobe;
  entry1.trailing_icon = Suggestion::Icon::kGoogle;
  suggestions.push_back(std::move(entry1));

  // A profile store entry.
  Suggestion entry2(u"User2");
  entry2.main_text.is_primary = Suggestion::Text::IsPrimary(true);
  entry2.additional_label =
      std::u16string(6, gfx::RenderText::kPasswordReplacementChar);
  entry2.popup_item_id = PopupItemId::kPasswordEntry;
  entry2.icon = Suggestion::Icon::kGlobe;
  entry2.trailing_icon = Suggestion::Icon::kNoIcon;
  suggestions.push_back(std::move(entry2));

  suggestions.emplace_back(PopupItemId::kSeparator);

  // The entry to open settings.
  Suggestion settings(
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_MANAGE_PASSWORDS));
  settings.popup_item_id = PopupItemId::kAllSavedPasswordsEntry;
  settings.icon = Suggestion::Icon::kSettings;
  settings.trailing_icon = Suggestion::Icon::kGooglePasswordManager;
  suggestions.push_back(std::move(settings));

  PrepareSuggestions(std::move(suggestions), PopupType::kPasswords);
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(PopupViewViewsBrowsertest,
                       InvokeUi_InsecureContext_PaymentDisabled) {
  Suggestion warning(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_WARNING_INSECURE_CONNECTION));
  warning.popup_item_id = PopupItemId::kInsecureContextPaymentDisabledMessage;
  PrepareSuggestions({std::move(warning)}, PopupType::kUnspecified);
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(PopupViewViewsBrowsertest,
                       ScrollingInRootPopupStickyFooter) {
  // Create many suggestions that don't fit the height and activate scrolling.
  std::vector<PopupItemId> suggestions(20, PopupItemId::kAddressEntry);
  suggestions.push_back(PopupItemId::kSeparator);
  suggestions.push_back(PopupItemId::kAutofillOptions);
  controller().set_suggestions(std::move(suggestions));
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(PopupViewViewsBrowsertest,
                       ScrollingInNonRootPopupNonStickyFooter) {
  // Create many suggestions that don't fit the height and activate scrolling.
  std::vector<PopupItemId> suggestions(20, PopupItemId::kAddressEntry);
  suggestions.push_back(PopupItemId::kSeparator);
  suggestions.push_back(PopupItemId::kAutofillOptions);
  controller().set_suggestions(std::move(suggestions));
  ShowAndVerifyUi(/*popup_has_parent=*/true);
}

INSTANTIATE_TEST_SUITE_P(All,
                         PopupViewViewsBrowsertest,
                         Combine(Bool(), Bool()),
                         PopupViewViewsBrowsertestBase::GetTestSuffix);

class PopupViewViewsBrowsertestShowAutocompleteDeleteButton
    : public PopupViewViewsBrowsertestBase {
 public:
  PopupViewViewsBrowsertestShowAutocompleteDeleteButton() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kAutofillShowAutocompleteDeleteButton},
        /*disabled_features=*/{features::kAutofillMoreProminentPopup});
  }
  ~PopupViewViewsBrowsertestShowAutocompleteDeleteButton() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(PopupViewViewsBrowsertestShowAutocompleteDeleteButton,
                       InvokeUi_Autocomplete) {
  PrepareSuggestions(CreateAutocompleteSuggestions(), PopupType::kAutocomplete);
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(PopupViewViewsBrowsertestShowAutocompleteDeleteButton,
                       InvokeUi_AutocompleteWith_Selected_Content) {
  PrepareSuggestions(CreateAutocompleteSuggestions(), PopupType::kAutocomplete);
  PrepareSelectedCell(CellIndex{1, CellType::kContent});
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(PopupViewViewsBrowsertestShowAutocompleteDeleteButton,
                       InvokeUi_Autofill_Profile_Selected_Profile) {
  PrepareSuggestions(CreateAutofillProfileSuggestions(), PopupType::kAddresses);
  PrepareSelectedCell(CellIndex{0, CellType::kContent});
  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(All,
                         PopupViewViewsBrowsertestShowAutocompleteDeleteButton,
                         Combine(Bool(), Bool()),
                         PopupViewViewsBrowsertestBase::GetTestSuffix);

}  // namespace autofill
