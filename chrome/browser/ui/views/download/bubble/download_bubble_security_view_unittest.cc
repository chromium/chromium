// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/download/bubble/download_bubble_security_view.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/download/bubble/download_bubble_ui_controller.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_core_service.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_item_warning_data.h"
#include "chrome/browser/download/download_ui_model.h"
#include "chrome/browser/download/offline_item_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/download/download_bubble_contents_view_info.h"
#include "chrome/browser/ui/download/download_bubble_info.h"
#include "chrome/browser/ui/download/download_bubble_security_view_info.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/download/bubble/download_bubble_contents_view.h"
#include "chrome/browser/ui/views/download/bubble/download_bubble_row_view.h"
#include "chrome/browser/ui/views/download/bubble/download_toolbar_button_view.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/download/public/common/mock_download_item.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/test/mock_download_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/color/color_id.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view.h"
#include "ui/views/window/dialog_client_view.h"

namespace {

using offline_items_collection::ContentId;
using ::testing::Return;
using ::testing::ReturnRefOfCopy;

using SubpageButton = DownloadBubbleSecurityViewInfo::SubpageButton;
using WarningSurface = DownloadItemWarningData::WarningSurface;
using WarningAction = DownloadItemWarningData::WarningAction;
using WarningActionEvent = DownloadItemWarningData::WarningActionEvent;

class MockDownloadBubbleNavigationHandler
    : public DownloadBubbleNavigationHandler {
 public:
  explicit MockDownloadBubbleNavigationHandler(
      DownloadBubbleSecurityViewInfo& security_view_info)
      : security_view_info_(security_view_info) {}
  virtual ~MockDownloadBubbleNavigationHandler() = default;
  void OpenPrimaryDialog() override {
    security_view_info_->Reset();
    last_opened_page_ = DownloadBubbleContentsView::Page::kPrimary;
  }
  void OpenSecurityDialog(const offline_items_collection::ContentId&) override {
    last_opened_page_ = DownloadBubbleContentsView::Page::kSecurity;
  }
  void CloseDialog(views::Widget::ClosedReason) override { was_closed_ = true; }
  void OnSecurityDialogButtonPress(const DownloadUIModel& model,
                                   DownloadCommands::Command command) override {
  }
  void OnDialogInteracted() override {}
  std::unique_ptr<views::BubbleDialogDelegate::CloseOnDeactivatePin>
  PreventDialogCloseOnDeactivate() override {
    return nullptr;
  }
  base::WeakPtr<DownloadBubbleNavigationHandler> GetWeakPtr() override {
    return weak_factory_.GetWeakPtr();
  }

  bool was_closed() const { return was_closed_; }
  std::optional<DownloadBubbleContentsView::Page> last_opened_page() const {
    return last_opened_page_;
  }

 private:
  raw_ref<DownloadBubbleSecurityViewInfo> security_view_info_;
  std::optional<DownloadBubbleContentsView::Page> last_opened_page_ =
      std::nullopt;
  bool was_closed_ = false;
  base::WeakPtrFactory<MockDownloadBubbleNavigationHandler> weak_factory_{this};
};

class MockDownloadBubbleSecurityViewDelegate
    : public DownloadBubbleSecurityView::Delegate {
 public:
  MockDownloadBubbleSecurityViewDelegate(download::DownloadItem* item1,
                                         download::DownloadItem* item2)
      : download_item1_(item1), download_item2_(item2) {}

  virtual ~MockDownloadBubbleSecurityViewDelegate() = default;

  void ProcessSecuritySubpageButtonPress(const ContentId&,
                                         DownloadCommands::Command) override {}

  void AddSecuritySubpageWarningActionEvent(
      const ContentId& id,
      DownloadItemWarningData::WarningAction action) override {
    download::DownloadItem* item = nullptr;
    if (id.id == "guid1") {
      item = download_item1_;
    } else {
      item = download_item2_;
    }
    DownloadItemWarningData::AddWarningActionEvent(
        item, WarningSurface::BUBBLE_SUBPAGE, action);
  }

  void ProcessDeepScanPress(const ContentId&,
                            DownloadItemWarningData::DeepScanTrigger trigger,
                            base::optional_ref<const std::string>) override {}
  void ProcessLocalDecryptionPress(
      const offline_items_collection::ContentId& id,
      base::optional_ref<const std::string> password) override {}
  void ProcessLocalPasswordInProgressClick(
      const offline_items_collection::ContentId& id,
      DownloadCommands::Command command) override {}
  bool IsEncryptedArchive(const ContentId&) override { return false; }
  bool HasPreviousIncorrectPassword(const ContentId&) override { return false; }

 private:
  const raw_ptr<download::DownloadItem> download_item1_;
  const raw_ptr<download::DownloadItem> download_item2_;
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

}  // namespace

class DownloadBubbleSecurityViewTest : public ChromeViewsTestBase {
 public:
  DownloadBubbleSecurityViewTest()
      : security_view_delegate_(
            std::make_unique<MockDownloadBubbleSecurityViewDelegate>(
                &download_item1_,
                &download_item2_)),
        manager_(std::make_unique<
                 testing::NiceMock<content::MockDownloadManager>>()),
        testing_profile_manager_(TestingBrowserProcess::GetGlobal()) {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kNoFirstRun);
  }

  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    ASSERT_TRUE(testing_profile_manager_.SetUp());
    profile_ = testing_profile_manager_.CreateTestingProfile("testing_profile");
    EXPECT_CALL(*manager_.get(), GetBrowserContext())
        .WillRepeatedly(testing::Return(profile_.get()));
    window_ = std::make_unique<TestBrowserWindow>();
    Browser::CreateParams params(profile_, true);
    params.type = Browser::TYPE_NORMAL;
    params.window = window_.get();
    browser_ = std::unique_ptr<Browser>(Browser::Create(params));

