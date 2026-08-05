// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/to_vector.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/content_setting_bubble_contents.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/permissions/chip/chip_controller.h"
#include "chrome/browser/ui/views/permissions/chip/permission_chip_view.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_chip.h"
#include "chrome/browser/ui/views/tabs/common/tab_strip_view.h"
#include "chrome/browser/ui/window_metadata/window_metadata_controller.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar_manager.h"
#include "components/permissions/permission_prompt.h"
#include "components/permissions/permission_request_enums.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/prediction_service/permission_ui_selector.h"
#include "components/permissions/resolvers/permission_prompt_options.h"
#include "components/permissions/test/enums_to_string.h"
#include "components/permissions/test/mock_permission_request.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/view_utils.h"

namespace {
using ::content::RenderFrameHost;
using ::content::WebContents;
using QuietUiReason = ::permissions::PermissionUiSelector::QuietUiReason;
using ::base::test::ScopedFeatureList;
using ::infobars::InfoBar;
using ::infobars::InfoBarManager;
using ::testing::_;
using ::testing::Combine;
using ::testing::Values;

class InfoBarObserver : public InfoBarManager::Observer {
 public:
  explicit InfoBarObserver(WebContents* web_contents)
      : web_contents_(web_contents) {
    infobars::ContentInfoBarManager* infobar_manager =
        infobars::ContentInfoBarManager::FromWebContents(web_contents);
    CHECK(infobar_manager != nullptr);
    infobar_manager->AddObserver(this);
  }
  ~InfoBarObserver() override {
    infobars::ContentInfoBarManager* infobar_manager =
        infobars::ContentInfoBarManager::FromWebContents(web_contents_);
    CHECK(infobar_manager != nullptr);
    infobar_manager->RemoveObserver(this);
  }
  void OnInfoBarAdded(InfoBar* infobar) override { info_bar_added_ = true; }
  void OnInfoBarRemoved(InfoBar* infobar, bool animate) override {}
  void OnInfoBarReplaced(InfoBar* old_infobar, InfoBar* new_infobar) override {}
  void OnManagerWillBeDestroyed(InfoBarManager* manager) override {}

  bool info_bar_added_ = false;

 private:
  raw_ptr<WebContents> web_contents_;
};
}  // namespace

