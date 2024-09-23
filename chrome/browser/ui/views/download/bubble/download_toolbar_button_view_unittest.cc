// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/download/bubble/download_toolbar_button_view.h"

#include "base/files/file_path.h"
#include "chrome/browser/download/bubble/download_bubble_ui_controller.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_core_service.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/offline_item_utils.h"
#include "chrome/browser/ui/views/download/bubble/download_bubble_contents_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/download/public/common/mock_download_item.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/test/mock_download_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRefOfCopy;

class MockDownloadBubbleUIController : public DownloadBubbleUIController {
 public:
  MockDownloadBubbleUIController(
      Browser* browser,
      std::vector<std::unique_ptr<NiceMock<download::MockDownloadItem>>>*
          download_items)
      : DownloadBubbleUIController(browser), download_items_(download_items) {}
  ~MockDownloadBubbleUIController() override = default;

  std::vector<DownloadUIModel::DownloadUIModelPtr> GetMainView() override {
    return GetModels();
  }
  std::vector<DownloadUIModel::DownloadUIModelPtr> GetPartialView() override {
    return GetModels();
  }

 private:
  std::vector<DownloadUIModel::DownloadUIModelPtr> GetModels() {
    std::vector<DownloadUIModel::DownloadUIModelPtr> models;
    for (const auto& item : *download_items_) {
      models.push_back(DownloadItemModel::Wrap(
          item.get(),
          std::make_unique<DownloadUIModel::BubbleStatusTextBuilder>()));
    }
    return models;
  }

  raw_ptr<std::vector<std::unique_ptr<NiceMock<download::MockDownloadItem>>>>
      download_items_;
};

class MockDownloadCoreService : public DownloadCoreService {
 public:
  MOCK_METHOD(ChromeDownloadManagerDelegate*, GetDownloadManagerDelegate, ());
  MOCK_METHOD(DownloadUIController*, GetDownloadUIController, ());
  MOCK_METHOD(DownloadHistory*, GetDownloadHistory, ());
  MOCK_METHOD(extensions::ExtensionDownloadsEventRouter*,
              GetExtensionEventRouter,
              ());
  MOCK_METHOD(bool, HasCreatedDownloadManager, ());
  MOCK_METHOD(int, BlockingShutdownCount, (), (const));
  MOCK_METHOD(void,
              CancelDownloads,
              (DownloadCoreService::CancelDownloadsTrigger));
  MOCK_METHOD(void,
              SetDownloadManagerDelegateForTesting,
              (std::unique_ptr<ChromeDownloadManagerDelegate> delegate));
  MOCK_METHOD(bool, IsDownloadUiEnabled, ());
  MOCK_METHOD(bool, IsDownloadObservedByExtension, ());
};

std::unique_ptr<KeyedService> BuildMockDownloadCoreService(
    content::BrowserContext* browser_context) {
  return std::make_unique<MockDownloadCoreService>();
}

// This tests the DownloadToolbarButtonView that is already in the test
// fixture's browser. This makes sure it is hooked up properly to the testing
// profile and downloads machinery.
// TODO(chlily): Investigate whether there is a better way to do this.
// TODO(chlily): Add more tests to cover all functionality.
class DownloadToolbarButtonViewTest : public TestWithBrowserView {
 public:
  DownloadToolbarButtonViewTest()
      : manager_(std::make_unique<NiceMock<content::MockDownloadManager>>()) {}

  DownloadToolbarButtonViewTest(const DownloadToolbarButtonViewTest&) = delete;
  DownloadToolbarButtonViewTest& operator=(
      const DownloadToolbarButtonViewTest&) = delete;

  ~DownloadToolbarButtonViewTest() override = default;

  // TODO(chlily): Factor out test utils into a separate file.
  // Sets up `num_items` mock download items with GUID equal to their index in
  // `download_items_`.
  void InitItems(int num_items) {
    for (int i = 0; i < num_items; ++i) {
      auto item = std::make_unique<NiceMock<download::MockDownloadItem>>();
      ON_CALL(*item, GetGuid())
          .WillByDefault(ReturnRefOfCopy(base::NumberToString(i)));
      ON_CALL(*item, GetURL())
          .WillByDefault(ReturnRefOfCopy(GURL("https://chromium.org")));
      // Make the download dangerous so that showing the security view is valid.
      ON_CALL(*item, GetDangerType())
          .WillByDefault(Return(download::DownloadDangerType::
                                    DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE));
      ON_CALL(*item, GetTargetFilePath())
          .WillByDefault(
              ReturnRefOfCopy(base::FilePath(FILE_PATH_LITERAL("foo"))));
      content::DownloadItemUtils::AttachInfoForTesting(item.get(), profile(),
                                                       nullptr);
      download_items_.push_back(std::move(item));
    }
  }

