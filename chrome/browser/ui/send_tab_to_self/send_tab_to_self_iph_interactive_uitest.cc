// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string_view>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_context_menu_delegate.h"
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_iph_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/browser/ui/toasts/toast_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/browser_tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/test/vertical_tabs_browser_test_mixin.h"
#include "chrome/browser/ui/views/user_education/browser_user_education_service.h"
#include "chrome/browser/user_education/tutorial_identifiers.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/user_education/interactive_feature_promo_test.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/send_tab_to_self/fake_send_tab_to_self_model.h"
#include "components/send_tab_to_self/features.h"
#include "components/send_tab_to_self/stub_send_tab_to_self_sync_service.h"
#include "components/send_tab_to_self/target_device_info.h"
#include "components/sync_device_info/device_info.h"
#include "components/tabs/public/tab_interface.h"
#include "components/user_education/common/tutorial/tutorial_registry.h"
#include "components/user_education/common/tutorial/tutorial_service.h"
#include "components/user_education/views/help_bubble_view.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/test/ui_controls.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "url/gurl.h"

namespace send_tab_to_self {

namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTabId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTabId);
constexpr char kScreenshotBaselineCL[] = "8239773";
constexpr char kSecondTabName[] = "SecondTab";
constexpr char kTargetDeviceCacheGuid[] = "target_device_guid";
constexpr char kTargetDeviceName[] = "Pixel 9";
#if !BUILDFLAG(IS_MAC)
constexpr char kDeviceMenuItemName[] = "DeviceMenuItem";
constexpr char16_t kTargetDeviceName16[] = u"Pixel 9";
#endif

}  // namespace

