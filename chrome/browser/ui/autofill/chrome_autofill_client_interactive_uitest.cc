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
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_data.h"
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

class MockAutofillExternalDelegate : public AutofillExternalDelegate {
 public:
  explicit MockAutofillExternalDelegate(
      BrowserAutofillManager* autofill_manager)
      : AutofillExternalDelegate(autofill_manager) {}
  ~MockAutofillExternalDelegate() override = default;

  MOCK_METHOD(void, OnSuggestionsShown, (), (override));
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
  form.fields[0].set_bounds(gfx::RectF(10, 10));
  client()->ShowAutofillFieldIphForManualFallbackFeature(form.fields[0]);

  auto delegate = std::make_unique<MockAutofillExternalDelegate>(
      &browser_autofill_manager());

  bool popup_shown = false;
  EXPECT_CALL(*delegate, OnSuggestionsShown).WillOnce([&] {
    popup_shown = true;
  });

  base::WeakPtr<AutofillExternalDelegate> weak_delegate =
      delegate->GetWeakPtrForTest();
  test_api(browser_autofill_manager()).SetExternalDelegate(std::move(delegate));

  // Set the bounds such that the Autofill Popup would overlap with the IPH (the
  // IPH is displayed right below `form.fields[0]`, whose bounds are set above).
  ChromeAutofillClient::PopupOpenArgs open_args(
      gfx::RectF(100, 100), base::i18n::TextDirection::LEFT_TO_RIGHT,
      {Suggestion(u"test")},
      AutofillSuggestionTriggerSource::kFormControlElementClicked,
      /*form_control_ax_id=*/0);
  client()->ShowAutofillSuggestions(open_args, weak_delegate);

  // Showing the Autofill Popup and hiding the IPH are asynchronous tasks.
  EXPECT_TRUE(base::test::RunUntil([&]() { return popup_shown; }))
      << "Showing the Autofill Popup timed out.";

  EXPECT_FALSE(chrome::FindBrowserWithTab(web_contents())
                   ->window()
                   ->IsFeaturePromoActive(
                       feature_engagement::kIPHAutofillManualFallbackFeature));
}

}  // namespace
}  // namespace autofill
