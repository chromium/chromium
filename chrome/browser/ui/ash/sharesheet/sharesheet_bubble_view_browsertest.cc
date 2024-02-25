// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/sharesheet/sharesheet_bubble_view.h"

#include <algorithm>

#include "ash/shell.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_base.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/app_service_test.h"
#include "chrome/browser/ash/policy/dlp/dlp_files_controller_ash.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
#include "chrome/browser/chromeos/policy/dlp/test/mock_dlp_rules_manager.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sharesheet/sharesheet_metrics.h"
#include "chrome/browser/sharesheet/sharesheet_service.h"
#include "chrome/browser/sharesheet/sharesheet_service_factory.h"
#include "chrome/browser/sharesheet/sharesheet_test_util.h"
#include "chrome/browser/sharesheet/sharesheet_types.h"
#include "chrome/browser/ui/ash/sharesheet/sharesheet_bubble_view_delegate.h"
#include "chrome/browser/ui/ash/sharesheet/sharesheet_target_button.h"
#include "chrome/browser/ui/ash/sharesheet/sharesheet_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/components/sharesheet/constants.h"
#include "components/services/app_service/public/cpp/intent_test_util.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace ash {
namespace sharesheet {

class SharesheetBubbleViewBrowserTest
    : public ::testing::WithParamInterface<bool>,
      public InProcessBrowserTest {
 public:
  SharesheetBubbleViewBrowserTest() {
    if (GetParam()) {
      scoped_feature_list_.InitAndEnableFeature(::features::kNearbySharing);
    } else {
      scoped_feature_list_.InitAndDisableFeature(::features::kNearbySharing);
    }
  }

  void ShowUi() {
    views::Widget::Widgets old_widgets;
    for (aura::Window* root_window : Shell::GetAllRootWindows())
      views::Widget::GetAllChildWidgets(root_window, &old_widgets);

    ::sharesheet::SharesheetService* const sharesheet_service =
        ::sharesheet::SharesheetServiceFactory::GetForProfile(
            browser()->profile());

    auto intent = apps_util::MakeShareIntent("text", "");
    intent->action = apps_util::kIntentActionSend;
    sharesheet_service->ShowBubble(
        browser()->tab_strip_model()->GetActiveWebContents(), std::move(intent),
        ::sharesheet::LaunchSource::kUnknown, base::DoNothing(),
        base::DoNothing());

    views::Widget::Widgets new_widgets;
    for (aura::Window* root_window : Shell::GetAllRootWindows())
      views::Widget::GetAllChildWidgets(root_window, &new_widgets);

    views::Widget::Widgets added_widgets;
    std::set_difference(new_widgets.begin(), new_widgets.end(),
                        old_widgets.begin(), old_widgets.end(),
                        std::inserter(added_widgets, added_widgets.begin()));
    ASSERT_EQ(added_widgets.size(), 1u);
    sharesheet_widget_ = *added_widgets.begin();
    ASSERT_EQ(sharesheet_widget_->GetName(), "SharesheetBubbleView");
  }

  bool VerifyUi() {
    if (sharesheet_widget_) {
      return sharesheet_widget_->IsVisible();
    }
    return false;
  }

  void DismissUi() {
    ASSERT_TRUE(sharesheet_widget_);
    sharesheet_widget_->Close();
    ASSERT_FALSE(sharesheet_widget_->IsVisible());
  }

 protected:
  raw_ptr<views::Widget, DanglingUntriaged> sharesheet_widget_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         SharesheetBubbleViewBrowserTest,
                         ::testing::Bool());

IN_PROC_BROWSER_TEST_P(SharesheetBubbleViewBrowserTest, InvokeUi_Default) {
  ShowUi();
  ASSERT_TRUE(VerifyUi());
  DismissUi();
}

class SharesheetBubbleViewPolicyBrowserTest
    : public SharesheetBubbleViewBrowserTest {
 public:
  class MockFilesController : public policy::DlpFilesControllerAsh {
   public:
    explicit MockFilesController(const policy::DlpRulesManager& rules_manager,
                                 Profile* profile)
        : DlpFilesControllerAsh(rules_manager, profile) {}
    ~MockFilesController() override = default;

    MOCK_METHOD(bool,
                IsLaunchBlocked,
                (const apps::AppUpdate&, const apps::IntentPtr&),
                (override));
  };

  void TearDownOnMainThread() override {
    // Make sure the rules manager does not return a freed files controller.
    ON_CALL(*rules_manager_, GetDlpFilesController)
        .WillByDefault(testing::Return(nullptr));

    // The files controller must be destroyed before the profile since it's
    // holding a pointer to it.
    mock_files_controller_.reset();

    SharesheetBubbleViewBrowserTest::TearDownOnMainThread();
  }

  void SetupRulesManager(bool is_dlp_blocked) {
    policy::DlpRulesManagerFactory::GetInstance()->SetTestingFactory(
        browser()->profile(),
        base::BindRepeating(
            &SharesheetBubbleViewPolicyBrowserTest::SetDlpRulesManager,
            base::Unretained(this)));
    ASSERT_TRUE(policy::DlpRulesManagerFactory::GetForPrimaryProfile());

    ON_CALL(*rules_manager_, IsFilesPolicyEnabled)
        .WillByDefault(testing::Return(true));

    EXPECT_CALL(*mock_files_controller_.get(), IsLaunchBlocked)
        .WillOnce(testing::Return(is_dlp_blocked));
  }

  void SetupAppService() {
    app_service_test_.SetUp(browser()->profile());

    AddAppServiceAppsForTesting("arcAppId", apps::AppType::kArc, "text/plain",
                                "https://example.com");
  }

  void AddAppServiceAppsForTesting(std::string app_id,
                                   apps::AppType app_type,
                                   std::string mime_type,
                                   std::optional<std::string> publisher_id) {
    apps::AppServiceProxy* app_service_proxy =
        apps::AppServiceProxyFactory::GetForProfile(browser()->profile());

    std::vector<apps::AppPtr> fake_apps;
    apps::AppPtr fake_app =
        std::make_unique<apps::App>(apps::AppType::kArc, app_id);
    fake_app->name = "xyz";
    fake_app->show_in_management = true;
    fake_app->readiness = apps::Readiness::kReady;
    if (publisher_id.has_value()) {
      fake_app->publisher_id = publisher_id.value();
    }
    std::vector<apps::PermissionPtr> fake_permissions;
    fake_app->permissions = std::move(fake_permissions);
    fake_app->handles_intents = true;
    apps::IntentFilterPtr filter =
        apps_util::MakeIntentFilterForMimeType(mime_type);
    fake_app->intent_filters.push_back(std::move(filter));

    fake_apps.push_back(std::move(fake_app));

    app_service_proxy->OnApps(std::move(fake_apps), app_type,
                              /*should_notify_initialized=*/false);
  }

  bool VerifyDlp(bool is_dlp_blocked) {
    if (!sharesheet_widget_) {
      return false;
    }
    SharesheetBubbleView* sharesheet_bubble_view_ =
        static_cast<SharesheetBubbleView*>(
            sharesheet_widget_->GetContentsView());
    views::View* targets = sharesheet_bubble_view_->GetViewByID(
        SharesheetViewID::TARGETS_DEFAULT_VIEW_ID);
    SharesheetTargetButton* button = static_cast<SharesheetTargetButton*>(
        targets->children()[targets->children().size() - 1]);
    return is_dlp_blocked ==
           (button->GetState() == SharesheetTargetButton::STATE_DISABLED);
  }

  MockFilesController* mock_files_controller() {
    return mock_files_controller_.get();
  }

 private:
  std::unique_ptr<KeyedService> SetDlpRulesManager(
      content::BrowserContext* context) {
    auto dlp_rules_manager =
        std::make_unique<testing::NiceMock<policy::MockDlpRulesManager>>(
            Profile::FromBrowserContext(context));
    rules_manager_ = dlp_rules_manager.get();

    mock_files_controller_ = std::make_unique<MockFilesController>(
        *rules_manager_, Profile::FromBrowserContext(context));
    ON_CALL(*rules_manager_, GetDlpFilesController)
        .WillByDefault(testing::Return(mock_files_controller_.get()));

    return dlp_rules_manager;
  }

  apps::AppServiceTest app_service_test_;
  raw_ptr<policy::MockDlpRulesManager, DanglingUntriaged> rules_manager_ =
      nullptr;
  std::unique_ptr<MockFilesController> mock_files_controller_ = nullptr;
};

INSTANTIATE_TEST_SUITE_P(All,
                         SharesheetBubbleViewPolicyBrowserTest,
                         ::testing::Bool());

IN_PROC_BROWSER_TEST_P(SharesheetBubbleViewPolicyBrowserTest,
                       InvokeUi_DlpAllowed) {
  SetupRulesManager(/*is_dlp_blocked*/ false);
  SetupAppService();
  ShowUi();
  ASSERT_TRUE(VerifyDlp(/*is_dlp_blocked*/ false));
  DismissUi();
}

IN_PROC_BROWSER_TEST_P(SharesheetBubbleViewPolicyBrowserTest,
                       InvokeUi_DlpBlocked) {
  SetupRulesManager(/*is_dlp_blocked*/ true);
  SetupAppService();
  ShowUi();
  ASSERT_TRUE(VerifyDlp(/*is_dlp_blocked*/ true));
  DismissUi();
}

class SharesheetBubbleViewNearbyShareBrowserTest : public InProcessBrowserTest {
 public:
  SharesheetBubbleViewNearbyShareBrowserTest() = default;

