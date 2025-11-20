// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/rtl.h"
#include "base/memory/weak_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_autofill_driver_injector.h"
#include "components/autofill/core/browser/filling/filling_product.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager_test_api.h"
#include "components/autofill/core/browser/metrics/autofill_settings_metrics.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/browser/ui/autofill_external_delegate.h"
#include "components/autofill/core/browser/ui/popup_open_enums.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/test/scoped_iph_feature_list.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect_f.h"
#include "url/gurl.h"

namespace autofill {

namespace {

using ::testing::Not;
using ::testing::Optional;
using ::testing::Return;

class TestAutofillExternalDelegate : public AutofillExternalDelegate {
 public:
  explicit TestAutofillExternalDelegate(
      BrowserAutofillManager* autofill_manager,
      AutofillClient* autofill_client)
      : AutofillExternalDelegate(autofill_manager), client_(*autofill_client) {}
  ~TestAutofillExternalDelegate() override = default;

  void OnSuggestionsShown(base::span<const Suggestion>) override {
    ++show_counter_;
    ui_session_id_at_last_show_ =
        client_->GetSessionIdForCurrentAutofillSuggestions();
    CHECK(ui_session_id_at_last_show_)
        << "session id should be non-empty directly after show";
  }

  FillingProduct GetMainFillingProduct() const override {
    return FillingProduct::kAutocomplete;
  }

  int show_counter() const { return show_counter_; }
  std::optional<AutofillClient::SuggestionUiSessionId>
  ui_session_id_at_last_show() const {
    return ui_session_id_at_last_show_;
  }

 private:
  const raw_ref<AutofillClient> client_;

  int show_counter_ = 0;
  std::optional<AutofillClient::SuggestionUiSessionId>
      ui_session_id_at_last_show_;
};

// This test class is needed to make the constructor public.
class TestChromeAutofillClient : public ChromeAutofillClient {
 public:
  explicit TestChromeAutofillClient(content::WebContents* web_contents)
      : ChromeAutofillClient(web_contents) {}
  ~TestChromeAutofillClient() override = default;
};

class ChromeAutofillClientBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    // `BrowserWindow::MaybeShowFeaturePromo()` doesn't work in tests unless the
    // IPH feature is explicitly enabled.
    iph_feature_list_.InitAndEnableFeatures(
        {feature_engagement::kIPHAutofillAiOptInFeature});
  }

  void SetUpOnMainThread() override {
    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GURL("https://test.com")));

    test_api(browser_autofill_manager())
        .SetExternalDelegate(std::make_unique<TestAutofillExternalDelegate>(
            &browser_autofill_manager(), client()));
  }

  TestAutofillExternalDelegate* aed() {
    return static_cast<TestAutofillExternalDelegate*>(
        test_api(browser_autofill_manager()).external_delegate());
  }

  TestChromeAutofillClient* client() {
    return autofill_client_injector_[web_contents()];
  }

  ContentAutofillDriver* driver() {
    return autofill_driver_injector_[web_contents()->GetPrimaryMainFrame()];
  }

  BrowserAutofillManager& browser_autofill_manager() {
    return static_cast<BrowserAutofillManager&>(driver()->GetAutofillManager());
  }

  content::WebContents* web_contents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  AutofillClient::SuggestionUiSessionId ShowSuggestions(
      const gfx::RectF& bounds) {
    return client()->ShowAutofillSuggestions(
        ChromeAutofillClient::PopupOpenArgs(
            bounds, base::i18n::TextDirection::LEFT_TO_RIGHT,
            {Suggestion(u"test", SuggestionType::kAutocompleteEntry)},
            AutofillSuggestionTriggerSource::kFormControlElementClicked,
            /*form_control_ax_id=*/0, PopupAnchorType::kField),
        test_api(browser_autofill_manager())
            .external_delegate()
            ->GetWeakPtrForTest());
  }

  // Waits until the suggestions have been at least once more since calling this
  // function.
  void WaitUntilSuggestionsHaveBeenShown(
      const base::Location& location = FROM_HERE) {
    EXPECT_TRUE(base::test::RunUntil([this, initial_count =
                                                suggestion_show_counter()]() {
      return suggestion_show_counter() > initial_count;
    })) << location.ToString()
        << ": Showing Autofill suggestions timed out.";
  }

  // Returns show many times the suggestions have been shown or updated.
  int suggestion_show_counter() { return aed()->show_counter(); }

  std::optional<AutofillClient::SuggestionUiSessionId>
  ui_session_id_at_last_show() {
    return aed()->ui_session_id_at_last_show();
  }

 private:
  test::AutofillBrowserTestEnvironment autofill_test_environment_;
  feature_engagement::test::ScopedIphFeatureList iph_feature_list_;
  TestAutofillClientInjector<TestChromeAutofillClient>
      autofill_client_injector_;
  TestAutofillDriverInjector<ContentAutofillDriver> autofill_driver_injector_;
};

