// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_picker_test_base.h"

#include "base/feature_list.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/browser/ui/profiles/profile_ui_test_utils.h"
#include "chrome/browser/ui/views/profiles/profile_picker_view.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/signin/public/base/signin_switches.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test_utils.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/url_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view.h"
#include "url/gurl.h"

ProfilePickerView* WithProfilePickerTestHelpers::view() {
  return static_cast<ProfilePickerView*>(ProfilePicker::GetViewForTesting());
}

views::Widget* WithProfilePickerTestHelpers::widget() {
  return view() ? view()->GetWidget() : nullptr;
}

views::WebView* WithProfilePickerTestHelpers::web_view() {
  return ProfilePicker::GetWebViewForTesting();
}

void WithProfilePickerTestHelpers::WaitForPickerWidgetCreated() {
  profiles::testing::WaitForPickerWidgetCreated();
}

void WithProfilePickerTestHelpers::WaitForLoadStop(const GURL& url) {
  profiles::testing::WaitForPickerUrl(url);
}

void WithProfilePickerTestHelpers::WaitForLoadStop(
    const GURL& url,
    content::WebContents* target) {
  ASSERT_NE(nullptr, target);

  LOG(WARNING)
      << "DEPRECATION: Deprecated call to ambiguous WaitForLoadStop() from the "
         "test fixture. Please migrate to call content::WaitForLoadStop() when "
         "waiting for a specific WebContents instance to finish loading or "
         "profiles::testing::WaitForPickerUrl() when waiting for a specific "
         "URL to be loaded by the profile picker.";

  if (web_contents() == target) {
    LOG(WARNING) << "DEPRECATION: Using "
                    "profiles::testing::WaitForPickerLoadStop";
    profiles::testing::WaitForPickerLoadStop(url);
  } else {
    LOG(WARNING) << "DEPRECATION: Using content::WaitForLoadStop";
    content::WaitForLoadStop(target);
  }
  EXPECT_EQ(target->GetLastCommittedURL(), url);
}

void WithProfilePickerTestHelpers::WaitForPickerClosed() {
  profiles::testing::WaitForPickerClosed();
  ASSERT_FALSE(ProfilePicker::IsOpen());
}

void WithProfilePickerTestHelpers::WaitForPickerClosedAndReopenedImmediately() {
  ASSERT_TRUE(ProfilePicker::IsOpen());
  profiles::testing::WaitForPickerClosed();
  EXPECT_TRUE(ProfilePicker::IsOpen());
}

content::WebContents* WithProfilePickerTestHelpers::web_contents() {
  if (!web_view()) {
    return nullptr;
  }
  return web_view()->GetWebContents();
}

GURL WithProfilePickerTestHelpers::GetSigninChromeSyncDiceUrl() {
  signin::Flow signin_flow = signin_util::IsForceSigninEnabled()
                                 ? signin::Flow::EMBEDDED_PROMO
                                 : signin::Flow::PROMO;

  return signin::GetChromeSyncURLForDice({
      .request_dark_scheme = view()->ShouldUseDarkColors(),
      .flow = signin_flow,
  });
}

GURL WithProfilePickerTestHelpers::GetChromeReauthURL(
    const std::string& email) {
  return signin::GetChromeReauthURL(
      {.email = email,
       .continue_url = GaiaUrls::GetInstance()->blank_page_url()});
}