  ~SharesheetBubbleViewNearbyShareBrowserTest() override = default;

  SharesheetBubbleView* sharesheet_bubble_view() {
    return sharesheet_bubble_view_;
  }

  void ShowNearbyShareBubble() {
    gfx::NativeWindow parent_window = browser()
                                          ->tab_strip_model()
                                          ->GetActiveWebContents()
                                          ->GetTopLevelNativeWindow();
    ::sharesheet::SharesheetService* const sharesheet_service =
        ::sharesheet::SharesheetServiceFactory::GetForProfile(
            browser()->profile());
    sharesheet_service->ShowNearbyShareBubbleForArc(
        parent_window, ::sharesheet::CreateValidTextIntent(),
        ::sharesheet::LaunchSource::kArcNearbyShare,
        /*delivered_callback=*/base::DoNothing(),
        /*close_callback=*/base::DoNothing(),
        /*cleanup_callback=*/base::DoNothing());
    bubble_delegate_ = static_cast<SharesheetBubbleViewDelegate*>(
        sharesheet_service->GetUiDelegateForTesting(parent_window));
    EXPECT_NE(bubble_delegate_, nullptr);
    sharesheet_bubble_view_ = bubble_delegate_->GetBubbleViewForTesting();
    EXPECT_NE(sharesheet_bubble_view_, nullptr);
    EXPECT_EQ(sharesheet_bubble_view_->GetID(), SHARESHEET_BUBBLE_VIEW_ID);

    EXPECT_TRUE(bubble_delegate_->IsBubbleVisible());
    auto* sharesheet_widget = sharesheet_bubble_view_->GetWidget();
    EXPECT_EQ(sharesheet_widget->GetName(), "SharesheetBubbleView");
    EXPECT_TRUE(sharesheet_widget->IsVisible());
  }

  void CloseBubble() {
    bubble_delegate_->CloseBubble(::sharesheet::SharesheetResult::kCancel);
    // |bubble_delegate_| and |sharesheet_bubble_view_| destruct on close.
    bubble_delegate_ = nullptr;
    sharesheet_bubble_view_ = nullptr;
  }

 private:
  raw_ptr<SharesheetBubbleViewDelegate> bubble_delegate_;
  raw_ptr<SharesheetBubbleView> sharesheet_bubble_view_;
};

IN_PROC_BROWSER_TEST_F(SharesheetBubbleViewNearbyShareBrowserTest,
                       ShowNearbyShareBubbleForArc) {
  base::HistogramTester histograms;

  ShowNearbyShareBubble();

  histograms.ExpectBucketCount(
      ::sharesheet::kSharesheetLaunchSourceResultHistogram,
      ::sharesheet::LaunchSource::kArcNearbyShare, 1);

  views::View* share_action_view = sharesheet_bubble_view()->GetViewByID(
      SharesheetViewID::SHARE_ACTION_VIEW_ID);
  ASSERT_TRUE(share_action_view->GetVisible());

  CloseBubble();
}

}  // namespace sharesheet
}  // namespace ash