    security_view_info_ = std::make_unique<DownloadBubbleSecurityViewInfo>();
    anchor_widget_ =
        CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
                         views::Widget::InitParams::TYPE_WINDOW);
    auto bubble_delegate = std::make_unique<views::BubbleDialogDelegate>(
        anchor_widget_->GetContentsView(), views::BubbleBorder::TOP_RIGHT);
    bubble_delegate_ = bubble_delegate.get();
    bubble_navigator_ = std::make_unique<MockDownloadBubbleNavigationHandler>(
        *security_view_info_);
    views::BubbleDialogDelegate::CreateBubble(std::move(bubble_delegate));
    bubble_delegate_->GetWidget()->Show();
    bubble_controller_ =
        std::make_unique<DownloadBubbleUIController>(browser_.get());
    security_view_ = bubble_delegate_->SetContentsView(
        std::make_unique<DownloadBubbleSecurityView>(
            security_view_delegate_.get(), *security_view_info_,
            bubble_navigator_->GetWeakPtr(), bubble_delegate_));

    DownloadCoreServiceFactory::GetInstance()->SetTestingFactory(
        browser_->profile(),
        base::BindRepeating(&BuildMockDownloadCoreService));
    MockDownloadCoreService* mock_dcs = static_cast<MockDownloadCoreService*>(
        DownloadCoreServiceFactory::GetForBrowserContext(browser_->profile()));
    ON_CALL(*mock_dcs, IsDownloadUiEnabled()).WillByDefault(Return(true));
    delegate_ =
        std::make_unique<ChromeDownloadManagerDelegate>(browser_->profile());
    ON_CALL(*mock_dcs, GetDownloadManagerDelegate())
        .WillByDefault(Return(delegate_.get()));

    // Needed to make the model not return an empty ContentId.
    ON_CALL(download_item1_, GetGuid())
        .WillByDefault(ReturnRefOfCopy(std::string("guid1")));
    ON_CALL(download_item2_, GetGuid())
        .WillByDefault(ReturnRefOfCopy(std::string("guid2")));

    content::DownloadItemUtils::AttachInfoForTesting(&download_item1_, profile_,
                                                     nullptr);
    content::DownloadItemUtils::AttachInfoForTesting(&download_item2_, profile_,
                                                     nullptr);

    const int bubble_width = ChromeLayoutProvider::Get()->GetDistanceMetric(
        views::DISTANCE_BUBBLE_PREFERRED_WIDTH);
    std::vector<DownloadUIModel::DownloadUIModelPtr> models;
    models.push_back(DownloadItemModel::Wrap(&download_item1_));
    models.push_back(DownloadItemModel::Wrap(&download_item2_));
    row1_model_ = models[0].get();
    row2_model_ = models[1].get();
    info_ = std::make_unique<DownloadBubbleRowListViewInfo>(std::move(models));
    row_list_view_ = std::make_unique<DownloadBubbleRowListView>(
        browser_->AsWeakPtr(), bubble_controller_->GetWeakPtr(),
        bubble_navigator_->GetWeakPtr(), bubble_width, *info_);

    // Give both items a valid default security subpage
    ON_CALL(download_item1_, GetDangerType())
        .WillByDefault(Return(
            download::DownloadDangerType::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE));
    ON_CALL(download_item1_, GetURL())
        .WillByDefault(ReturnRefOfCopy(GURL("https://example.com/a.exe")));

    ON_CALL(download_item2_, GetDangerType())
        .WillByDefault(Return(
            download::DownloadDangerType::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE));
    ON_CALL(download_item2_, GetURL())
        .WillByDefault(ReturnRefOfCopy(GURL("https://example.com/a.exe")));
  }

  void TearDown() override {
    delegate_.reset();
    // All windows need to be closed before tear down.
    anchor_widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

  DownloadBubbleSecurityViewTest(const DownloadBubbleSecurityViewTest&) =
      delete;
  DownloadBubbleSecurityViewTest& operator=(
      const DownloadBubbleSecurityViewTest&) = delete;

  void UpdateView() { security_view_->UpdateViews(); }

  std::unique_ptr<ChromeDownloadManagerDelegate> delegate_;
  testing::NiceMock<download::MockDownloadItem> download_item1_;
  testing::NiceMock<download::MockDownloadItem> download_item2_;
  std::unique_ptr<MockDownloadBubbleSecurityViewDelegate>
      security_view_delegate_;
  raw_ptr<views::BubbleDialogDelegate, DanglingUntriaged> bubble_delegate_ =
      nullptr;
  std::unique_ptr<DownloadBubbleUIController> bubble_controller_;
  raw_ptr<DownloadBubbleSecurityView, DanglingUntriaged> security_view_ =
      nullptr;
  std::unique_ptr<views::Widget> anchor_widget_;
  std::unique_ptr<DownloadBubbleSecurityViewInfo> security_view_info_;
  std::unique_ptr<MockDownloadBubbleNavigationHandler> bubble_navigator_;

  std::unique_ptr<DownloadBubbleRowListViewInfo> info_;
  std::unique_ptr<DownloadBubbleRowListView> row_list_view_;
  raw_ptr<DownloadUIModel> row1_model_;
  raw_ptr<DownloadUIModel> row2_model_;

  std::unique_ptr<testing::NiceMock<content::MockDownloadManager>> manager_;
  TestingProfileManager testing_profile_manager_;
  raw_ptr<Profile> profile_ = nullptr;
  std::unique_ptr<TestBrowserWindow> window_;
  std::unique_ptr<Browser> browser_;
};

