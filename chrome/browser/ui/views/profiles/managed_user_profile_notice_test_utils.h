// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_MANAGED_USER_PROFILE_NOTICE_TEST_UTILS_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_MANAGED_USER_PROFILE_NOTICE_TEST_UTILS_H_

#include <string_view>

#include "testing/gtest/include/gtest/gtest.h"

namespace content {
class WebContents;
}  // namespace content

namespace managed_user_profile_notice_test {

// DOM selectors of the managed user profile notice WebUI. They differ between
// the legacy and the refreshed implementation of the screens.
//
// Instances are only meant to be obtained via `GetSelectors()`; the members
// refer to string literals with static storage duration.
struct ManagedUserProfileNoticeSelectors {
  std::string_view app;
  std::string_view disclaimer;
  std::string_view proceed_button;
  std::string_view cancel_button;
};

// Returns the selectors matching the UI variant selected by
// `use_refreshed_ui`.
const ManagedUserProfileNoticeSelectors& GetSelectors(bool use_refreshed_ui);

// Waits until the "Proceed" button is visible and clicks it.
testing::AssertionResult WaitForAndClickProceedButton(
    content::WebContents* web_contents,
    const ManagedUserProfileNoticeSelectors& selectors);

// Waits until the "Cancel" button is visible and clicks it.
testing::AssertionResult WaitForAndClickCancelButton(
    content::WebContents* web_contents,
    const ManagedUserProfileNoticeSelectors& selectors);

// Waits until the "Learn more" link is visible and clicks it.
testing::AssertionResult WaitForAndClickLearnMoreLink(
    content::WebContents* web_contents,
    const ManagedUserProfileNoticeSelectors& selectors);

// Waits for `web_contents` to finish loading and for the app and the
// `screen_selector` element in its shadow root to be rendered.
testing::AssertionResult WaitForWebContentsLoaded(
    content::WebContents* web_contents,
    std::string_view app,
    std::string_view screen_selector);

}  // namespace managed_user_profile_notice_test

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_MANAGED_USER_PROFILE_NOTICE_TEST_UTILS_H_