  void SetUp() override {
    TestWithBrowserView::SetUp();
    toolbar_button()->DisableDownloadStartedAnimationForTesting();
    toolbar_button()->DisableAutoCloseTimerForTesting();
    toolbar_button()->SetBubbleControllerForTesting(
        std::make_unique<MockDownloadBubbleUIController>(browser(),
                                                         &download_items_));

    DownloadCoreServiceFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindRepeating(&BuildMockDownloadCoreService));
    MockDownloadCoreService* mock_dcs = static_cast<MockDownloadCoreService*>(
        DownloadCoreServiceFactory::GetForBrowserContext(profile()));
    ON_CALL(*mock_dcs, IsDownloadUiEnabled()).WillByDefault(Return(true));
    delegate_ = std::make_unique<ChromeDownloadManagerDelegate>(profile());
    ON_CALL(*mock_dcs, GetDownloadManagerDelegate())
        .WillByDefault(Return(delegate_.get()));
  }

  void TearDown() override {
    delegate_.reset();
    TestWithBrowserView::TearDown();
  }

  DownloadToolbarButtonView* toolbar_button() {
    return browser_view()->toolbar_button_provider()->GetDownloadButton();
  }

 protected:
  std::unique_ptr<ChromeDownloadManagerDelegate> delegate_;
  std::unique_ptr<NiceMock<content::MockDownloadManager>> manager_;
  std::vector<std::unique_ptr<NiceMock<download::MockDownloadItem>>>
      download_items_;
};

TEST_F(DownloadToolbarButtonViewTest, ShowHide) {
  ASSERT_FALSE(toolbar_button()->IsShowing());
  toolbar_button()->Show();
  EXPECT_TRUE(toolbar_button()->IsShowing());
  toolbar_button()->Hide();
  EXPECT_FALSE(toolbar_button()->IsShowing());
}

TEST_F(DownloadToolbarButtonViewTest, OpenPrimaryDialog) {
  InitItems(1);
  toolbar_button()->ShowDetails();
  toolbar_button()->OpenPrimaryDialog();
  EXPECT_EQ(toolbar_button()->bubble_contents_for_testing()->VisiblePage(),
            DownloadBubbleContentsView::Page::kPrimary);
}

TEST_F(DownloadToolbarButtonViewTest, OpenSecurityDialog) {
  // Init 2 items to make sure that the right item is shown.
  InitItems(2);
  toolbar_button()->ShowDetails();
  offline_items_collection::ContentId content_id =
      OfflineItemUtils::GetContentIdForDownload(download_items_[0].get());
  toolbar_button()->OpenSecurityDialog(content_id);
  EXPECT_EQ(toolbar_button()->bubble_contents_for_testing()->VisiblePage(),
            DownloadBubbleContentsView::Page::kSecurity);
  EXPECT_EQ(toolbar_button()
                ->bubble_contents_for_testing()
                ->security_view_for_testing()
                ->content_id(),
            content_id);
}

TEST_F(DownloadToolbarButtonViewTest, OpenMostSpecificDialogToSecurityPage) {
  // Init 2 items to make sure that the right item is shown.
  InitItems(2);
  offline_items_collection::ContentId content_id =
      OfflineItemUtils::GetContentIdForDownload(download_items_[0].get());
  toolbar_button()->OpenMostSpecificDialog(content_id);
  EXPECT_TRUE(toolbar_button()->IsShowing());
  EXPECT_EQ(toolbar_button()->bubble_contents_for_testing()->VisiblePage(),
            DownloadBubbleContentsView::Page::kSecurity);
  EXPECT_EQ(toolbar_button()
                ->bubble_contents_for_testing()
                ->security_view_for_testing()
                ->content_id(),
            content_id);
}

TEST_F(DownloadToolbarButtonViewTest, OpenMostSpecificDialogToPrimaryPage) {
  InitItems(1);
  // Make this a not-dangerous download so that the most specific dialog is just
  // the primary page.
  EXPECT_CALL(*download_items_[0], GetDangerType())
      .WillRepeatedly(Return(
          download::DownloadDangerType::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS));
  toolbar_button()->OpenMostSpecificDialog(
      OfflineItemUtils::GetContentIdForDownload(download_items_[0].get()));
  EXPECT_TRUE(toolbar_button()->IsShowing());
  EXPECT_EQ(toolbar_button()->bubble_contents_for_testing()->VisiblePage(),
            DownloadBubbleContentsView::Page::kPrimary);
}

}  // namespace