TEST_F(DownloadBubbleSecurityViewTest,
       UpdateSecurityView_WillHaveAppropriateDialogButtons) {
  // Two buttons, one prominent
  security_view_info_->InitializeForDownload(*row1_model_);
  security_view_info_->SetSubpageButtonsForTesting(
      {SubpageButton(DownloadCommands::Command::DISCARD, std::u16string(),
                     /*is_prominent=*/true),
       SubpageButton(DownloadCommands::Command::KEEP, std::u16string(),
                     /*is_prominent=*/false, ui::kColorAlertHighSeverity)});

  EXPECT_EQ(bubble_delegate_->buttons(),
            static_cast<int>(ui::mojom::DialogButton::kOk) |
                static_cast<int>(ui::mojom::DialogButton::kCancel));
  EXPECT_EQ(bubble_delegate_->GetDefaultDialogButton(),
            static_cast<int>(ui::mojom::DialogButton::kOk));

  // Two buttons, none prominent
  security_view_->Reset();
  security_view_info_->InitializeForDownload(*row1_model_);
  security_view_info_->SetSubpageButtonsForTesting(
      {SubpageButton(DownloadCommands::Command::DISCARD, std::u16string(),
                     /*is_prominent=*/false),
       SubpageButton(DownloadCommands::Command::KEEP, std::u16string(),
                     /*is_prominent=*/false, ui::kColorAlertHighSeverity)});
  UpdateView();

  EXPECT_EQ(bubble_delegate_->buttons(),
            static_cast<int>(ui::mojom::DialogButton::kOk) |
                static_cast<int>(ui::mojom::DialogButton::kCancel));
  EXPECT_EQ(bubble_delegate_->GetDefaultDialogButton(),
            static_cast<int>(ui::mojom::DialogButton::kNone));

  // One button, none prominent
  security_view_->Reset();
  security_view_info_->InitializeForDownload(*row1_model_);
  security_view_info_->SetSubpageButtonsForTesting(
      {SubpageButton(DownloadCommands::Command::DISCARD, std::u16string(),
                     /*is_prominent=*/false)});
  UpdateView();

  EXPECT_EQ(bubble_delegate_->buttons(),
            static_cast<int>(ui::mojom::DialogButton::kOk));
  EXPECT_EQ(bubble_delegate_->GetDefaultDialogButton(),
            static_cast<int>(ui::mojom::DialogButton::kNone));

  // No buttons, none prominent
  security_view_->Reset();
  security_view_info_->InitializeForDownload(*row1_model_);
  security_view_info_->SetSubpageButtonsForTesting({});
  UpdateView();

  EXPECT_EQ(bubble_delegate_->buttons(),
            static_cast<int>(ui::mojom::DialogButton::kNone));
  EXPECT_EQ(bubble_delegate_->GetDefaultDialogButton(),
            static_cast<int>(ui::mojom::DialogButton::kNone));
}

