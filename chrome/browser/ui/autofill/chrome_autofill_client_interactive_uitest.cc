// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/rtl.h"
#include "base/memory/weak_ptr.h"
#include "base/test/run_until.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_autofill_driver_injector.h"
#include "components/autofill/core/browser/autofill_external_delegate.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/browser_autofill_manager_test_api.h"
#include "components/autofill/core/browser/filling_product.h"
#include "components/autofill/core/browser/ui/popup_open_enums.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/common/aliases.h"
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
      BrowserAutofillManager* autofill_manager)
      : AutofillExternalDelegate(autofill_manager) {}
  ~TestAutofillExternalDelegate() override = default;

  void OnSuggestionsShown(base::span<const Suggestion>) override {
    ++show_counter_;
  }

  FillingProduct GetMainFillingProduct() const override {
    return FillingProduct::kAutocomplete;
  }

  int show_counter() const { return show_counter_; }

 private:
  int show_counter_ = 0;
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
        {feature_engagement::kIPHAutofillManualFallbackFeature});
  }

  void SetUpOnMainThread() override {
    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GURL("http://test.com")));

    test_api(browser_autofill_manager())
        .SetExternalDelegate(std::make_unique<TestAutofillExternalDelegate>(
            &browser_autofill_manager()));
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
            {Suggestion(u"test")},
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
  int suggestion_show_counter() {
    return static_cast<TestAutofillExternalDelegate*>(
               test_api(browser_autofill_manager()).external_delegate())
        ->show_counter();
  }

 private:
  test::AutofillBrowserTestEnvironment autofill_test_environment_;
  feature_engagement::test::ScopedIphFeatureList iph_feature_list_;
  TestAutofillClientInjector<TestChromeAutofillClient>
      autofill_client_injector_;
  TestAutofillDriverInjector<ContentAutofillDriver> autofill_driver_injector_;
};

// This test displays a manual fallback IPH, and then tries to show the Autofill
// Popup on top of it (they would overlap). The expected behaviour is
// that the IPH is hidden and the Autofill Popup is successfully shown.
IN_PROC_BROWSER_TEST_F(ChromeAutofillClientBrowserTest,
                       AutofillPopupIsShownIfOverlappingWithIph) {
  FormData form = test::CreateTestAddressFormData();
  test_api(form).field(0).set_bounds(gfx::RectF(10, 10));
  client()->ShowAutofillFieldIphForFeature(
      form.fields()[0], AutofillClient::IphFeature::kManualFallback);

  // Set the bounds such that the Autofill Popup would overlap with the IPH (the
  // IPH is displayed right below `form.fields[0]`, whose bounds are set above).
  ShowSuggestions(/*bounds=*/gfx::RectF(100, 100));
  WaitUntilSuggestionsHaveBeenShown();

  EXPECT_FALSE(chrome::FindBrowserWithTab(web_contents())
                   ->window()
                   ->IsFeaturePromoActive(
                       feature_engagement::kIPHAutofillManualFallbackFeature));
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
  EXPECT_THAT(client()->GetSessionIdForCurrentAutofillSuggestions(),
              std::make_optional(first_id));

  const AutofillClient::SuggestionUiSessionId second_id =
      ShowSuggestions(gfx::RectF(60, 60));
  EXPECT_NE(first_id, second_id);
  // Since showing suggestions is asynchronous, the identifier returned by
  // ShowAutofillSuggestions can be different from the one currently showing.
  EXPECT_THAT(client()->GetSessionIdForCurrentAutofillSuggestions(),
              Not(Optional(second_id)));
  // But once the new popup has been shown, they will be the same.
  WaitUntilSuggestionsHaveBeenShown();
  EXPECT_THAT(client()->GetSessionIdForCurrentAutofillSuggestions(),
              Optional(second_id));

  // Updating the suggestions does not lead to a new identifier. Note that
  // updating the suggestions is synchronous.
  const int old_count = suggestion_show_counter();
  client()->UpdateAutofillSuggestions(
      {Suggestion(u"other text")}, FillingProduct::kAutocomplete,
      AutofillSuggestionTriggerSource::kUnspecified);
  EXPECT_GT(suggestion_show_counter(), old_count);
  EXPECT_THAT(client()->GetSessionIdForCurrentAutofillSuggestions(),
              Optional(second_id));
}

}  // namespace
}  // namespace autofill
