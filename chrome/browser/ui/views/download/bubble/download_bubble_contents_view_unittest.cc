// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/download/bubble/download_bubble_contents_view.h"

#include "chrome/browser/download/bubble/download_bubble_ui_controller.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_ui_model.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/download/bubble/download_bubble_row_view.h"
#include "chrome/browser/ui/views/download/bubble/download_toolbar_button_view.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/download/public/common/mock_download_item.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/test/mock_download_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/view.h"
#include "ui/views/window/dialog_client_view.h"

namespace {

using ::testing::NiceMock;
using ::testing::ReturnRefOfCopy;

class MockDownloadBubbleUIController : public DownloadBubbleUIController {
 public:
  explicit MockDownloadBubbleUIController(Browser* browser)
      : DownloadBubbleUIController(browser) {}
  ~MockDownloadBubbleUIController() = default;
};

class MockDownloadBubbleNavigationHandler
    : public DownloadBubbleNavigationHandler {
 public:
  virtual ~MockDownloadBubbleNavigationHandler() = default;
  void OpenPrimaryDialog() override {}
  void OpenSecurityDialog(DownloadBubbleRowView*) override {}
  void CloseDialog(views::Widget::ClosedReason) override {}
  void ResizeDialog() override {}
  void OnDialogInteracted() override {}
  base::WeakPtr<DownloadBubbleNavigationHandler> GetWeakPtr() override {
    return weak_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockDownloadBubbleNavigationHandler> weak_factory_{this};
};

class DownloadBubbleContentsViewTest
    : public ChromeViewsTestBase,
      public ::testing::WithParamInterface<bool> {
 public:
  DownloadBubbleContentsViewTest()
      : manager_(std::make_unique<
                 testing::NiceMock<content::MockDownloadManager>>()),
        testing_profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  bool IsPrimaryPartialView() const { return GetParam(); }

  // Sets up `num_items` mock download items with GUID equal to their index in
  // `download_items_`.
  void InitItems(int num_items) {
    for (int i = 0; i < num_items; ++i) {
      auto item = std::make_unique<NiceMock<download::MockDownloadItem>>();
      EXPECT_CALL(*item, GetGuid())
          .WillRepeatedly(ReturnRefOfCopy(base::NumberToString(i)));
      content::DownloadItemUtils::AttachInfoForTesting(item.get(), profile_,
                                                       nullptr);
      download_items_.push_back(std::move(item));
    }
  }

  std::vector<DownloadUIModel::DownloadUIModelPtr> GetModels() {
    std::vector<DownloadUIModel::DownloadUIModelPtr> models;
    for (const auto& item : download_items_) {
      models.push_back(DownloadItemModel::Wrap(
          item.get(),
          std::make_unique<DownloadUIModel::BubbleStatusTextBuilder>()));
    }
    return models;
  }

  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    ASSERT_TRUE(testing_profile_manager_.SetUp());
    profile_ = testing_profile_manager_.CreateTestingProfile("testing_profile");
    EXPECT_CALL(*manager_, GetBrowserContext())
        .WillRepeatedly(testing::Return(profile_.get()));
    window_ = std::make_unique<TestBrowserWindow>();
    Browser::CreateParams params(profile_, true);
    params.type = Browser::TYPE_NORMAL;
    params.window = window_.get();
    browser_ = std::unique_ptr<Browser>(Browser::Create(params));

    anchor_widget_ = CreateTestWidget(views::Widget::InitParams::TYPE_WINDOW);
    auto bubble_delegate = std::make_unique<views::BubbleDialogDelegate>(
        anchor_widget_->GetContentsView(), views::BubbleBorder::TOP_RIGHT);
    bubble_delegate_ = bubble_delegate.get();
    navigation_handler_ =
        std::make_unique<MockDownloadBubbleNavigationHandler>();
    views::BubbleDialogDelegate::CreateBubble(std::move(bubble_delegate));
    bubble_delegate_->GetWidget()->Show();
    bubble_controller_ =
        std::make_unique<MockDownloadBubbleUIController>(browser_.get());

    // TODO(chlily): Parameterize test on one vs multiple items.
    InitItems(1);
    contents_view_ = std::make_unique<DownloadBubbleContentsView>(
        browser_->AsWeakPtr(), bubble_controller_->GetWeakPtr(),
        navigation_handler_->GetWeakPtr(), IsPrimaryPartialView(), GetModels(),
        bubble_delegate_);
  }

  void TearDown() override {
    profile_ = nullptr;
    bubble_delegate_ = nullptr;
    // All windows need to be closed before tear down.
    anchor_widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

  DownloadBubbleContentsViewTest(const DownloadBubbleContentsViewTest&) =
      delete;
  DownloadBubbleContentsViewTest& operator=(
      const DownloadBubbleContentsViewTest&) = delete;

  raw_ptr<views::BubbleDialogDelegate> bubble_delegate_ = nullptr;
  std::unique_ptr<MockDownloadBubbleUIController> bubble_controller_;
  std::unique_ptr<MockDownloadBubbleNavigationHandler> navigation_handler_;
  std::unique_ptr<views::Widget> anchor_widget_;

  std::unique_ptr<DownloadBubbleContentsView> contents_view_;

  std::vector<std::unique_ptr<NiceMock<download::MockDownloadItem>>>
      download_items_;

  std::unique_ptr<testing::NiceMock<content::MockDownloadManager>> manager_;
  TestingProfileManager testing_profile_manager_;
  raw_ptr<Profile> profile_ = nullptr;
  std::unique_ptr<TestBrowserWindow> window_;
  std::unique_ptr<Browser> browser_;
};

// The test parameter is whether the primary view is the partial view.
INSTANTIATE_TEST_SUITE_P(/* no label */,
                         DownloadBubbleContentsViewTest,
                         ::testing::Bool());

TEST_P(DownloadBubbleContentsViewTest, Destroy) {
  contents_view_->UpdateSecurityView(
      contents_view_->GetPrimaryViewRowForTesting(0));
  // Destroying the contents view should not result in a crash, because the
  // raw_ptrs will have been properly cleared.
  contents_view_.reset();
}

}  // namespace
