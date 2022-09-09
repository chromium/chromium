// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "chrome/browser/ui/autofill_assistant/password_change/assistant_onboarding_controller.h"
#include "chrome/browser/ui/autofill_assistant/password_change/assistant_onboarding_controller_impl.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "url/gurl.h"

constexpr char kUrl[] = "https://www.google.com/example_page";

class AssistantOnboardingControllerImplBrowserTest
    : public InProcessBrowserTest {
 public:
  AssistantOnboardingControllerImplBrowserTest() {
    model_.learn_more_url = GURL(kUrl);
  }
  ~AssistantOnboardingControllerImplBrowserTest() override = default;

  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }
  const AssistantOnboardingInformation& model() { return model_; }

 private:
  AssistantOnboardingInformation model_;
};

IN_PROC_BROWSER_TEST_F(AssistantOnboardingControllerImplBrowserTest,
                       NavigateToLearnMorePage) {
  // Create the controller here to ensure that the BrowserTest is all set up.
  std::unique_ptr<AssistantOnboardingController> controller =
      AssistantOnboardingController::Create(model(), web_contents());
  content::TestNavigationObserver navigation_observer(model().learn_more_url);
  navigation_observer.StartWatchingNewWebContents();
  controller->OnLearnMoreClicked();
  navigation_observer.Wait();
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), model().learn_more_url);
}