namespace test {
class MockPermissionRequestManager
    : public permissions::PermissionRequestManager {
 private:
  explicit MockPermissionRequestManager(
      const GURL& origin,
      const std::vector<permissions::RequestType> request_types,
      bool with_gesture,
      WebContents* web_contents)
      : MockPermissionRequestManager(origin,
                                     request_types,
                                     with_gesture,
                                     /*quiet_ui_reason=*/std::nullopt,
                                     web_contents) {}

  explicit MockPermissionRequestManager(
      const GURL& origin,
      const std::vector<permissions::RequestType> request_types,
      bool with_gesture,
      std::optional<QuietUiReason> quiet_ui_reason,
      WebContents* web_contents)
      : permissions::PermissionRequestManager(web_contents),
        quiet_ui_reason_(quiet_ui_reason),
        web_contents_(web_contents) {
    requests_ = base::ToVector(
        request_types,
        [&](auto request_type)
            -> std::unique_ptr<permissions::PermissionRequest> {
          return std::make_unique<permissions::MockPermissionRequest>(
              origin, request_type,
              with_gesture
                  ? permissions::PermissionRequestGestureType::GESTURE
                  : permissions::PermissionRequestGestureType::NO_GESTURE);
        });
    raw_requests_ = base::ToVector(
        requests_,
        [](const auto& request)
            -> raw_ptr<permissions::PermissionRequest, VectorExperimental> {
          return request.get();
        });

    requests_[0]->set_requesting_frame_id(
        web_contents->GetPrimaryMainFrame()->GetGlobalId());

    ON_CALL(*this, Dismiss).WillByDefault([]() { NOTREACHED(); });
    ON_CALL(*this, Deny).WillByDefault([]() { NOTREACHED(); });
    ON_CALL(*this, Ignore).WillByDefault([]() { NOTREACHED(); });
    ON_CALL(*this, Accept).WillByDefault([]() { NOTREACHED(); });
    ON_CALL(*this, PreIgnoreQuietPrompt).WillByDefault([]() { NOTREACHED(); });
    ON_CALL(*this, FinalizeCurrentRequests).WillByDefault([]() {
      NOTREACHED();
    });
  }

 public:
  ~MockPermissionRequestManager() override { requests_.clear(); }
  static MockPermissionRequestManager* CreateForWebContents(
      const GURL& origin,
      const std::vector<permissions::RequestType> request_types,
      bool with_gesture,
      WebContents* web_contents) {
    web_contents->SetUserData(
        UserDataKey(),
        base::WrapUnique(static_cast<PermissionRequestManager*>(
            new MockPermissionRequestManager(origin, request_types,
                                             with_gesture, web_contents))));
    return static_cast<MockPermissionRequestManager*>(
        permissions::PermissionRequestManager::FromWebContents(web_contents));
  }

  static MockPermissionRequestManager* CreateForWebContents(
      const GURL& origin,
      const std::vector<permissions::RequestType> request_types,
      bool with_gesture,
      std::optional<QuietUiReason> quiet_ui_reason,
      WebContents* web_contents) {
    web_contents->SetUserData(
        UserDataKey(), base::WrapUnique(static_cast<PermissionRequestManager*>(
                           new MockPermissionRequestManager(
                               origin, request_types, with_gesture,
                               quiet_ui_reason, web_contents))));
    return static_cast<MockPermissionRequestManager*>(
        permissions::PermissionRequestManager::FromWebContents(web_contents));
  }

  const std::vector<std::unique_ptr<permissions::PermissionRequest>>& Requests()
      override {
    return requests_;
  }

  GURL GetRequestingOrigin() const override {
    return raw_requests_.front()->requesting_origin();
  }

  GURL GetEmbeddingOrigin() const override {
    return GURL("https://embedder.example.com");
  }

  MOCK_METHOD(void, Dismiss, (const PromptOptions&), (override));
  MOCK_METHOD(void, Deny, (const PromptOptions&), (override));
  MOCK_METHOD(void, Ignore, (const PromptOptions&), (override));
  MOCK_METHOD(void, Accept, (const PromptOptions&), (override));
  MOCK_METHOD(void, PreIgnoreQuietPrompt, (), (override));
  MOCK_METHOD(void, FinalizeCurrentRequests, (), (override));

  void OpenHelpCenterLink(const ui::Event& event) override {}
  void SetManageClicked() override { requests_.clear(); }
  void SetLearnMoreClicked() override { requests_.clear(); }
  void SetHatsShownCallback(base::OnceCallback<void()> callback) override {}

  bool RecreateView() override { return false; }
  const permissions::PermissionPrompt* GetCurrentPrompt() const override {
    return nullptr;
  }

  bool WasCurrentRequestAlreadyDisplayed() override {
    return was_current_request_already_displayed_;
  }
  bool ShouldDropCurrentRequestIfCannotShowQuietly() const override {
    return false;
  }
  bool ShouldCurrentRequestUseQuietUI() const override {
    return quiet_ui_reason_.has_value();
  }
  std::optional<QuietUiReason> ReasonForUsingQuietUi() const override {
    return quiet_ui_reason_;
  }
  void SetDismissOnTabClose() override {}
  void SetPromptShown() override {}
  void SetDecisionTime() override {}

  base::WeakPtr<permissions::PermissionPrompt::Delegate> GetWeakPtr() override {
    return weak_factory_.GetWeakPtr();
  }

  WebContents* GetAssociatedWebContents() override { return web_contents_; }

  bool IsRequestInProgress() { return !requests_.empty(); }

  void SetAlreadyDisplayed() { was_current_request_already_displayed_ = true; }

  void ClearRequests() { requests_.clear(); }

  void SetView(std::unique_ptr<permissions::PermissionPrompt> view) {
    view_ = std::move(view);
  }

 private:
  std::vector<raw_ptr<permissions::PermissionRequest, VectorExperimental>>
      raw_requests_;
  bool was_current_request_already_displayed_ = false;
  std::optional<QuietUiReason> quiet_ui_reason_;
  raw_ptr<WebContents> web_contents_;
  base::WeakPtrFactory<MockPermissionRequestManager> weak_factory_{this};
};
}  // namespace test