// -----------------------------------------------------------------------------
// SendTabToSelfTutorialInteractiveUiTest
//
// Component-level tests for the Send Tab to Self User Education tutorial
// definition. Verifies active tab anchor resolution, string IDs, step
// lifecycle callbacks, and metric emissions in isolation using synthetic
// view elements.
// -----------------------------------------------------------------------------
class SendTabToSelfTutorialInteractiveUiTest : public InteractiveBrowserTest {
 public:
  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    MaybeRegisterChromeTutorials(*GetTutorialService()->tutorial_registry());
  }

  void TearDownOnMainThread() override {
    auto* const service = GetTutorialService();
    service->CancelTutorialIfRunning();
    service->tutorial_registry()->RemoveTutorialForTesting(
        kSendTabToSelfTutorialId);
    InteractiveBrowserTest::TearDownOnMainThread();
  }

  user_education::TutorialService* GetTutorialService() {
    return UserEducationServiceFactory::GetForBrowserContext(
               browser()->GetProfile())
        ->tutorial_service();
  }

  // Starts the Send Tab to Self tutorial with optional callbacks.
  auto StartTutorial(user_education::TutorialService::CompletedCallback
                         completed = base::DoNothing(),
                     user_education::TutorialService::AbortedCallback aborted =
                         base::DoNothing()) {
    return Do([this, completed = std::move(completed),
               aborted = std::move(aborted)]() mutable {
      GetTutorialService()->StartTutorial(
          kSendTabToSelfTutorialId,
          BrowserElements::From(browser())->GetContext(), std::move(completed),
          std::move(aborted));
    });
  }

  // Cancels any running tutorial and waits for the bubble to hide.
  auto CancelTutorial() {
    return Steps(
        Do([this]() { GetTutorialService()->CancelTutorialIfRunning(); }),
        WaitForHide(
            user_education::HelpBubbleView::kHelpBubbleElementIdForTesting));
  }

  // Verifies that the help bubble anchors to the specified tab index.
  auto CheckHelpBubbleAnchor(int tab_index) {
    return CheckView(
        user_education::HelpBubbleView::kHelpBubbleElementIdForTesting,
        [this, tab_index](user_education::HelpBubbleView* bubble) {
          auto* const browser_view =
              BrowserView::GetBrowserViewForBrowser(browser());
          tabs::TabInterface* const tab =
              browser()->tab_strip_model()->GetTabAtIndex(tab_index);
          return tab && bubble->GetAnchorView() ==
                            browser_view->tab_strip_view()->GetTabAnchorView(
                                tab->GetHandle());
        });
  }

  // Verifies that the help bubble anchors to the specified element identifier.
  auto CheckHelpBubbleAnchor(ui::ElementIdentifier id) {
    return CheckView(
        user_education::HelpBubbleView::kHelpBubbleElementIdForTesting,
        [this, id](user_education::HelpBubbleView* bubble) {
          auto* const browser_view =
              BrowserView::GetBrowserViewForBrowser(browser());
          auto* const view =
              views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
                  id,
                  views::ElementTrackerViews::GetContextForView(browser_view));
          return bubble->GetAnchorView() == view;
        });
  }

  // Verifies the help bubble body string resource.
  auto CheckHelpBubbleBodyText(int string_id) {
    return CheckViewProperty(
        user_education::HelpBubbleView::kBodyTextIdForTesting,
        &views::Label::GetText, l10n_util::GetStringUTF16(string_id));
  }

  // Verifies the help bubble title string resource.
  auto CheckHelpBubbleTitleText(int string_id) {
    return CheckViewProperty(
        user_education::HelpBubbleView::kTitleTextIdForTesting,
        &views::Label::GetText, l10n_util::GetStringUTF16(string_id));
  }

  // Injects a dummy view with the specified element identifier for testing.
  auto AddDummyElement(ui::ElementIdentifier id) {
    return Do([this, id]() {
      auto* const browser_view =
          BrowserView::GetBrowserViewForBrowser(browser());
      views::View* const view = browser_view->contents_web_view()->AddChildView(
          std::make_unique<views::View>());
      view->SetProperty(views::kElementIdentifierKey, id);
    });
  }

  // Generates the test sequence verifying tutorial anchoring to active tabs.
  auto TestAnchorsToActiveTabSequence() {
    DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTabElementId);

    return Steps(
        AddInstrumentedTab(kSecondTabElementId,
                           GURL(chrome::kChromeUINewTabURL)),
        Check([this]() {
          return browser()->tab_strip_model()->active_index() == 1;
        }),
        StartTutorial(),
        WaitForShow(
            user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
        CheckHelpBubbleAnchor(1), CancelTutorial(),
        SelectTab(kTabStripElementId, 0), Check([this]() {
          return browser()->tab_strip_model()->active_index() == 0;
        }),
        StartTutorial(),
        WaitForShow(
            user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
        CheckHelpBubbleAnchor(0), CancelTutorial());
  }

  base::HistogramTester histogram_tester_;
};

IN_PROC_BROWSER_TEST_F(SendTabToSelfTutorialInteractiveUiTest,
                       AnchorsToActiveTab) {
  RunTestSequence(TestAnchorsToActiveTabSequence());
}