TEST_F(DownloadBubbleSecurityViewTest, VerifyLogWarningActions) {
  DownloadItemWarningData::AddWarningActionEvent(
      &download_item1_, WarningSurface::BUBBLE_MAINPAGE, WarningAction::SHOWN);

  size_t item1_actions_expected = 0u, item2_actions_expected = 0u;

  // Back action logged.
  {
    auto security_view_info =
        std::make_unique<DownloadBubbleSecurityViewInfo>();
    auto bubble_navigator =
        std::make_unique<MockDownloadBubbleNavigationHandler>(
            *security_view_info);
    auto security_view = std::make_unique<DownloadBubbleSecurityView>(
        security_view_delegate_.get(), *security_view_info,
        bubble_navigator->GetWeakPtr(), bubble_delegate_);
    security_view_info->InitializeForDownload(*row1_model_);

    security_view->BackButtonPressed();
    ++item1_actions_expected;

    // Delete early to ensure DISMISS is not logged if BACK is already logged.
    security_view.reset();
    std::vector<WarningActionEvent> events =
        DownloadItemWarningData::GetWarningActionEvents(&download_item1_);
    ASSERT_EQ(events.size(), item1_actions_expected);
    EXPECT_EQ(events.back().action, WarningAction::BACK);
  }

  // Close action logged
  {
    auto security_view_info =
        std::make_unique<DownloadBubbleSecurityViewInfo>();
    auto bubble_navigator =
        std::make_unique<MockDownloadBubbleNavigationHandler>(
            *security_view_info);
    auto security_view = std::make_unique<DownloadBubbleSecurityView>(
        security_view_delegate_.get(), *security_view_info,
        bubble_navigator->GetWeakPtr(), bubble_delegate_);
    security_view_info->InitializeForDownload(*row1_model_);

    security_view->CloseBubble();
    ++item1_actions_expected;
    EXPECT_TRUE(bubble_navigator->was_closed());

    security_view.reset();
    std::vector<WarningActionEvent> events =
        DownloadItemWarningData::GetWarningActionEvents(&download_item1_);
    ASSERT_EQ(events.size(), item1_actions_expected);
    EXPECT_EQ(events.back().action, WarningAction::CLOSE);
  }

  // Dismiss action logged
  {
    auto security_view_info =
        std::make_unique<DownloadBubbleSecurityViewInfo>();
    auto bubble_navigator =
        std::make_unique<MockDownloadBubbleNavigationHandler>(
            *security_view_info);
    auto security_view = std::make_unique<DownloadBubbleSecurityView>(
        security_view_delegate_.get(), *security_view_info,
        bubble_navigator->GetWeakPtr(), bubble_delegate_);
    security_view_info->InitializeForDownload(*row1_model_);

    security_view->MaybeLogDismiss();
    ++item1_actions_expected;

    std::vector<WarningActionEvent> events =
        DownloadItemWarningData::GetWarningActionEvents(&download_item1_);
    ASSERT_EQ(events.size(), item1_actions_expected);
    EXPECT_EQ(events.back().action, WarningAction::DISMISS);
  }

  // Dismiss action logged after update
  {
    auto security_view_info =
        std::make_unique<DownloadBubbleSecurityViewInfo>();
    auto bubble_navigator =
        std::make_unique<MockDownloadBubbleNavigationHandler>(
            *security_view_info);
    auto security_view = std::make_unique<DownloadBubbleSecurityView>(
        security_view_delegate_.get(), *security_view_info,
        bubble_navigator->GetWeakPtr(), bubble_delegate_);
    security_view_info->InitializeForDownload(*row1_model_);

    security_view->BackButtonPressed();
    ++item1_actions_expected;

    std::vector<WarningActionEvent> events =
        DownloadItemWarningData::GetWarningActionEvents(&download_item1_);
    ASSERT_EQ(events.size(), item1_actions_expected);
    EXPECT_EQ(events.back().action, WarningAction::BACK);

    // The security view can be re-opened after back button is pressed.
    security_view_info->InitializeForDownload(*row1_model_);

    security_view->MaybeLogDismiss();
    ++item1_actions_expected;

    events = DownloadItemWarningData::GetWarningActionEvents(&download_item1_);
    ASSERT_EQ(events.size(), item1_actions_expected);
    EXPECT_EQ(events.back().action, WarningAction::DISMISS);
  }

  // Dismiss action not logged after update to a different download.
  {
    auto security_view_info =
        std::make_unique<DownloadBubbleSecurityViewInfo>();
    auto bubble_navigator =
        std::make_unique<MockDownloadBubbleNavigationHandler>(
            *security_view_info);
    auto security_view = std::make_unique<DownloadBubbleSecurityView>(
        security_view_delegate_.get(), *security_view_info,
        bubble_navigator->GetWeakPtr(), bubble_delegate_);
    security_view_info->InitializeForDownload(*row1_model_);

    security_view->BackButtonPressed();
    ++item1_actions_expected;

    std::vector<WarningActionEvent> events =
        DownloadItemWarningData::GetWarningActionEvents(&download_item1_);
    ASSERT_EQ(events.size(), item1_actions_expected);
    EXPECT_EQ(events.back().action, WarningAction::BACK);

    // The security view can be re-opened for a different download after back
    // button is pressed.
    DownloadItemWarningData::AddWarningActionEvent(
        &download_item2_, WarningSurface::BUBBLE_MAINPAGE,
        WarningAction::SHOWN);
    security_view_info->InitializeForDownload(*row2_model_);

    // Since the reset occurs while we are showing download2, we don't log
    // DISMISS for the first download.
    security_view->MaybeLogDismiss();
    ++item2_actions_expected;

    events = DownloadItemWarningData::GetWarningActionEvents(&download_item1_);
    EXPECT_EQ(events.size(), item1_actions_expected);

    // Instead, DISMISS is logged for the second download.
    events = DownloadItemWarningData::GetWarningActionEvents(&download_item2_);
    ASSERT_EQ(events.size(), item2_actions_expected);
    EXPECT_EQ(events[0].action, WarningAction::DISMISS);
  }

  // Dismiss action not logged when download is removed.
  {
    auto security_view_info =
        std::make_unique<DownloadBubbleSecurityViewInfo>();
    auto bubble_navigator =
        std::make_unique<MockDownloadBubbleNavigationHandler>(
            *security_view_info);
    auto security_view = std::make_unique<DownloadBubbleSecurityView>(
        security_view_delegate_.get(), *security_view_info,
        bubble_navigator->GetWeakPtr(), bubble_delegate_);
    security_view_info->InitializeForDownload(*row1_model_);

    // No action is logged upon removal of download.
    download_item1_.Remove();
    download_item1_.NotifyObserversDownloadRemoved();

    // No action is logged upon reset because we are no longer initialized.
    security_view.reset();

    std::vector<WarningActionEvent> events =
        DownloadItemWarningData::GetWarningActionEvents(&download_item1_);
    EXPECT_EQ(events.size(), item1_actions_expected);
  }
}

