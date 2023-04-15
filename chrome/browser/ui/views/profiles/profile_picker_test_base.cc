// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_picker_test_base.h"

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/ui/profile_picker.h"
#include "chrome/browser/ui/profile_ui_test_utils.h"
#include "chrome/browser/ui/views/profiles/profile_picker_view.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/url_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"
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

void WithProfilePickerTestHelpers::WaitForLoadStop(
    const GURL& url,
    content::WebContents* target) {
  if (!target) {
    profiles::testing::WaitForPickerLoadStop(url);
    return;
  }

  content::WaitForLoadStop(target);
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
  return signin::GetChromeSyncURLForDice({
      .request_dark_scheme = view()->ShouldUseDarkColors(),
      .for_promo_flow = true,
  });
}
