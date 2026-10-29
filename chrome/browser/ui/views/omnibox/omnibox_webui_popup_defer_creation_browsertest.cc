// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/auto_reset.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_view.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_webui_creation.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_base.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_base_content.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/common/omnibox_features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"

// Tests when the WebUI popup that shows omnibox suggestions creates its
// content.
class OmniboxWebUIPopupCreationBrowserTest : public InProcessBrowserTest {
 public:
  // Enables the WebUI suggestion popup.
  OmniboxWebUIPopupCreationBrowserTest() {
    feature_list_.InitWithFeatures({omnibox::internal::kWebUIOmniboxPopup},
                                   {features::kWebUILocationBar});
  }

 protected:
  LocationBarView* location_bar() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->toolbar()
        ->location_bar_view();
  }

  OmniboxPopupPresenterBase* presenter() {
    OmniboxPopupView* popup_view = location_bar()->GetOmniboxPopupView();
    return popup_view ? popup_view->presenter() : nullptr;
  }

  // Runs an omnibox query whose synchronous matches open the suggestion
  // popup, as typing does.
  void TypeAndWaitForPopup() {
    OmniboxController* controller = location_bar()->GetOmniboxController();
    controller->edit_model()->SetUserText(u"foo");
    AutocompleteInput input(
        u"foo", metrics::OmniboxEventProto::BLANK,
        ChromeAutocompleteSchemeClassifier(browser()->GetProfile()));
    input.set_omit_asynchronous_matches(true);
    controller->StartAutocomplete(input);
    ASSERT_TRUE(presenter()->IsShown());
  }

  // Closes the suggestion popup by clearing the suggestions.
  void ClosePopup() {
    location_bar()->GetOmniboxController()->StopAutocomplete(
        /*clear_result=*/true);
    ASSERT_FALSE(presenter()->IsShown());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests `omnibox::kOmniboxWebUIPopupDeferCreation`, which defers creating the
// popup's content until the browser window has painted.
class OmniboxWebUIPopupDeferCreationBrowserTest
    : public OmniboxWebUIPopupCreationBrowserTest {
 public:
  OmniboxWebUIPopupDeferCreationBrowserTest() {
    feature_list_.InitAndEnableFeature(
        omnibox::kOmniboxWebUIPopupDeferCreation);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  // Follow the deferral feature rather than the on-demand creation that
  // `InProcessBrowserTest` sets up.
  base::AutoReset<bool> create_popup_webui_on_demand_ =
      omnibox::SetCreatePopupWebUIOnDemandForTesting(false);
};

// The popup creates its content without user action once the browser window
// has presented, and keeps it hidden until it opens.
IN_PROC_BROWSER_TEST_F(OmniboxWebUIPopupDeferCreationBrowserTest,
                       CreatesHiddenContentAfterFirstPresentation) {
  ASSERT_TRUE(presenter());
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return presenter()->GetWebUIContent() != nullptr; }));
  EXPECT_FALSE(presenter()->IsShown());

  TypeAndWaitForPopup();
  EXPECT_TRUE(presenter()->GetWebUIContent());
}

// Browser tests create the popup's content only when something needs it (see
// `InProcessBrowserTest`). Tests that opening the popup creates it.
class OmniboxWebUIPopupOnDemandCreationBrowserTest
    : public OmniboxWebUIPopupCreationBrowserTest {
 public:
  // Enables deferred creation to show that on-demand creation takes precedence.
  OmniboxWebUIPopupOnDemandCreationBrowserTest() {
    defer_creation_feature_.InitAndEnableFeature(
        omnibox::kOmniboxWebUIPopupDeferCreation);
  }

 private:
  base::test::ScopedFeatureList defer_creation_feature_;
  // Explicit, so that these tests don't depend on the browser test default.
  base::AutoReset<bool> on_demand_creation_ =
      omnibox::SetCreatePopupWebUIOnDemandForTesting(true);
};

// Typing before the content exists opens the popup, which creates the content
// and its widget.
IN_PROC_BROWSER_TEST_F(OmniboxWebUIPopupOnDemandCreationBrowserTest,
                       CreatesContentWhenPopupOpens) {
  ASSERT_TRUE(presenter());
  EXPECT_FALSE(presenter()->GetWebUIContent());
  EXPECT_FALSE(presenter()->GetWidget());

  TypeAndWaitForPopup();
  EXPECT_TRUE(presenter()->GetWidget());
  OmniboxPopupWebUIBaseContent* content = presenter()->GetWebUIContent();
  ASSERT_TRUE(content);
  EXPECT_TRUE(content::WaitForLoadStop(content->GetWrappedWebContents()));
}