TEST_F(DownloadBubbleSecurityViewTest, ResizesOnUpdate) {
  // This test simulates the deep scanning flow. The prompt for scanning is
  // wider than the scan in progress view. The bubble should be able to scale up
  // and down in these transitions.
  security_view_info_->InitializeForDownload(*row1_model_);
  security_view_info_->SetSubpageButtonsForTesting(
      {SubpageButton(DownloadCommands::Command::DISCARD, std::u16string(),
                     /*is_prominent=*/true)});
  UpdateView();

  int short_width =
      bubble_delegate_->GetDialogClientView()->GetMinimumSize().width();

  security_view_->Reset();
  security_view_info_->InitializeForDownload(*row1_model_);
  security_view_info_->SetSubpageButtonsForTesting({SubpageButton(
      DownloadCommands::Command::DISCARD,
      std::u16string(u"really really really really really really long "
                     u"button text"),
      /*is_prominent=*/true)});
  UpdateView();
  int medium_width =
      bubble_delegate_->GetDialogClientView()->GetMinimumSize().width();

  ASSERT_LT(short_width, medium_width);

  security_view_->Reset();
  security_view_info_->InitializeForDownload(*row1_model_);
  security_view_info_->SetSubpageButtonsForTesting(
      {SubpageButton(DownloadCommands::Command::DISCARD, std::u16string(),
                     /*is_prominent=*/true)});
  UpdateView();
  EXPECT_EQ(short_width,
            bubble_delegate_->GetDialogClientView()->GetMinimumSize().width());
}