class PermissionChipBrowserTest : public InProcessBrowserTest {
 public:
  PermissionChipBrowserTest()
      : animation_mode_reset_(gfx::AnimationTestApi::SetRichAnimationRenderMode(
            gfx::Animation::RichAnimationRenderMode::FORCE_ENABLED)) {}

  PermissionChipBrowserTest(const PermissionChipBrowserTest&) = delete;
  PermissionChipBrowserTest& operator=(const PermissionChipBrowserTest&) =
      delete;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL("/title1.html")));
    web_contents_ = browser()->tab_strip_model()->GetActiveWebContents();
  }

  BrowserView* browser_view() {
    return BrowserView::GetBrowserViewForBrowser(browser());
  }

  void ClickOnChip(ChipController* controller) {
    views::test::ButtonTestApi(
        views::AsViewClass<views::Button>(
            views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
                PermissionChipView::kPermissionRequestChipElementId,
                views::ElementTrackerViews::GetContextForView(browser_view()))))
        .NotifyClick(ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(),
                                    gfx::Point(), ui::EventTimeForNow(),
                                    ui::EF_LEFT_MOUSE_BUTTON, 0));
    base::RunLoop().RunUntilIdle();
  }

  void ClickOnAcceptPermissionRequestQuietChip(ChipController* controller) {
    auto* bubble = controller->GetContentSettingBubbleContentsForTesting();
    EXPECT_NE(bubble, nullptr);
    bubble->AsDialogDelegate()->Accept();
  }

  raw_ptr<WebContents, DanglingUntriaged> web_contents_;
  // Some of these tests rely on animation being enabled. This forces
  // animation on even if it's turned off in the OS.
  gfx::AnimationTestApi::RenderModeResetter animation_mode_reset_;
};

IN_PROC_BROWSER_TEST_F(PermissionChipBrowserTest, AlreadyDisplayedRequestTest) {
  auto& delegate = *test::MockPermissionRequestManager::CreateForWebContents(
      GURL("https://test.origin"), {permissions::RequestType::kNotifications},
      false, web_contents_);
  delegate.SetAlreadyDisplayed();

  EXPECT_TRUE(delegate.WasCurrentRequestAlreadyDisplayed());

  PermissionPromptChip chip_prompt(web_contents_, &delegate);
  ChipController* chip_controller =
      chip_prompt.get_chip_controller_for_testing();

  EXPECT_FALSE(chip_controller->IsBubbleShowing());

  // The permission request was already displayed, but the dismiss timer will
  // not be triggered directly after the chip is displayed because of the popup
  // bubble.
  EXPECT_FALSE(chip_controller->is_collapse_timer_running_for_testing());
  EXPECT_FALSE(chip_controller->is_dismiss_timer_running_for_testing());

  // All chips are feature auto popup bubble. They should not resolve a prompt
  // automatically.
  EXPECT_TRUE(delegate.IsRequestInProgress());
  delegate.ClearRequests();
}