IN_PROC_BROWSER_TEST_F(SendTabToSelfTutorialInteractiveUiTest, TutorialSteps) {
  UNCALLED_MOCK_CALLBACK(user_education::TutorialService::CompletedCallback,
                         completed);
  UNCALLED_MOCK_CALLBACK(user_education::TutorialService::AbortedCallback,
                         aborted);

  EXPECT_CALL(completed, Run).Times(1);

  RunTestSequence(
      // Step 1: Start tutorial and verify bubble on active tab.
      StartTutorial(completed.Get(), aborted.Get()),
      WaitForShow(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
      CheckHelpBubbleAnchor(0),
      CheckHelpBubbleBodyText(IDS_TUTORIAL_SEND_TAB_TO_SELF_STEP_1_BODY),

      // Step 2: Show the Send Tab to Self menu item view.
      AddDummyElement(kTabSendTabToSelfMenuItem),
      WaitForHide(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
      WaitForShow(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
      CheckHelpBubbleAnchor(kTabSendTabToSelfMenuItem),
      CheckHelpBubbleBodyText(IDS_TUTORIAL_SEND_TAB_TO_SELF_STEP_2_BODY),

      // Step 3: Show the ToastView.
      AddDummyElement(toasts::ToastView::kToastViewId),
      WaitForHide(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
      WaitForShow(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
      CheckHelpBubbleAnchor(toasts::ToastView::kToastViewId),
      CheckHelpBubbleTitleText(IDS_TUTORIAL_SEND_TAB_TO_SELF_SUCCESS_TITLE),
      CheckHelpBubbleBodyText(IDS_TUTORIAL_SEND_TAB_TO_SELF_SUCCESS_BODY),

      // Complete tutorial by clicking default button.
      PressButton(user_education::HelpBubbleView::kDefaultButtonIdForTesting),
      WaitForHide(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
      Check([this]() { return !GetTutorialService()->IsRunningTutorial(); }),
      Do([this]() {
        histogram_tester_.ExpectUniqueSample(
            "Tutorial.SendTabToSelf.Completion", 1, 1);
      }));
}

IN_PROC_BROWSER_TEST_F(SendTabToSelfTutorialInteractiveUiTest,
                       TutorialDismissed) {
  UNCALLED_MOCK_CALLBACK(user_education::TutorialService::CompletedCallback,
                         completed);
  UNCALLED_MOCK_CALLBACK(user_education::TutorialService::AbortedCallback,
                         aborted);

  EXPECT_CALL(aborted, Run).Times(1);

  RunTestSequence(
      // Step 1: Start tutorial and verify bubble on active tab.
      StartTutorial(completed.Get(), aborted.Get()),
      WaitForShow(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
      CheckHelpBubbleAnchor(0),

      // Dismiss the tutorial via the close button on the help bubble.
      PressButton(user_education::HelpBubbleView::kCloseButtonIdForTesting),
      WaitForHide(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
      Check([this]() { return !GetTutorialService()->IsRunningTutorial(); }),
      Do([this]() {
        histogram_tester_.ExpectUniqueSample(
            "Tutorial.SendTabToSelf.Completion", 0, 1);
      }));
}

// -----------------------------------------------------------------------------
// SendTabToSelfVerticalTabsInteractiveUiTest
//
// Verifies active tab anchoring for the Send Tab to Self tutorial when
// vertical tabs are enabled.
// -----------------------------------------------------------------------------
class SendTabToSelfVerticalTabsInteractiveUiTest
    : public VerticalTabsBrowserTestMixin<
          SendTabToSelfTutorialInteractiveUiTest> {};

IN_PROC_BROWSER_TEST_F(SendTabToSelfVerticalTabsInteractiveUiTest,
                       AnchorsToActiveTabView) {
  RunTestSequence(TestAnchorsToActiveTabSequence());
}

// -----------------------------------------------------------------------------
// SendTabToSelfIphInteractiveUiTest
//
// End-to-end integration tests for the Send Tab to Self IPH feature promo.
// Verifies promo triggering on navigation, interactive user journey through
// live tab context menus and device submenus to toast completion, and baseline
// screenshot capture.
// -----------------------------------------------------------------------------
class SendTabToSelfIphInteractiveUiTest : public InteractiveFeaturePromoTest {
 public:
  SendTabToSelfIphInteractiveUiTest()
      : InteractiveFeaturePromoTest(UseDefaultTrackerAllowingPromos(
            {feature_engagement::kIPHSendTabToSelfTutorialFeature})) {
    scoped_feature_list_.InitWithFeatures(
        {send_tab_to_self::kSendTabToSelfEnhancedDesktopUI,
         send_tab_to_self::kSendTabToSelfPostSendToast},
        {});
  }

  ~SendTabToSelfIphInteractiveUiTest() override = default;

  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    SendTabToSelfSyncServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindOnce([](content::BrowserContext* context)
                                    -> std::unique_ptr<KeyedService> {
          return std::make_unique<
              send_tab_to_self::StubSendTabToSelfSyncService>();
        }));
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_https_test_server().Start());
    InteractiveFeaturePromoTest::SetUpOnMainThread();

    FakeSendTabToSelfModel* model =
        static_cast<StubSendTabToSelfSyncService*>(
            SendTabToSelfSyncServiceFactory::GetForProfile(
                browser()->GetProfile()))
            ->GetFakeSendTabToSelfModel();
    TargetDeviceInfo target_device(kTargetDeviceName, kTargetDeviceCacheGuid,
                                   syncer::DeviceInfo::FormFactor::kPhone,
                                   syncer::DeviceInfo::OsType::kAndroid,
                                   base::Time::Now());
    model->SetTargetDeviceInfoSortedList({target_device});
    model->SetHasValidTargetDevice(true);
    model->SetIsReady(true);
  }

#if !BUILDFLAG(IS_MAC)
  static views::MenuItemView* FindMenuItemWithTitle(
      views::View* root,
      std::u16string_view title_substring) {
    if (auto* const menu_item = views::AsViewClass<views::MenuItemView>(root)) {
      if (menu_item->title().find(title_substring) != std::u16string::npos) {
        return menu_item;
      }
    }
    for (views::View* child : root->children()) {
      if (auto* const result = FindMenuItemWithTitle(child, title_substring)) {
        return result;
      }
    }
    return nullptr;
  }

  auto NameMenuItemWithTitle(std::string_view name,
                             std::u16string_view title_substring) {
    return NameView(name, base::BindLambdaForTesting([=]() -> views::View* {
                      for (views::Widget* widget :
                           views::test::WidgetTest::GetAllWidgets()) {
                        if (auto* const result = FindMenuItemWithTitle(
                                widget->GetRootView(), title_substring)) {
                          return result;
                        }
                      }
                      return nullptr;
                    }));
  }
#endif

  auto StopToastTimer() {
    return Do([this]() {
      ToastController::From(browser())->GetToastCloseTimerForTesting()->Stop();
    });
  }

  // Submenu items in Cocoa context menus on macOS (MenuControllerCocoa) do not
  // assign tags to submenu items (TYPE_SUBMENU), causing Kombucha's
  // SelectMenuItem to fail. Additionally, Cocoa menu hierarchies do not expose
  // views::MenuItemView widgets and run a modal tracking loop. This helper
  // triggers the send action directly on macOS and dismisses the open context
  // menu to exit modal tracking, while performing the live submenu UI
  // interaction on other platforms.
  auto SelectSendTabToSelfDeviceItem() {
#if BUILDFLAG(IS_MAC)
    return Do([this]() {
      content::WebContents* const web_contents =
          browser()->tab_strip_model()->GetActiveWebContents();
      SendTabToSelfContextMenuDelegate(web_contents, ShareEntryPoint::kTabMenu)
          .ExecuteCommand(IDC_CONTENT_CONTEXT_SEND_TAB_TO_SELF_DEVICE1, 0);

      static_cast<BrowserTabStripController*>(
          BrowserView::GetBrowserViewForBrowser(browser())
              ->horizontal_tab_strip_for_testing()
              ->controller())
          ->CloseContextMenuForTesting();
    });
#else
    return Steps(
        SelectMenuItem(kTabSendTabToSelfMenuItem),
        NameMenuItemWithTitle(kDeviceMenuItemName, kTargetDeviceName16),
        InAnyContext(SelectMenuItem(kDeviceMenuItemName)));
#endif
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(SendTabToSelfIphInteractiveUiTest,
                       PromoTriggersOnEligiblePage) {
  const GURL eligible_url =
      embedded_https_test_server().GetURL("example.com", "/title1.html");

  RunTestSequence(
      InstrumentTab(kTabId), NavigateWebContents(kTabId, eligible_url),
      WaitForPromo(feature_engagement::kIPHSendTabToSelfTutorialFeature));
}

IN_PROC_BROWSER_TEST_F(SendTabToSelfIphInteractiveUiTest,
                       TutorialFlowCompletesOnTabSend) {
  const GURL eligible_url =
      embedded_https_test_server().GetURL("example.com", "/title1.html");

  RunTestSequence(
      InstrumentTab(kTabId), NavigateWebContents(kTabId, eligible_url),
      WaitForPromo(feature_engagement::kIPHSendTabToSelfTutorialFeature),
      PressDefaultPromoButton(),
      // Step 1: Bubble is shown on the active tab.
      MoveMouseTo(kTabElementId), ClickMouse(ui_controls::RIGHT),
      // Step 2: Context menu open, bubble on Send Tab to Self menu item.
      WaitForShow(kTabSendTabToSelfMenuItem), SelectSendTabToSelfDeviceItem(),
      // Completion step: bubble anchored to the toast notification.
      WaitForShow(toasts::ToastView::kToastViewId),
      WaitForShow(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting));
}

IN_PROC_BROWSER_TEST_F(SendTabToSelfIphInteractiveUiTest,
                       TutorialFlowCompletesWhenNonFirstTabIsActive) {
  const GURL eligible_url =
      embedded_https_test_server().GetURL("example.com", "/title1.html");

  RunTestSequence(
      InstrumentTab(kTabId), AddInstrumentedTab(kSecondTabId, eligible_url),
      WaitForPromo(feature_engagement::kIPHSendTabToSelfTutorialFeature),
      PressDefaultPromoButton(),
      // Step 1: Bubble is shown on the active tab (second tab at index 1).
      NameDescendantViewByType<Tab>(kBrowserViewElementId, kSecondTabName, 1),
      MoveMouseTo(kSecondTabName), ClickMouse(ui_controls::RIGHT),
      // Step 2: Context menu open, bubble on Send Tab to Self menu item.
      WaitForShow(kTabSendTabToSelfMenuItem), SelectSendTabToSelfDeviceItem(),
      // Completion step: bubble anchored to the toast notification.
      WaitForShow(toasts::ToastView::kToastViewId),
      WaitForShow(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting));
}

IN_PROC_BROWSER_TEST_F(SendTabToSelfIphInteractiveUiTest,
                       CaptureTutorialScreenshots) {
  const GURL eligible_url =
      embedded_https_test_server().GetURL("example.com", "/title1.html");

  RunTestSequence(
      SetOnIncompatibleAction(
          OnIncompatibleAction::kIgnoreAndContinue,
          "Screenshots not supported in all testing environments."),
      InstrumentTab(kTabId), AddInstrumentedTab(kSecondTabId, eligible_url),
      // 1. Initial tutorial entry promo bubble anchored to the active tab
      // (second tab).
      WaitForPromo(feature_engagement::kIPHSendTabToSelfTutorialFeature),
      Screenshot(user_education::HelpBubbleView::kHelpBubbleElementIdForTesting,
                 /*screenshot_name=*/"SendTabToSelfTutorialPromoBubble",
                 /*baseline_cl=*/kScreenshotBaselineCL),
      // 2. Step 1: Bubble anchored to the active tab prompting to right-click.
      PressDefaultPromoButton(),
      WaitForShow(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
      Screenshot(user_education::HelpBubbleView::kHelpBubbleElementIdForTesting,
                 /*screenshot_name=*/"SendTabToSelfTutorialStep1ActiveTab",
                 /*baseline_cl=*/kScreenshotBaselineCL),
      // 3. Step 2: Bubble anchored to Send Tab to Self menu item in context
      // menu.
      NameDescendantViewByType<Tab>(kBrowserViewElementId, kSecondTabName, 1),
      MoveMouseTo(kSecondTabName), ClickMouse(ui_controls::RIGHT),
      WaitForShow(kTabSendTabToSelfMenuItem),
      InAnyContext(Screenshot(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting,
          /*screenshot_name=*/"SendTabToSelfTutorialStep2MenuItem",
          /*baseline_cl=*/kScreenshotBaselineCL)),
      // 4. Step 3 / Success step: Bubble anchored to the post-send toast
      // notification.
      SelectSendTabToSelfDeviceItem(),
      WaitForShow(toasts::ToastView::kToastViewId),
      WaitForShow(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
      InAnyContext(Screenshot(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting,
          /*screenshot_name=*/"SendTabToSelfTutorialStep3ToastSuccess",
          /*baseline_cl=*/kScreenshotBaselineCL)));
}

}  // namespace send_tab_to_self