TEST_F(DownloadBubbleSecurityViewTest, ProcessButtonClick) {
  security_view_info_->InitializeForDownload(*row1_model_);
  EXPECT_TRUE(
      security_view_->ProcessButtonClick(DownloadCommands::Command::DISCARD,
                                         /*is_secondary_button=*/false));

  EXPECT_FALSE(
      security_view_->ProcessButtonClick(DownloadCommands::Command::DEEP_SCAN,
                                         /*is_secondary_button=*/false));
}

TEST_F(DownloadBubbleSecurityViewTest, InitializeAndReset) {
  security_view_info_->InitializeForDownload(*row1_model_);
  EXPECT_TRUE(security_view_->IsInitialized());
  EXPECT_EQ(security_view_->content_id(),
            OfflineItemUtils::GetContentIdForDownload(&download_item1_));

  // Reset and initialize with the other download.
  security_view_->Reset();
  security_view_info_->InitializeForDownload(*row2_model_);
  EXPECT_TRUE(security_view_->IsInitialized());
  EXPECT_EQ(security_view_->content_id(),
            OfflineItemUtils::GetContentIdForDownload(&download_item2_));

  // Initialize directly to a different download without resetting.
  security_view_info_->InitializeForDownload(*row1_model_);
  EXPECT_TRUE(security_view_->IsInitialized());
  EXPECT_EQ(security_view_->content_id(),
            OfflineItemUtils::GetContentIdForDownload(&download_item1_));

  // Initialize to the same download without resetting.
  security_view_info_->InitializeForDownload(*row1_model_);
  EXPECT_TRUE(security_view_->IsInitialized());
  EXPECT_EQ(security_view_->content_id(),
            OfflineItemUtils::GetContentIdForDownload(&download_item1_));
}

TEST_F(DownloadBubbleSecurityViewTest, ReturnToPrimaryDialog) {
  ON_CALL(download_item2_, GetDangerType())
      .WillByDefault(
          Return(download::DOWNLOAD_DANGER_TYPE_DEEP_SCANNED_FAILED));
  security_view_info_->InitializeForDownload(*row1_model_);
  EXPECT_TRUE(security_view_->IsInitialized());
  EXPECT_EQ(security_view_->content_id(),
            OfflineItemUtils::GetContentIdForDownload(&download_item1_));

  // Initialize to a different download without resetting.
  security_view_info_->InitializeForDownload(*row2_model_);

  // Because the new item's danger type is a terminal state of a deep scan,
  // we should return to the primary dialog and reset the security view.
  EXPECT_FALSE(security_view_->IsInitialized());
  EXPECT_FALSE(security_view_info_->content_id().has_value());
  EXPECT_EQ(*bubble_navigator_->last_opened_page(),
            DownloadBubbleContentsView::Page::kPrimary);
}

