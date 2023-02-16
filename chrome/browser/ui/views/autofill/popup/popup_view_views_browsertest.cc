// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_view_views.h"

#include <tuple>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/scoped_environment_variable_override.h"
#include "base/strings/strcat.h"
#include "chrome/browser/autofill/mock_autofill_popup_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/common/aliases.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/render_text.h"
#include "ui/views/widget/widget.h"

namespace autofill {

namespace {

using testing::NiceMock;
using testing::Return;
using CellIndex = PopupViewViews::CellIndex;
using CellType = PopupRowView::CellType;

std::vector<Suggestion> CreateAutofillProfileSuggestions() {
  std::vector<Suggestion> suggestions;
  suggestions.emplace_back("123 Apple St.", "Charles", "accountIcon", 1);
  suggestions.emplace_back("3734 Elvis Presley Blvd.", "Elvis", "accountIcon",
                           2);

  suggestions.emplace_back(POPUP_ITEM_ID_SEPARATOR);

  Suggestion settings(l10n_util::GetStringUTF16(IDS_AUTOFILL_MANAGE_ADDRESSES));
  settings.frontend_id = POPUP_ITEM_ID_AUTOFILL_OPTIONS;
  settings.icon = "settingsIcon";
  suggestions.push_back(std::move(settings));

  return suggestions;
}

}  // namespace

// If the boolean test parameter is `true`, dark mode is enforced.
class PopupViewViewsBrowsertest : public UiBrowserTest,
                                  public testing::WithParamInterface<bool> {
 public:
  PopupViewViewsBrowsertest() = default;
  ~PopupViewViewsBrowsertest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    if (GetParam()) {
      command_line->AppendSwitch(switches::kForceDarkMode);
    }
  }

  void SetUpOnMainThread() override {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    gfx::NativeView native_view = web_contents->GetNativeView();
    EXPECT_CALL(controller_, container_view())
        .WillRepeatedly(Return(native_view));
    EXPECT_CALL(controller_, GetWebContents())
        .WillRepeatedly(Return(web_contents));
  }

  void PrepareSuggestions(std::vector<Suggestion> suggestions) {
    controller_.set_suggestions(std::move(suggestions));
  }

  void PrepareSelectedCell(CellIndex cell) { selected_cell_ = cell; }

  void ShowUi(const std::string& name) override {
    EXPECT_CALL(controller_, ViewDestroyed());
    view_ = new PopupViewViews(controller_.GetWeakPtr(),
                               views::Widget::GetWidgetForNativeWindow(
                                   browser()->window()->GetNativeWindow()));
    view_->Show(AutoselectFirstSuggestion(false));
    if (selected_cell_) {
      view_->SetSelectedCell(selected_cell_);
    }
  }

  bool VerifyUi() override {
    if (!view_) {
      return false;
    }

    views::Widget* widget = view_->GetWidget();
    if (!widget) {
      return false;
    }

    // VerifyPixelUi works only for these platforms.
    // TODO(crbug.com/958242): Revise this if supported platforms change.
#if BUILDFLAG(IS_WIN) || (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
    auto* test_info = testing::UnitTest::GetInstance()->current_test_info();
    const std::string screenshot_name =
        base::StrCat({test_info->test_case_name(), "_", test_info->name()});

    return VerifyPixelUi(widget, "PopupViewViewsBrowsertest", screenshot_name);
#else
    return true;
#endif
  }

  void WaitForUserDismissal() override {
    ui_test_utils::WaitForBrowserToClose();
  }

  void TearDownOnMainThread() override { view_ = nullptr; }

 private:
  std::unique_ptr<base::ScopedEnvironmentVariableOverride> scoped_env_override_;
  NiceMock<autofill::MockAutofillPopupController> controller_;
  raw_ptr<PopupViewViews> view_ = nullptr;

  // The index of the selected cell. No cell is selected by default.
  absl::optional<CellIndex> selected_cell_;
};

IN_PROC_BROWSER_TEST_P(PopupViewViewsBrowsertest, InvokeUi_Autocomplete) {
  PrepareSuggestions({Suggestion("Autocomplete entry 1", "", "", 0),
                      Suggestion("Autocomplete entry 2", "", "", 0)});
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(PopupViewViewsBrowsertest, InvokeUi_Autofill_Profile) {
  PrepareSuggestions(CreateAutofillProfileSuggestions());
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(PopupViewViewsBrowsertest,
                       InvokeUi_Autofill_Profile_Selected_Profile) {
  PrepareSuggestions(CreateAutofillProfileSuggestions());
  PrepareSelectedCell(CellIndex{0, CellType::kContent});
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(PopupViewViewsBrowsertest,
                       InvokeUi_Autofill_Profile_Selected_Footer) {
  PrepareSuggestions(CreateAutofillProfileSuggestions());
  PrepareSelectedCell(CellIndex{3, CellType::kContent});
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
  entry1.frontend_id = POPUP_ITEM_ID_ACCOUNT_STORAGE_PASSWORD_ENTRY;
  entry1.icon = "globeIcon";
  entry1.trailing_icon = "google";
  suggestions.push_back(std::move(entry1));

  // A profile store entry.
  Suggestion entry2(u"User2");
  entry2.main_text.is_primary = Suggestion::Text::IsPrimary(true);
  entry2.additional_label =
      std::u16string(6, gfx::RenderText::kPasswordReplacementChar);
  entry2.frontend_id = POPUP_ITEM_ID_PASSWORD_ENTRY;
  entry2.icon = "globeIcon";
  entry2.trailing_icon = "";
  suggestions.push_back(std::move(entry2));

  suggestions.emplace_back(POPUP_ITEM_ID_SEPARATOR);

  // The entry to open settings.
  Suggestion settings(
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_MANAGE_PASSWORDS));
  settings.frontend_id = POPUP_ITEM_ID_ALL_SAVED_PASSWORDS_ENTRY;
  settings.icon = "settingsIcon";
  settings.trailing_icon = "googlePasswordManager";
  suggestions.push_back(std::move(settings));

  PrepareSuggestions(std::move(suggestions));
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(PopupViewViewsBrowsertest,
                       InvokeUi_InsecureContext_PaymentDisabled) {
  Suggestion warning(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_WARNING_INSECURE_CONNECTION));
  warning.frontend_id = POPUP_ITEM_ID_INSECURE_CONTEXT_PAYMENT_DISABLED_MESSAGE;
  PrepareSuggestions({std::move(warning)});
  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(All, PopupViewViewsBrowsertest, testing::Bool());

}  // namespace autofill