IN_PROC_BROWSER_TEST_F(PermissionChipBrowserTest, AccessibleName) {
  auto& delegate = *test::MockPermissionRequestManager::CreateForWebContents(
      GURL("https://test.origin"), {permissions::RequestType::kNotifications},
      false, web_contents_);
  delegate.SetAlreadyDisplayed();

  EXPECT_TRUE(delegate.WasCurrentRequestAlreadyDisplayed());

  PermissionPromptChip chip_prompt(web_contents_, &delegate);
  ChipController* chip_controller =
      chip_prompt.get_chip_controller_for_testing();

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), embedded_test_server()->GetURL("/title2.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  std::u16string tab_title =
      WindowMetadataController::From(browser())->GetTitleForTab(
          browser()->tab_strip_model()->GetTabAtIndex(0)->GetHandle());
  std::u16string permission_title = l10n_util::GetStringFUTF16(
      IDS_TAB_AX_LABEL_PERMISSION_REQUESTED_FORMAT, tab_title);

  chip_controller->ShowPermissionUi(delegate.GetWeakPtr());
  chip_controller->AnimateExpand();
  ui::AXNodeData data;
  browser_view()
      ->tab_strip_view()
      ->GetTabAnchorViewAt(0)
      ->GetViewAccessibility()
      .GetAccessibleNodeData(&data);
  EXPECT_TRUE(chip_controller->IsPermissionPromptChipVisible());
  EXPECT_EQ(permission_title,
            data.GetString16Attribute(ax::mojom::StringAttribute::kName));

  chip_controller->HideChip();
  data = ui::AXNodeData();
  browser_view()
      ->tab_strip_view()
      ->GetTabAnchorViewAt(0)
      ->GetViewAccessibility()
      .GetAccessibleNodeData(&data);
  EXPECT_FALSE(chip_controller->IsPermissionPromptChipVisible());
  EXPECT_EQ(tab_title,
            data.GetString16Attribute(ax::mojom::StringAttribute::kName));

  delegate.ClearRequests();
}

IN_PROC_BROWSER_TEST_F(PermissionChipBrowserTest, ClickOnRequestChipTest) {
  auto& delegate = *test::MockPermissionRequestManager::CreateForWebContents(
      GURL("https://test.origin"), {permissions::RequestType::kNotifications},
      true, web_contents_);
  PermissionPromptChip chip_prompt(web_contents_, &delegate);
  ChipController* chip_controller =
      chip_prompt.get_chip_controller_for_testing();

  // Due to animation issue, the collapse timer will not be started.
  EXPECT_FALSE(chip_controller->is_collapse_timer_running_for_testing());
  EXPECT_FALSE(chip_controller->is_dismiss_timer_running_for_testing());

  EXPECT_TRUE(chip_controller->IsAnimating());
  views::AsViewClass<PermissionChipView>(
      views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
          PermissionChipView::kPermissionRequestChipElementId,
          views::ElementTrackerViews::GetContextForView(browser_view())))
      ->StopAnimationForTesting();
  chip_controller->OnExpandAnimationEnded();
  EXPECT_FALSE(chip_controller->IsAnimating());

  EXPECT_FALSE(chip_controller->is_collapse_timer_running_for_testing());
  EXPECT_FALSE(chip_controller->is_dismiss_timer_running_for_testing());

  EXPECT_TRUE(delegate.IsRequestInProgress());
  // Bubble is showing automatically.
  EXPECT_TRUE(chip_controller->IsBubbleShowing());

  EXPECT_FALSE(chip_controller->is_collapse_timer_running_for_testing());
  EXPECT_FALSE(chip_controller->is_dismiss_timer_running_for_testing());

  // A click on the chip hides the popup bubble and resolves a permission
  // request.
  EXPECT_CALL(delegate, Dismiss(_)).WillOnce([&delegate]() {
    delegate.ClearRequests();
  });
  ClickOnChip(chip_controller);
  EXPECT_FALSE(chip_controller->IsBubbleShowing());
  EXPECT_FALSE(delegate.IsRequestInProgress());
}

