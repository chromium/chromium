// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_picker_test_base.h"

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/ui/profile_picker.h"
#include "chrome/browser/ui/profile_ui_test_utils.h"
#include "chrome/browser/ui/views/profiles/profile_picker_view.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"
#include "url/gurl.h"

ProfilePickerTestBase::ProfilePickerTestBase() = default;

ProfilePickerTestBase::~ProfilePickerTestBase() = default;

ProfilePickerView* ProfilePickerTestBase::view() {
  return static_cast<ProfilePickerView*>(ProfilePicker::GetViewForTesting());
}

views::Widget* ProfilePickerTestBase::widget() {
  return view() ? view()->GetWidget() : nullptr;
}

views::WebView* ProfilePickerTestBase::web_view() {
  return ProfilePicker::GetWebViewForTesting();
}

void ProfilePickerTestBase::WaitForPickerWidgetCreated() {
  profiles::testing::WaitForPickerWidgetCreated();
}

void ProfilePickerTestBase::WaitForLoadStop(const GURL& url,
                                            content::WebContents* target) {
  if (!target) {
    profiles::testing::WaitForPickerLoadStop(url);
    return;
  }

  content::WaitForLoadStop(target);
  EXPECT_EQ(target->GetLastCommittedURL(), url);
}

void ProfilePickerTestBase::WaitForPickerClosed() {
  profiles::testing::WaitForPickerClosed();
  ASSERT_FALSE(ProfilePicker::IsOpen());
}

void ProfilePickerTestBase::WaitForPickerClosedAndReopenedImmediately() {
  ASSERT_TRUE(ProfilePicker::IsOpen());
  profiles::testing::WaitForPickerClosed();
  EXPECT_TRUE(ProfilePicker::IsOpen());
}

content::WebContents* ProfilePickerTestBase::web_contents() {
  if (!web_view())
    return nullptr;
  return web_view()->GetWebContents();
}
