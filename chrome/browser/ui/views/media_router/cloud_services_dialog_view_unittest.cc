// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_router/cloud_services_dialog_view.h"

#include "base/run_loop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/widget/widget.h"

namespace media_router {

class CloudServicesDialogViewTest : public ChromeViewsTestBase {
 public:
  CloudServicesDialogViewTest() = default;
  ~CloudServicesDialogViewTest() override = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    profile_ = std::make_unique<TestingProfile>();
    window_ = std::make_unique<TestBrowserWindow>();
    Browser::CreateParams browser_params(profile_.get(), true);
    browser_params.window = window_.get();
    browser_ = std::make_unique<Browser>(browser_params);

    views::Widget::InitParams widget_params =
        CreateParams(views::Widget::InitParams::TYPE_WINDOW);
    widget_params.ownership =
        views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    anchor_widget_ = std::make_unique<views::Widget>();
    anchor_widget_->Init(std::move(widget_params));
    anchor_widget_->Show();
  }

  void TearDown() override {
    anchor_widget_.reset();
    browser_.reset();
    window_.reset();
    profile_.reset();

    ChromeViewsTestBase::TearDown();
  }

 protected:
  void ShowDialog() {
    CloudServicesDialogView::ShowDialog(anchor_widget_->GetContentsView(),
                                        browser_.get());
    EXPECT_TRUE(CloudServicesDialogView::IsShowing());
  }

  CloudServicesDialogView* GetDialog() {
    return CloudServicesDialogView::GetDialogForTest();
  }

  void ExpectPrefsToBe(bool value) {
    PrefService* pref_service = browser_->profile()->GetPrefs();
    EXPECT_EQ(value,
              pref_service->GetBoolean(prefs::kMediaRouterEnableCloudServices));
    EXPECT_EQ(value, pref_service->GetBoolean(
                         prefs::kMediaRouterCloudServicesPrefSet));
  }

  std::unique_ptr<BrowserWindow> window_;
  std::unique_ptr<Browser> browser_;
  std::unique_ptr<views::Widget> anchor_widget_;
  std::unique_ptr<TestingProfile> profile_;
};

TEST_F(CloudServicesDialogViewTest, Enable) {
  ExpectPrefsToBe(false);
  ShowDialog();
  GetDialog()->Accept();
  // Clicking on the "Enable" button should set the pref values to true.
  ExpectPrefsToBe(true);
}

TEST_F(CloudServicesDialogViewTest, Cancel) {
  ExpectPrefsToBe(false);
  ShowDialog();
  GetDialog()->Cancel();
  // Clicking on the "Cancel" button should result in no pref changes.
  ExpectPrefsToBe(false);
}

TEST_F(CloudServicesDialogViewTest, Close) {
  ExpectPrefsToBe(false);
  ShowDialog();
  GetDialog()->Close();
  // Closing the dialog via the close button should result in no pref changes.
  ExpectPrefsToBe(false);
}

}  // namespace media_router
