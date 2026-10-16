// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/device_signals_disclaimer_test_utils.h"

#include <string>
#include <string_view>

#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"

namespace device_signals_disclaimer_test {

namespace {
// Waits for `button` in the shadow root of `app` to be visible and clicks it.
testing::AssertionResult WaitForAndClickButton(
    content::WebContents* web_contents,
    std::string_view app,
    std::string_view button) {
  if (!content::WaitForLoadStop(web_contents)) {
    return testing::AssertionFailure() << "WebContents failed to load";
  }
  std::string script = content::JsReplace(R"(
    new Promise((resolve) => {
      const interval = setInterval(() => {
        const button =
            document.querySelector($1)?.shadowRoot?.querySelector($2);
        if (button && !button.hidden) {
          clearInterval(interval);
          button.click();
          resolve(true);
        }
      }, 50);
    });
  )",
                                          app, button);

  testing::AssertionResult result = content::ExecJs(web_contents, script);
  if (!result) {
    return testing::AssertionFailure()
           << "WaitForAndClickButton failed for " << app << " -> " << button
           << ": " << result.message();
  }
  return result;
}

}  // namespace

const ManagedUserProfileNoticeSelectors& GetSelectors(bool use_refreshed_ui) {
  static constexpr ManagedUserProfileNoticeSelectors kLegacySelectors = {
      .app = "managed-user-profile-notice-app",
      .disclaimer = "signals-disclaimer",
      .proceed_button = "#proceed-button",
      .cancel_button = "#cancel-button",
  };

  static constexpr ManagedUserProfileNoticeSelectors kRefreshedSelectors = {
      .app = "managed-user-profile-notice-app-refresh",
      .disclaimer = "signals-disclaimer-refresh",
      .proceed_button = "#proceedButton",
      .cancel_button = "#cancelButton",
  };

  return use_refreshed_ui ? kRefreshedSelectors : kLegacySelectors;
}

testing::AssertionResult WaitForAndClickProceedButton(
    content::WebContents* web_contents,
    const ManagedUserProfileNoticeSelectors& selectors) {
  return WaitForAndClickButton(web_contents, selectors.app,
                               selectors.proceed_button);
}

testing::AssertionResult WaitForAndClickCancelButton(
    content::WebContents* web_contents,
    const ManagedUserProfileNoticeSelectors& selectors) {
  return WaitForAndClickButton(web_contents, selectors.app,
                               selectors.cancel_button);
}

testing::AssertionResult WaitForAndClickLearnMoreLink(
    content::WebContents* web_contents,
    const ManagedUserProfileNoticeSelectors& selectors) {
  if (!content::WaitForLoadStop(web_contents)) {
    return testing::AssertionFailure() << "WebContents failed to load";
  }
  std::string script = content::JsReplace(R"(
    new Promise((resolve) => {
      const interval = setInterval(() => {
        const link = document.querySelector($1)
                         ?.shadowRoot?.querySelector($2)
                         ?.shadowRoot?.querySelector('#learnMoreLink');
        if (link && !link.hidden) {
          clearInterval(interval);
          link.click();
          resolve(true);
        }
      }, 50);
    });
  )",
                                          selectors.app, selectors.disclaimer);

  testing::AssertionResult result = content::ExecJs(web_contents, script);
  if (!result) {
    return testing::AssertionFailure()
           << "WaitForAndClickLearnMoreLink failed: " << result.message();
  }
  return result;
}

testing::AssertionResult WaitForWebContentsLoaded(
    content::WebContents* web_contents,
    const ManagedUserProfileNoticeSelectors& selectors) {
  if (!content::WaitForLoadStop(web_contents)) {
    return testing::AssertionFailure() << "WebContents failed to load";
  }
  content::WaitForCopyableViewInWebContents(web_contents);

  std::string script = content::JsReplace(R"(
    new Promise((resolve) => {
      const interval = setInterval(() => {
        const app = document.querySelector($1);
        if (app && app.shadowRoot) {
          const disclaimer = app.shadowRoot.querySelector($2);
          if (disclaimer && disclaimer.shadowRoot) {
            clearInterval(interval);
            Promise.all([
              app.updateComplete,
              disclaimer.updateComplete
            ]).then(() => resolve(true));
          }
        }
      }, 50);
    });
  )",
                                          selectors.app, selectors.disclaimer);
  testing::AssertionResult result = content::ExecJs(web_contents, script);
  if (!result) {
    return testing::AssertionFailure()
           << "WaitForWebContentsLoaded failed: " << result.message();
  }
  return result;
}

}  // namespace device_signals_disclaimer_test