IN_PROC_BROWSER_TEST_F(PermissionChipBrowserTest,
                       DisplayQuietChipNoAbusiveTest) {
  auto& delegate = *test::MockPermissionRequestManager::CreateForWebContents(
      GURL("https://test.origin"), {permissions::RequestType::kNotifications},
      true, QuietUiReason::kEnabledInPrefs, web_contents_);
  EXPECT_CALL(delegate, PreIgnoreQuietPrompt()).WillOnce([&delegate]() {
    return delegate.PermissionRequestManager::PreIgnoreQuietPrompt();
  });
  PermissionPromptChip chip_prompt(web_contents_, &delegate);
  ChipController* chip_controller =
      chip_prompt.get_chip_controller_for_testing();

  EXPECT_FALSE(chip_controller->IsBubbleShowing());

  // Due to animation issue, the collapse timer will not be started.
  EXPECT_FALSE(chip_controller->is_collapse_timer_running_for_testing());
  EXPECT_FALSE(chip_controller->is_dismiss_timer_running_for_testing());

  EXPECT_TRUE(chip_controller->IsAnimating());
  views::AsViewClass<PermissionChipView>(
      views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
          PermissionChipView::kPermissionRequestChipElementId,
          views::ElementTrackerViews::GetContextForView(browser_view())))
      ->StopAnimationForTesting();
  chip_controller->OnExpandAnimationEnded();
  EXPECT_FALSE(chip_controller->IsAnimating());

  EXPECT_TRUE(chip_controller->is_collapse_timer_running_for_testing());
  EXPECT_FALSE(chip_controller->is_dismiss_timer_running_for_testing());
  EXPECT_TRUE(chip_controller->chip()->GetVisible());

  // Fire collapse timer and verify state transitions to dismiss timer.
  chip_controller->fire_collapse_timer_for_testing();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(delegate.IsRequestInProgress());

  // The collapse timer is fired and the dismiss timer is started.
  EXPECT_FALSE(chip_controller->is_collapse_timer_running_for_testing());
  EXPECT_TRUE(chip_controller->is_dismiss_timer_running_for_testing());
  EXPECT_TRUE(chip_controller->chip()->GetVisible());

  EXPECT_CALL(delegate, Ignore(_)).WillOnce([&delegate]() {
    delegate.ClearRequests();
  });
  chip_controller->fire_dismiss_timer_for_testing();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(chip_controller->chip()->GetVisible());
  EXPECT_FALSE(delegate.IsRequestInProgress());
}

IN_PROC_BROWSER_TEST_F(PermissionChipBrowserTest,
                       ClickOnQuietChipNoAbusiveTest) {
  auto& delegate = *test::MockPermissionRequestManager::CreateForWebContents(
      GURL("https://test.origin"), {permissions::RequestType::kNotifications},
      true, QuietUiReason::kEnabledInPrefs, web_contents_);
  EXPECT_CALL(delegate, PreIgnoreQuietPrompt()).WillOnce([&delegate]() {
    return delegate.PermissionRequestManager::PreIgnoreQuietPrompt();
  });
  PermissionPromptChip chip_prompt(web_contents_, &delegate);
  ChipController* chip_controller =
      chip_prompt.get_chip_controller_for_testing();

  views::AsViewClass<PermissionChipView>(
      views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
          PermissionChipView::kPermissionRequestChipElementId,
          views::ElementTrackerViews::GetContextForView(browser_view())))
      ->StopAnimationForTesting();
  chip_controller->OnExpandAnimationEnded();

  // Open a permission popup bubble.
  ClickOnChip(chip_controller);
  EXPECT_TRUE(chip_controller->IsBubbleShowing());

  // Collapse timer was restarted.
  EXPECT_FALSE(chip_controller->is_collapse_timer_running_for_testing());
  EXPECT_FALSE(chip_controller->is_dismiss_timer_running_for_testing());

  EXPECT_TRUE(chip_controller->IsBubbleShowing());
  EXPECT_TRUE(delegate.IsRequestInProgress());

  EXPECT_FALSE(chip_controller->is_collapse_timer_running_for_testing());
  EXPECT_FALSE(chip_controller->is_dismiss_timer_running_for_testing());

  EXPECT_CALL(delegate, Dismiss(_)).WillOnce([&delegate]() {
    delegate.ClearRequests();
  });
  // The second click on the chip hides the popup bubble.
  ClickOnChip(chip_controller);
  EXPECT_FALSE(chip_controller->IsBubbleShowing());
  EXPECT_FALSE(delegate.IsRequestInProgress());
}