// Test that an update with an insecure download status does not cause us to
// return to the primary dialog.
TEST_F(DownloadBubbleSecurityViewTest, InsecureDontReturnToPrimaryDialog) {
  security_view_info_->InitializeForDownload(*row1_model_);
  ASSERT_TRUE(security_view_info_->HasSubpage());
  EXPECT_TRUE(security_view_->IsInitialized());
  EXPECT_EQ(security_view_->content_id(),
            OfflineItemUtils::GetContentIdForDownload(&download_item1_));

  // Update the download to be insecure but not dangerous.
  EXPECT_CALL(download_item1_, GetDangerType())
      .WillRepeatedly(Return(
          download::DownloadDangerType::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS));
  EXPECT_CALL(download_item1_, GetInsecureDownloadStatus())
      .WillRepeatedly(
          Return(download::DownloadItem::InsecureDownloadStatus::BLOCK));
  EXPECT_CALL(download_item1_, GetURL())
      .WillRepeatedly(ReturnRefOfCopy(GURL("http://insecure.com/a.exe")));
  download_item1_.NotifyObserversDownloadUpdated();

  ASSERT_TRUE(security_view_info_->HasSubpage());
  EXPECT_TRUE(security_view_->IsInitialized());
  EXPECT_EQ(security_view_->content_id(),
            OfflineItemUtils::GetContentIdForDownload(&download_item1_));
  // The update did not cause us to return to the primary page because there is
  // a subpage.
  EXPECT_FALSE(bubble_navigator_->last_opened_page());
}

// Test that an update where the new state does not have a subpage causes us to
// return to the primary dialog.
TEST_F(DownloadBubbleSecurityViewTest, ReturnToPrimaryDialogNoSubpage) {
  security_view_info_->InitializeForDownload(*row1_model_);
  ASSERT_TRUE(security_view_info_->HasSubpage());
  EXPECT_TRUE(security_view_->IsInitialized());
  EXPECT_EQ(security_view_->content_id(),
            OfflineItemUtils::GetContentIdForDownload(&download_item1_));

  // Update the download.
  EXPECT_CALL(download_item1_, GetDangerType())
      .WillRepeatedly(Return(
          download::DownloadDangerType::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS));
  download_item1_.NotifyObserversDownloadUpdated();

  ASSERT_FALSE(security_view_info_->HasSubpage());
  EXPECT_FALSE(security_view_->IsInitialized());
  EXPECT_FALSE(security_view_info_->content_id().has_value());
  EXPECT_EQ(DownloadBubbleContentsView::Page::kPrimary,
            *bubble_navigator_->last_opened_page());
}

// Test validating a dangerous download, such that it goes from having
// a UI info subpage to not having one. See crbug.com/1478390.
TEST_F(DownloadBubbleSecurityViewTest, ValidateDangerousDownload) {
  security_view_info_->InitializeForDownload(*row1_model_);
  ASSERT_TRUE(security_view_info_->HasSubpage());
  EXPECT_TRUE(security_view_->IsInitialized());
  EXPECT_EQ(security_view_->content_id(),
            OfflineItemUtils::GetContentIdForDownload(&download_item1_));

  // "Validate" the download.
  ON_CALL(download_item1_, GetDangerType())
      .WillByDefault(Return(
          download::DownloadDangerType::DOWNLOAD_DANGER_TYPE_USER_VALIDATED));
  download_item1_.NotifyObserversDownloadUpdated();

  ASSERT_FALSE(security_view_info_->HasSubpage());
  EXPECT_FALSE(security_view_->IsInitialized());
  EXPECT_FALSE(security_view_info_->content_id().has_value());
  EXPECT_EQ(DownloadBubbleContentsView::Page::kPrimary,
            *bubble_navigator_->last_opened_page());
}
