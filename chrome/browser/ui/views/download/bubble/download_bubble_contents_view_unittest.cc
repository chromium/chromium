// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/download/bubble/download_bubble_contents_view.h"

#include "base/test/gmock_expected_support.h"
#include "chrome/browser/download/bubble/download_bubble_ui_controller.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_core_service.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_ui_model.h"
#include "chrome/browser/download/offline_item_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/download/download_bubble_info.h"
#include "chrome/browser/ui/hats/mock_trust_safety_sentiment_service.h"
#include "chrome/browser/ui/hats/trust_safety_sentiment_service_factory.h"
#include "chrome/browser/ui/views/download/bubble/download_bubble_primary_view.h"
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
#include "ui/views/controls/scroll_view.h"
#include "ui/views/view.h"
#include "ui/views/window/dialog_client_view.h"

namespace {

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRefOfCopy;

class MockDownloadBubbleNavigationHandler
    : public DownloadBubbleNavigationHandler {
 public:
  virtual ~MockDownloadBubbleNavigationHandler() = default;
  void OpenPrimaryDialog() override {}
  void OpenSecurityDialog(const offline_items_collection::ContentId&) override {
  }
  void CloseDialog(views::Widget::ClosedReason) override {}
  MOCK_METHOD(void,
              OnSecurityDialogButtonPress,
              (const DownloadUIModel& model, DownloadCommands::Command command),
              (override));
  void OnDialogInteracted() override {}
  std::unique_ptr<views::BubbleDialogDelegate::CloseOnDeactivatePin>
  PreventDialogCloseOnDeactivate() override {
    return nullptr;
  }
  base::WeakPtr<DownloadBubbleNavigationHandler> GetWeakPtr() override {
    return weak_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockDownloadBubbleNavigationHandler> weak_factory_{this};
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

class DownloadBubbleContentsViewTest
    : public ChromeViewsTestBase,
      public ::testing::WithParamInterface<bool> {
 public:
  DownloadBubbleContentsViewTest()
      : testing_profile_manager_(TestingBrowserProcess::GetGlobal()),
        manager_(std::make_unique<
                 testing::NiceMock<content::MockDownloadManager>>()) {}

  bool IsPrimaryPartialView() const { return GetParam(); }

  // Sets up `num_items` mock download items with GUID equal to their index in
  // `download_items_`.
  void InitItems(int num_items) {
    for (int i = 0; i < num_items; ++i) {
      auto item = std::make_unique<NiceMock<download::MockDownloadItem>>();
      EXPECT_CALL(*item, GetGuid())
          .WillRepeatedly(ReturnRefOfCopy(base::NumberToString(i)));
      EXPECT_CALL(*item, GetURL())
          .WillRepeatedly(ReturnRefOfCopy(GURL("https://chromium.org")));
      // Make the download dangerous so that showing the security view is valid.
      EXPECT_CALL(*item, GetDangerType())
          .WillRepeatedly(Return(download::DownloadDangerType::
                                     DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE));
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
    DownloadCoreServiceFactory::GetInstance()->SetTestingFactory(
        profile_, base::BindRepeating(&BuildMockDownloadCoreService));
    mock_download_core_service_ = static_cast<MockDownloadCoreService*>(
        DownloadCoreServiceFactory::GetForBrowserContext(profile_));
    EXPECT_CALL(*mock_download_core_service_, IsDownloadUiEnabled())
        .WillRepeatedly(Return(true));
    delegate_ = std::make_unique<ChromeDownloadManagerDelegate>(profile_);
    EXPECT_CALL(*mock_download_core_service_, GetDownloadManagerDelegate())
        .WillRepeatedly(Return(delegate_.get()));
    EXPECT_CALL(*manager_, GetBrowserContext())
        .WillRepeatedly(Return(profile_.get()));
    window_ = std::make_unique<TestBrowserWindow>();
    Browser::CreateParams params(profile_, true);
    params.type = Browser::TYPE_NORMAL;
    params.window = window_.get();
    browser_ = std::unique_ptr<Browser>(Browser::Create(params));

    anchor_widget_ =
        CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
                         views::Widget::InitParams::TYPE_WINDOW);
    auto bubble_delegate = std::make_unique<views::BubbleDialogDelegate>(
        anchor_widget_->GetContentsView(), views::BubbleBorder::TOP_RIGHT);
    bubble_delegate_ = bubble_delegate.get();
    navigation_handler_ =
        std::make_unique<MockDownloadBubbleNavigationHandler>();
    bubble_controller_ =
        std::make_unique<DownloadBubbleUIController>(browser_.get());

    // TODO(chlily): Parameterize test on one vs multiple items.
    InitItems(2);
    contents_view_ = std::make_unique<DownloadBubbleContentsView>(
        browser_->AsWeakPtr(), bubble_controller_->GetWeakPtr(),
        navigation_handler_->GetWeakPtr(), IsPrimaryPartialView(),
        std::make_unique<DownloadBubbleContentsViewInfo>(GetModels()),
        bubble_delegate_);
    // The contents view has to be set up before the bubble is shown, because it
    // sets initially focused view on the delegate (which cannot be set after
    // the widget is shown).
    views::BubbleDialogDelegate::CreateBubble(std::move(bubble_delegate));
    bubble_delegate_->GetWidget()->Show();
  }

  void SetUpMockTrustSafetySentimentSurveys() {
    mock_sentiment_service_ = static_cast<MockTrustSafetySentimentService*>(
        TrustSafetySentimentServiceFactory::GetInstance()
            ->SetTestingFactoryAndUse(
                profile_,
                base::BindRepeating(&BuildMockTrustSafetySentimentService)));
  }

  void ExpectInteractedWithDownloadUICalled() {
    EXPECT_CALL(*mock_sentiment_service_,
                InteractedWithDownloadWarningUI(
                    DownloadItemWarningData::WarningSurface::BUBBLE_SUBPAGE,
                    DownloadItemWarningData::WarningAction::DISCARD));
  }

  void TearDown() override {
    profile_ = nullptr;
    bubble_delegate_ = nullptr;
    mock_sentiment_service_ = nullptr;
    // All windows need to be closed before tear down.
    anchor_widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

  DownloadBubbleContentsViewTest(const DownloadBubbleContentsViewTest&) =
      delete;
  DownloadBubbleContentsViewTest& operator=(
      const DownloadBubbleContentsViewTest&) = delete;

  TestingProfileManager testing_profile_manager_;
  raw_ptr<Profile> profile_ = nullptr;
  raw_ptr<MockDownloadCoreService> mock_download_core_service_;
  std::unique_ptr<ChromeDownloadManagerDelegate> delegate_;
  std::unique_ptr<testing::NiceMock<content::MockDownloadManager>> manager_;
  std::unique_ptr<TestBrowserWindow> window_;
  std::unique_ptr<Browser> browser_;
  std::vector<std::unique_ptr<NiceMock<download::MockDownloadItem>>>
      download_items_;
  raw_ptr<views::BubbleDialogDelegate> bubble_delegate_ = nullptr;
  std::unique_ptr<DownloadBubbleUIController> bubble_controller_;
  std::unique_ptr<MockDownloadBubbleNavigationHandler> navigation_handler_;
  std::unique_ptr<views::Widget> anchor_widget_;

  std::unique_ptr<DownloadBubbleContentsView> contents_view_;
  raw_ptr<MockTrustSafetySentimentService> mock_sentiment_service_ = nullptr;
};

// The test parameter is whether the primary view is the partial view.
INSTANTIATE_TEST_SUITE_P(/* no label */,
                         DownloadBubbleContentsViewTest,
                         ::testing::Bool());

TEST_P(DownloadBubbleContentsViewTest, ShowSecurityPage) {
  // Download that doesn't exist in the row list view.
  EXPECT_DEATH_IF_SUPPORTED(
      contents_view_->ShowSecurityPage(
          offline_items_collection::ContentId{"bogus", "fake"}),
      "");
  EXPECT_FALSE(contents_view_->security_view_for_testing()->IsInitialized());

  // Download that exists in the row list view.
  contents_view_->ShowSecurityPage(
      OfflineItemUtils::GetContentIdForDownload(download_items_[0].get()));
  EXPECT_EQ(
      contents_view_->security_view_for_testing()->content_id(),
      OfflineItemUtils::GetContentIdForDownload(download_items_[0].get()));

  // Different download.
  contents_view_->ShowSecurityPage(
      OfflineItemUtils::GetContentIdForDownload(download_items_[1].get()));
  EXPECT_EQ(
      contents_view_->security_view_for_testing()->content_id(),
      OfflineItemUtils::GetContentIdForDownload(download_items_[1].get()));

  // Same as previous download.
  contents_view_->ShowSecurityPage(
      OfflineItemUtils::GetContentIdForDownload(download_items_[1].get()));
  EXPECT_EQ(
      contents_view_->security_view_for_testing()->content_id(),
      OfflineItemUtils::GetContentIdForDownload(download_items_[1].get()));
}

TEST_P(DownloadBubbleContentsViewTest, Destroy) {
  contents_view_->ShowSecurityPage(
      OfflineItemUtils::GetContentIdForDownload(download_items_[0].get()));
  // Destroying the contents view should not result in a crash, because the
  // raw_ptrs will have been properly cleared.
  contents_view_.reset();
}

// Switching back and forth between pages should work and not crash.
TEST_P(DownloadBubbleContentsViewTest, SwitchPages) {
  // Switch from primary to security view.
  contents_view_->ShowSecurityPage(
      OfflineItemUtils::GetContentIdForDownload(download_items_[0].get()));
  EXPECT_EQ(
      contents_view_->security_view_for_testing()->content_id(),
      OfflineItemUtils::GetContentIdForDownload(download_items_[0].get()));
  EXPECT_EQ(contents_view_->VisiblePage(),
            DownloadBubbleContentsView::Page::kSecurity);

  // Switch back to primary view. The security view should be reset and not
  // crash.
  EXPECT_EQ(nullptr, contents_view_->ShowPrimaryPage());
  EXPECT_EQ(contents_view_->VisiblePage(),
            DownloadBubbleContentsView::Page::kPrimary);
  EXPECT_FALSE(contents_view_->security_view_for_testing()->IsInitialized());

  // Switch to security view for the other download.
  contents_view_->ShowSecurityPage(
      OfflineItemUtils::GetContentIdForDownload(download_items_[1].get()));
  EXPECT_TRUE(contents_view_->security_view_for_testing()->IsInitialized());
  EXPECT_EQ(
      contents_view_->security_view_for_testing()->content_id(),
      OfflineItemUtils::GetContentIdForDownload(download_items_[1].get()));
  EXPECT_EQ(contents_view_->VisiblePage(),
            DownloadBubbleContentsView::Page::kSecurity);

  // Switch back to primary view. The security view should be reset and not
  // crash.
  EXPECT_EQ(nullptr, contents_view_->ShowPrimaryPage());
  EXPECT_EQ(contents_view_->VisiblePage(),
            DownloadBubbleContentsView::Page::kPrimary);
  EXPECT_FALSE(contents_view_->security_view_for_testing()->IsInitialized());

  // Switch to security view for the first download again.
  contents_view_->ShowSecurityPage(
      OfflineItemUtils::GetContentIdForDownload(download_items_[0].get()));
  EXPECT_TRUE(contents_view_->security_view_for_testing()->IsInitialized());
  EXPECT_EQ(
      contents_view_->security_view_for_testing()->content_id(),
      OfflineItemUtils::GetContentIdForDownload(download_items_[0].get()));
  EXPECT_EQ(contents_view_->VisiblePage(),
            DownloadBubbleContentsView::Page::kSecurity);

  // Should not crash.
  contents_view_.reset();
}

TEST_P(DownloadBubbleContentsViewTest, ShowPrimaryPageSpecifyingContentId) {
  DownloadBubbleRowView* expected_row =
      contents_view_->GetPrimaryViewRowForTesting(0);
  EXPECT_EQ(
      expected_row,
      contents_view_->ShowPrimaryPage(
          OfflineItemUtils::GetContentIdForDownload(download_items_[0].get())));
  EXPECT_EQ(contents_view_->VisiblePage(),
            DownloadBubbleContentsView::Page::kPrimary);
  EXPECT_TRUE(contents_view_->primary_view_for_testing()
                  ->scroll_view_for_testing()
                  ->GetVisibleRect()
                  .Contains(expected_row->bounds()));
}

TEST_P(DownloadBubbleContentsViewTest, ProcessSecuritySubpageButtonPress) {
  contents_view_->ShowSecurityPage(
      OfflineItemUtils::GetContentIdForDownload(download_items_[0].get()));
  EXPECT_TRUE(contents_view_->security_view_for_testing()->IsInitialized());

  EXPECT_CALL(*download_items_[0], Remove());
  contents_view_->ProcessSecuritySubpageButtonPress(
      OfflineItemUtils::GetContentIdForDownload(download_items_[0].get()),
      DownloadCommands::Command::DISCARD);
}

TEST_P(DownloadBubbleContentsViewTest,
       TrustSafetySentimentInteractedWithDownloadWarningUI) {
  SetUpMockTrustSafetySentimentSurveys();
  ExpectInteractedWithDownloadUICalled();
  contents_view_->ShowSecurityPage(
      OfflineItemUtils::GetContentIdForDownload(download_items_[0].get()));
  contents_view_->ProcessSecuritySubpageButtonPress(
      OfflineItemUtils::GetContentIdForDownload(download_items_[0].get()),
      DownloadCommands::Command::DISCARD);
}

TEST_P(DownloadBubbleContentsViewTest, AddSecuritySubpageWarningActionEvent) {
  contents_view_->ShowSecurityPage(
      OfflineItemUtils::GetContentIdForDownload(download_items_[0].get()));
  EXPECT_TRUE(contents_view_->security_view_for_testing()->IsInitialized());

  // First action is required to be SHOWN.
  DownloadItemWarningData::AddWarningActionEvent(
      download_items_[0].get(),
      DownloadItemWarningData::WarningSurface::BUBBLE_MAINPAGE,
      DownloadItemWarningData::WarningAction::SHOWN);

  contents_view_->AddSecuritySubpageWarningActionEvent(
      OfflineItemUtils::GetContentIdForDownload(download_items_[0].get()),
      DownloadItemWarningData::WarningAction::BACK);

  std::vector<DownloadItemWarningData::WarningActionEvent> events =
      DownloadItemWarningData::GetWarningActionEvents(download_items_[0].get());
  ASSERT_EQ(events.size(), 1u);
  EXPECT_EQ(events[0].action, DownloadItemWarningData::WarningAction::BACK);
}

TEST_P(DownloadBubbleContentsViewTest, LogDismissOnDestroyed) {
  contents_view_->ShowSecurityPage(
      OfflineItemUtils::GetContentIdForDownload(download_items_[0].get()));
  EXPECT_TRUE(contents_view_->security_view_for_testing()->IsInitialized());

  // First action is required to be SHOWN.
  DownloadItemWarningData::AddWarningActionEvent(
      download_items_[0].get(),
      DownloadItemWarningData::WarningSurface::BUBBLE_MAINPAGE,
      DownloadItemWarningData::WarningAction::SHOWN);

  contents_view_.reset();

  std::vector<DownloadItemWarningData::WarningActionEvent> events =
      DownloadItemWarningData::GetWarningActionEvents(download_items_[0].get());
  ASSERT_EQ(events.size(), 1u);
  EXPECT_EQ(events[0].action, DownloadItemWarningData::WarningAction::DISMISS);
}

TEST_P(DownloadBubbleContentsViewTest,
       DontLogDismissOnDestroyedIfSecurityViewNotShown) {
  contents_view_->ShowSecurityPage(
      OfflineItemUtils::GetContentIdForDownload(download_items_[0].get()));
  EXPECT_TRUE(contents_view_->security_view_for_testing()->IsInitialized());

  // First action is required to be SHOWN.
  DownloadItemWarningData::AddWarningActionEvent(
      download_items_[0].get(),
      DownloadItemWarningData::WarningSurface::BUBBLE_MAINPAGE,
      DownloadItemWarningData::WarningAction::SHOWN);

  contents_view_->ShowPrimaryPage();

  contents_view_.reset();

  std::vector<DownloadItemWarningData::WarningActionEvent> events =
      DownloadItemWarningData::GetWarningActionEvents(download_items_[0].get());
  EXPECT_TRUE(events.empty());
}

TEST_P(DownloadBubbleContentsViewTest,
       ProcessSecuritySubpageButtonPressCallsOnSecurityDialogButtonPress) {
  contents_view_->ShowSecurityPage(
      OfflineItemUtils::GetContentIdForDownload(download_items_[0].get()));
  EXPECT_TRUE(contents_view_->security_view_for_testing()->IsInitialized());

  EXPECT_CALL(*download_items_[0], Remove());
  EXPECT_CALL(*navigation_handler_, OnSecurityDialogButtonPress(
                                        _, DownloadCommands::Command::DISCARD))
      .Times(1);
  contents_view_->ProcessSecuritySubpageButtonPress(
      OfflineItemUtils::GetContentIdForDownload(download_items_[0].get()),
      DownloadCommands::Command::DISCARD);
}

}  // namespace