IN_PROC_BROWSER_TEST_F(PermissionChipBrowserTest, DisplayQuietChipAbusiveTest) {
  auto& delegate = *test::MockPermissionRequestManager::CreateForWebContents(
      GURL("https://test.origin"), {permissions::RequestType::kNotifications},
      true, QuietUiReason::kTriggeredDueToAbusiveRequests, web_contents_);
  EXPECT_CALL(delegate, PreIgnoreQuietPrompt()).WillOnce([&delegate]() {
    return delegate.PermissionRequestManager::PreIgnoreQuietPrompt();
  });
  PermissionPromptChip chip_prompt(web_contents_, &delegate);
  ChipController* chip_controller =
      chip_prompt.get_chip_controller_for_testing();

  EXPECT_FALSE(chip_controller->IsBubbleShowing());

  // The quiet abusive chip does not have animation and will start the
  // dismiss timer immediately after displaying.
  EXPECT_FALSE(chip_controller->IsAnimating());
  EXPECT_FALSE(chip_controller->is_collapse_timer_running_for_testing());
  EXPECT_TRUE(chip_controller->is_dismiss_timer_running_for_testing());
  EXPECT_TRUE(chip_controller->chip()->GetVisible());

  EXPECT_CALL(delegate, Ignore(_)).WillOnce([&delegate]() {
    delegate.ClearRequests();
  });
  chip_controller->fire_dismiss_timer_for_testing();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(chip_controller->chip()->GetVisible());
  EXPECT_FALSE(delegate.IsRequestInProgress());
}

IN_PROC_BROWSER_TEST_F(PermissionChipBrowserTest, ClickOnQuietChipAbusiveTest) {
  auto& delegate = *test::MockPermissionRequestManager::CreateForWebContents(
      GURL("https://test.origin"), {permissions::RequestType::kNotifications},
      true, QuietUiReason::kTriggeredDueToAbusiveRequests, web_contents_);
  EXPECT_CALL(delegate, PreIgnoreQuietPrompt()).WillOnce([&delegate]() {
    return delegate.PermissionRequestManager::PreIgnoreQuietPrompt();
  });
  PermissionPromptChip chip_prompt(web_contents_, &delegate);
  ChipController* chip_controller =
      chip_prompt.get_chip_controller_for_testing();

  // Open a permission popup bubble.
  ClickOnChip(chip_controller);
  EXPECT_TRUE(chip_controller->IsBubbleShowing());

  EXPECT_FALSE(chip_controller->is_collapse_timer_running_for_testing());
  EXPECT_FALSE(chip_controller->is_dismiss_timer_running_for_testing());

  EXPECT_TRUE(delegate.IsRequestInProgress());
  EXPECT_TRUE(chip_controller->IsBubbleShowing());

  EXPECT_CALL(delegate, Dismiss(_)).WillOnce([&delegate]() {
    delegate.ClearRequests();
  });
  // The second click on the chip hides the prompt.
  ClickOnChip(chip_controller);
  EXPECT_FALSE(chip_controller->IsBubbleShowing());
  EXPECT_FALSE(delegate.IsRequestInProgress());
}

class PermissionPromiseLifetimeModulationTest
    : public PermissionChipBrowserTest {};

IN_PROC_BROWSER_TEST_F(PermissionPromiseLifetimeModulationTest,
                       NotificationsRequestsNotPreignoredForNonQuietPrompts) {
  auto& delegate = *test::MockPermissionRequestManager::CreateForWebContents(
      GURL("https://test.origin"), {permissions::RequestType::kNotifications},
      true,
      /*quiet_ui_reason=*/std::nullopt, web_contents_);

  PermissionPromptChip chip_prompt(web_contents_, &delegate);
  EXPECT_TRUE(delegate.IsRequestInProgress());
  delegate.ClearRequests();
}

IN_PROC_BROWSER_TEST_F(PermissionPromiseLifetimeModulationTest,
                       GeolocationRequestsNotPreignoredForNonQuietPrompts) {
  auto& delegate = *test::MockPermissionRequestManager::CreateForWebContents(
      GURL("https://test.origin"), {permissions::RequestType::kGeolocation},
      true, /*quiet_ui_reason=*/std::nullopt, web_contents_);

  PermissionPromptChip chip_prompt(web_contents_, &delegate);
  EXPECT_TRUE(delegate.IsRequestInProgress());
  delegate.ClearRequests();
}

template <class ParamType>
std::string TestNameGenerator(const testing::TestParamInfo<ParamType>& info) {
  return base::StrCat({test::ToString(std::get<0>(info.param)), "For",
                       test::ToString(std::get<1>(info.param))});
}