// This test displays an autofill field IPH, and then tries to show the Autofill
// Popup on top of it (they would overlap). The expected behaviour is
// that the IPH is hidden and the Autofill Popup is successfully shown.
IN_PROC_BROWSER_TEST_F(ChromeAutofillClientBrowserTest,
                       AutofillPopupIsShownIfOverlappingWithIph) {
  FormData form = test::CreateTestAddressFormData();
  test_api(form).field(0).set_bounds(gfx::RectF(10, 10));
  client()->ShowAutofillFieldIphForFeature(
      form.fields()[0], AutofillClient::IphFeature::kAutofillAi);

  // Set the bounds such that the Autofill Popup would overlap with the IPH (the
  // IPH is displayed right below `form.fields[0]`, whose bounds are set above).
  ShowSuggestions(/*bounds=*/gfx::RectF(100, 100));
  WaitUntilSuggestionsHaveBeenShown();

  EXPECT_FALSE(
      BrowserUserEducationInterface::From(browser())->IsFeaturePromoActive(
          feature_engagement::kIPHAutofillAiOptInFeature));
}

IN_PROC_BROWSER_TEST_F(ChromeAutofillClientBrowserTest, SuggestionUiSessionId) {
  // Before a popup is showing, no identifier is returned.
  EXPECT_EQ(client()->GetSessionIdForCurrentAutofillSuggestions(),
            std::nullopt);

  // Showing suggestions leads (asynchronously) to showing a popup with the
  // identifier returned by ShowAutofillSuggestions.
  const AutofillClient::SuggestionUiSessionId first_id =
      ShowSuggestions(gfx::RectF(50, 50));
  WaitUntilSuggestionsHaveBeenShown();
  EXPECT_THAT(ui_session_id_at_last_show(), std::make_optional(first_id));

  const AutofillClient::SuggestionUiSessionId second_id =
      ShowSuggestions(gfx::RectF(60, 60));
  EXPECT_NE(first_id, second_id);
  // Since showing suggestions is asynchronous, the identifier returned by
  // ShowAutofillSuggestions can be different from the one currently showing.
  EXPECT_THAT(client()->GetSessionIdForCurrentAutofillSuggestions(),
              Not(Optional(second_id)));
  // But once the new popup has been shown, they will be the same.
  WaitUntilSuggestionsHaveBeenShown();
  EXPECT_THAT(ui_session_id_at_last_show(), Optional(second_id));

  // Updating the suggestions does not lead to a new identifier. Note that
  // updating the suggestions is synchronous.
  const int old_count = suggestion_show_counter();
  const bool is_showing_suggestions =
      client()->GetSessionIdForCurrentAutofillSuggestions().has_value();
  client()->UpdateAutofillSuggestions(
      {Suggestion(u"other text", SuggestionType::kAutocompleteEntry)},
      FillingProduct::kAutocomplete,
      AutofillSuggestionTriggerSource::kUnspecified);
  EXPECT_GT(suggestion_show_counter(), old_count);
  // It is possible that some external interaction with popup could have led
  // to the suggestions hiding. In that case, updating the suggestions should
  // be a no-op. Note that this is a "hack" to reduce flakiness for the test.
  if (is_showing_suggestions) {
    EXPECT_THAT(client()->GetSessionIdForCurrentAutofillSuggestions(),
                Optional(second_id));
  } else {
    EXPECT_EQ(client()->GetSessionIdForCurrentAutofillSuggestions(),
              std::nullopt);
  }
}

IN_PROC_BROWSER_TEST_F(ChromeAutofillClientBrowserTest,
                       ShowAutofillSettings_RecordsMetrics) {
  base::HistogramTester histogram_tester;
  client()->ShowAutofillSettings(SuggestionType::kManageAddress);
  histogram_tester.ExpectUniqueSample(
      "Autofill.AddressesSettingsPage.VisitReferrer",
      autofill_metrics::AutofillSettingsReferrer::kFillingFlowDropdown, 1);

  client()->ShowAutofillSettings(SuggestionType::kManageCreditCard);
  histogram_tester.ExpectUniqueSample(
      "Autofill.PaymentMethodsSettingsPage.VisitReferrer",
      autofill_metrics::AutofillSettingsReferrer::kFillingFlowDropdown, 1);

  client()->ShowAutofillSettings(SuggestionType::kManageIban);
  histogram_tester.ExpectUniqueSample(
      "Autofill.PaymentMethodsSettingsPage.VisitReferrer",
      autofill_metrics::AutofillSettingsReferrer::kFillingFlowDropdown, 2);
}

class ChromeAutofillClientYourSavedInfoTest
    : public ChromeAutofillClientBrowserTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    ChromeAutofillClientBrowserTest::SetUpInProcessBrowserTestFixture();
    feature_list_.InitAndEnableFeature(
        autofill::features::kYourSavedInfoSettingsPage);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ChromeAutofillClientYourSavedInfoTest,
                       ShowAutofillSettings_NavigatesToYourSavedInfo) {
  base::HistogramTester histogram_tester;
  client()->ShowAutofillSettings(SuggestionType::kManageAutofillAi);

  histogram_tester.ExpectUniqueSample(
      "Autofill.YourSavedInfoSettingsPage.VisitReferrer",
      autofill_metrics::AutofillSettingsReferrer::kFillingFlowDropdown, 1);
  content::WebContents* active_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(
      active_contents->GetVisibleURL(),
      GURL(std::string("chrome://settings/") + chrome::kYourSavedInfoSubPage));
}

}  // namespace
}  // namespace autofill