using QuietUiReasonsTestCase =
    std::tuple<permissions::RequestType, QuietUiReason>;

class QuietUiPreignoreTest
    : public PermissionPromiseLifetimeModulationTest,
      public testing::WithParamInterface<QuietUiReasonsTestCase> {};

INSTANTIATE_TEST_SUITE_P(
    TestCases,
    QuietUiPreignoreTest,
    Combine(Values(permissions::RequestType::kGeolocation,
                   permissions::RequestType::kNotifications),
            Values(QuietUiReason::kEnabledInPrefs,
                   QuietUiReason::kTriggeredByCrowdDeny,
                   QuietUiReason::kTriggeredDueToAbusiveRequests,
                   QuietUiReason::kTriggeredDueToAbusiveContent,
                   QuietUiReason::kServicePredictedVeryUnlikelyGrant,
                   QuietUiReason::kOnDevicePredictedVeryUnlikelyGrant,
                   QuietUiReason::kTriggeredDueToDisruptiveBehavior)),
    /*name_generator=*/
    TestNameGenerator<QuietUiPreignoreTest::ParamType>);

IN_PROC_BROWSER_TEST_P(QuietUiPreignoreTest,
                       PermissionRequestsForAllQuietUiReasonsGetPreignored) {
  auto [request_type, quiet_ui_reason] = GetParam();

  auto& delegate = *test::MockPermissionRequestManager::CreateForWebContents(
      GURL("https://test.origin"), {request_type}, true, quiet_ui_reason,
      web_contents_);

  EXPECT_CALL(delegate, PreIgnoreQuietPrompt()).WillOnce([&delegate]() {
    return delegate.PermissionRequestManager::PreIgnoreQuietPrompt();
  });
  PermissionPromptChip chip_prompt(web_contents_, &delegate);
  delegate.ClearRequests();
}

class QuietUiAbusiveRequestsTest
    : public PermissionPromiseLifetimeModulationTest,
      public testing::WithParamInterface<QuietUiReasonsTestCase> {};

INSTANTIATE_TEST_SUITE_P(
    TestCases,
    QuietUiAbusiveRequestsTest,
    Combine(Values(permissions::RequestType::kNotifications),
            Values(QuietUiReason::kTriggeredDueToAbusiveRequests,
                   QuietUiReason::kTriggeredDueToAbusiveContent,
                   QuietUiReason::kTriggeredDueToDisruptiveBehavior)),
    /*name_generator=*/
    TestNameGenerator<QuietUiAbusiveRequestsTest::ParamType>);

IN_PROC_BROWSER_TEST_P(QuietUiAbusiveRequestsTest, GetsDenied) {
  auto [request_type, quiet_ui_reason] = GetParam();
  InfoBarObserver infobar_observer(web_contents_);

  auto& delegate = *test::MockPermissionRequestManager::CreateForWebContents(
      GURL("https://test.origin"), {request_type}, true, quiet_ui_reason,
      web_contents_);

  EXPECT_CALL(delegate, PreIgnoreQuietPrompt()).WillOnce([&delegate]() {
    return delegate.PermissionRequestManager::PreIgnoreQuietPrompt();
  });
  PermissionPromptChip chip_prompt(web_contents_, &delegate);
  ChipController* chip_controller =
      chip_prompt.get_chip_controller_for_testing();

  // Open a permission popup bubble.
  ClickOnChip(chip_controller);
  ASSERT_TRUE(chip_controller->IsBubbleShowing());

  EXPECT_CALL(delegate, Deny(_)).WillOnce([&delegate]() {
    delegate.ClearRequests();
  });

  EXPECT_TRUE(delegate.IsRequestInProgress());
  ClickOnAcceptPermissionRequestQuietChip(chip_controller);
  EXPECT_FALSE(delegate.IsRequestInProgress());
}

class QuietUiNonAbusiveRequestsTest
    : public PermissionPromiseLifetimeModulationTest,
      public testing::WithParamInterface<QuietUiReasonsTestCase> {};

INSTANTIATE_TEST_SUITE_P(
    TestCases,
    QuietUiNonAbusiveRequestsTest,
    Combine(Values(permissions::RequestType::kGeolocation,
                   permissions::RequestType::kNotifications),
            Values(QuietUiReason::kEnabledInPrefs)),
    /*name_generator=*/
    TestNameGenerator<QuietUiNonAbusiveRequestsTest::ParamType>);

IN_PROC_BROWSER_TEST_P(QuietUiNonAbusiveRequestsTest, GetsAccepted) {
  auto [request_type, quiet_ui_reason] = GetParam();

  auto& delegate = *test::MockPermissionRequestManager::CreateForWebContents(
      GURL("https://test.origin"), {request_type}, true, quiet_ui_reason,
      web_contents_);

  EXPECT_CALL(delegate, PreIgnoreQuietPrompt()).WillOnce([&delegate]() {
    return delegate.PermissionRequestManager::PreIgnoreQuietPrompt();
  });
  PermissionPromptChip chip_prompt(web_contents_, &delegate);
  ChipController* chip_controller =
      chip_prompt.get_chip_controller_for_testing();

  // Open a permission popup bubble.
  ClickOnChip(chip_controller);
  ASSERT_TRUE(chip_controller->IsBubbleShowing());

  EXPECT_CALL(delegate, Accept(_)).WillOnce([&delegate]() {
    delegate.ClearRequests();
  });

  EXPECT_TRUE(delegate.IsRequestInProgress());
  ClickOnAcceptPermissionRequestQuietChip(chip_controller);
  EXPECT_FALSE(delegate.IsRequestInProgress());
}

class InfobarTest
    : public PermissionPromiseLifetimeModulationTest,
      public testing::WithParamInterface<permissions::RequestType> {};

INSTANTIATE_TEST_SUITE_P(
    TestCases,
    InfobarTest,
    testing::ValuesIn<permissions::RequestType>({
        permissions::RequestType::kNotifications,
        permissions::RequestType::kGeolocation,
    }),
    /*name_generator=*/
    [](const testing::TestParamInfo<InfobarTest::ParamType>& info) {
      return std::string(test::ToString(info.param));
    });

IN_PROC_BROWSER_TEST_P(InfobarTest, ShowInfobarIfNecessary) {
  base::HistogramTester histogram_tester;
  InfoBarObserver infobar_observer(web_contents_);

  auto& delegate = *test::MockPermissionRequestManager::CreateForWebContents(
      GURL("https://test.origin"), {GetParam()}, true,
      QuietUiReason::kEnabledInPrefs, web_contents_);

  EXPECT_CALL(delegate, PreIgnoreQuietPrompt()).WillOnce([&delegate]() {
    return delegate.PermissionRequestManager::PreIgnoreQuietPrompt();
  });

  auto chip_prompt =
      std::make_unique<PermissionPromptChip>(web_contents_, &delegate);
  ChipController* chip_controller =
      chip_prompt->get_chip_controller_for_testing();
  delegate.SetView(std::move(chip_prompt));

  // Open a permission popup bubble.
  ClickOnChip(chip_controller);
  ASSERT_TRUE(chip_controller->IsBubbleShowing());

  EXPECT_CALL(delegate, Accept(_))
      .WillOnce([&delegate](const PromptOptions& prompt_options) {
        delegate.PermissionRequestManager::Accept(prompt_options);
      });
  EXPECT_CALL(delegate, FinalizeCurrentRequests()).WillOnce([&delegate]() {
    delegate.PermissionRequestManager::FinalizeCurrentRequests();
  });

  EXPECT_TRUE(delegate.IsRequestInProgress());
  ClickOnAcceptPermissionRequestQuietChip(chip_controller);
  EXPECT_FALSE(delegate.IsRequestInProgress());
  EXPECT_EQ(infobar_observer.info_bar_added_, true);
  histogram_tester.ExpectBucketCount(
      "Permissions.QuietPrompt.Preignore.PageReloadInfoBar", true, 1);
  histogram_tester.ExpectBucketCount(
      "Permissions.QuietPrompt.Preignore.PageReloadInfoBar", false, 0);
}
