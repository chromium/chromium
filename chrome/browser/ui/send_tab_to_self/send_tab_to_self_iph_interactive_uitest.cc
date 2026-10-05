// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string_view>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_iph_controller.h"
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_sub_menu_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
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
#include "components/send_tab_to_self/send_tab_to_self_entry.h"
#include "components/send_tab_to_self/stub_send_tab_to_self_sync_service.h"
#include "components/send_tab_to_self/target_device_info.h"
#include "components/sync_device_info/device_info.h"
#include "components/tabs/public/tab_interface.h"
#include "components/user_education/common/tutorial/tutorial_registry.h"
#include "components/user_education/common/tutorial/tutorial_service.h"
#include "components/user_education/common/user_education_features.h"
#include "components/user_education/views/help_bubble_view.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/test/ui_controls.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

namespace send_tab_to_self {

namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTabId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTabId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPromoTabId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNtpTabId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondEligibleTabId);
constexpr char kScreenshotBaselineCL[] = "8239773";
constexpr char kSecondTabName[] = "SecondTab";
constexpr char kTargetDeviceCacheGuid[] = "target_device_guid";
constexpr char kTargetDeviceName[] = "Pixel 9";
#if !BUILDFLAG(IS_MAC)
constexpr char kDeviceMenuItemName[] = "DeviceMenuItem";
constexpr char16_t kTargetDeviceName16[] = u"Pixel 9";
#endif

// StubSendTabToSelfSyncService::GetEntryPointDisplayReason() ignores the URL
// and always returns its configured reason. The production service returns
// std::nullopt for non-HTTP(S) URLs. Mirror that here so the startup promo
// doesn't trigger on the initial about:blank tab before the test navigates.
class TestSendTabToSelfSyncService : public StubSendTabToSelfSyncService {
 public:
  std::optional<EntryPointDisplayReason> GetEntryPointDisplayReason(
      const GURL& url_to_share) override {
    if (!SendTabToSelfEntry::IsValidUrl(url_to_share)) {
      return std::nullopt;
    }
    return StubSendTabToSelfSyncService::GetEntryPointDisplayReason(
        url_to_share);
  }
};

}  // namespace

// -----------------------------------------------------------------------------
// SendTabToSelfTutorialInteractiveUiTest
//
// Component-level tests for the Send Tab to Self User Education tutorial
// definition. Verifies promo tab anchor resolution, string IDs, step
// lifecycle callbacks, and metric emissions in isolation using synthetic
// view elements.
// -----------------------------------------------------------------------------
class SendTabToSelfTutorialInteractiveUiTest : public InteractiveBrowserTest {
 public:
  SendTabToSelfTutorialInteractiveUiTest() {
    scoped_feature_list_.InitWithFeatures(
        {feature_engagement::kIPHSendTabToSelfTutorialFeature,
         send_tab_to_self::kSendTabToSelfEnhancedDesktopUI},
        {});
  }

  // Step 1 only anchors to an eligible promo tab. The real sync service never
  // offers Send Tab to Self on about:blank or the NTP, so use the stub, which
  // offers it on every tab. Unlike in SendTabToSelfIphInteractiveUiTest, the
  // startup promo can't trigger on about:blank here because browser tests
  // block all IPH by default.
  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    SendTabToSelfSyncServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindOnce([](content::BrowserContext* context)
                                    -> std::unique_ptr<KeyedService> {
          return std::make_unique<StubSendTabToSelfSyncService>();
        }));
  }

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

  // Records the tab at `tab_index` as the tab the intro promo was shown on, as
  // ChooseSendTabToSelfPromoAnchorTab() does when the promo anchors.
  auto SetPromoTab(int tab_index) {
    return Do([this, tab_index]() {
      SendTabToSelfIphController::From(browser())->SetPromoTab(
          browser()->tab_strip_model()->GetTabAtIndex(tab_index)->GetHandle());
    });
  }

  // Cancels any running tutorial and waits for the bubble to hide.
  auto CancelTutorial() {
    return Steps(
        Do([this]() { GetTutorialService()->CancelTutorialIfRunning(); }),
        WaitForHide(
            user_education::HelpBubbleView::kHelpBubbleElementIdForTesting));
  }

  // Verifies that the help bubble anchors to the specified element identifier.
  auto CheckHelpBubbleAnchoredToElement(ui::ElementIdentifier id) {
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

  // Verifies the help bubble arrow orientation.
  auto CheckHelpBubbleArrow(views::BubbleBorder::Arrow arrow) {
    return CheckView(
        user_education::HelpBubbleView::kHelpBubbleElementIdForTesting,
        [arrow](user_education::HelpBubbleView* bubble) {
          return bubble->arrow() == arrow;
        });
  }

  // Verifies that the help bubble anchors to the tab at `tab_index` in
  // `browser`.
  auto CheckHelpBubbleAnchoredToTab(BrowserWindowInterface* browser,
                                    int tab_index) {
    return CheckView(
        user_education::HelpBubbleView::kHelpBubbleElementIdForTesting,
        [browser, tab_index](user_education::HelpBubbleView* bubble) {
          auto* const browser_view =
              BrowserView::GetBrowserViewForBrowser(browser);
          tabs::TabInterface* const tab =
              browser->GetTabStripModel()->GetTabAtIndex(tab_index);
          return tab && bubble->GetAnchorView() ==
                            browser_view->tab_strip_view()->GetTabAnchorView(
                                tab->GetHandle());
        });
  }

  // Injects a dummy view with the specified element identifier for testing.
  auto AddDummyElement(ui::ElementIdentifier id) {
    return Do([this, id]() {
      auto* const browser_view =
          BrowserView::GetBrowserViewForBrowser(browser());
      views::View* const view = browser_view->GetContentsView()->AddChildView(
          std::make_unique<views::View>());
      view->SetProperty(views::kElementIdentifierKey, id);
    });
  }

  // Generates the test sequence verifying that Step 1 anchors to the promo tab
  // rather than the active tab.
  auto TestAnchorsToPromoTabSequence() {
    DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTabElementId);

    return Steps(
        AddInstrumentedTab(kSecondTabElementId,
                           GURL(chrome::kChromeUINewTabURL)),
        Check([this]() {
          return browser()->tab_strip_model()->active_index() == 1;
        }),
        SetPromoTab(1), StartTutorial(),
        WaitForShow(
            user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
        CheckHelpBubbleAnchoredToTab(browser(), 1), CancelTutorial(),
        // The promo tab is inactive; Step 1 still anchors to it.
        SetPromoTab(0), StartTutorial(),
        WaitForShow(
            user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
        CheckHelpBubbleAnchoredToTab(browser(), 0), CancelTutorial());
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  base::HistogramTester histogram_tester_;
};

IN_PROC_BROWSER_TEST_F(SendTabToSelfTutorialInteractiveUiTest,
                       AnchorsToPromoTab) {
  RunTestSequence(TestAnchorsToPromoTabSequence());
}

IN_PROC_BROWSER_TEST_F(SendTabToSelfTutorialInteractiveUiTest,
                       TutorialAbortsWithoutPromoTab) {
  UNCALLED_MOCK_CALLBACK(user_education::TutorialService::CompletedCallback,
                         completed);
  UNCALLED_MOCK_CALLBACK(user_education::TutorialService::AbortedCallback,
                         aborted);

  EXPECT_CALL_IN_SCOPE(
      aborted, Run,
      RunTestSequence(
          StartTutorial(completed.Get(), aborted.Get()),
          EnsureNotPresent(
              user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
          Check([this]() {
            return !GetTutorialService()->IsRunningTutorial();
          })));
}

IN_PROC_BROWSER_TEST_F(SendTabToSelfTutorialInteractiveUiTest, TutorialSteps) {
  UNCALLED_MOCK_CALLBACK(user_education::TutorialService::CompletedCallback,
                         completed);
  UNCALLED_MOCK_CALLBACK(user_education::TutorialService::AbortedCallback,
                         aborted);

  EXPECT_CALL_IN_SCOPE(
      completed, Run,
      RunTestSequence(
          // Step 1: Start tutorial and verify bubble on the promo tab.
          SetPromoTab(0), StartTutorial(completed.Get(), aborted.Get()),
          WaitForShow(
              user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
          CheckHelpBubbleAnchoredToTab(browser(), 0),
          CheckHelpBubbleBodyText(IDS_TUTORIAL_SEND_TAB_TO_SELF_STEP_1_BODY),

          // Step 2: Show the Send Tab to Self menu item view.
          AddDummyElement(kTabSendTabToSelfMenuItem),
          WaitForHide(
              user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
          WaitForShow(
              user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
          CheckHelpBubbleAnchoredToElement(kTabSendTabToSelfMenuItem),
          CheckHelpBubbleArrow(views::BubbleBorder::BOTTOM_LEFT),
          CheckHelpBubbleBodyText(IDS_TUTORIAL_SEND_TAB_TO_SELF_STEP_2_BODY),

          // Step 3: Show the ToastView.
          AddDummyElement(toasts::ToastView::kToastViewId),
          WaitForHide(
              user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
          WaitForShow(
              user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
          CheckHelpBubbleAnchoredToElement(toasts::ToastView::kToastViewId),
          CheckHelpBubbleTitleText(IDS_TUTORIAL_SEND_TAB_TO_SELF_SUCCESS_TITLE),
          CheckHelpBubbleBodyText(IDS_TUTORIAL_SEND_TAB_TO_SELF_SUCCESS_BODY),

          // Complete tutorial by clicking default button.
          PressButton(
              user_education::HelpBubbleView::kDefaultButtonIdForTesting),
          WaitForHide(
              user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
          Check(
              [this]() { return !GetTutorialService()->IsRunningTutorial(); }),
          Do([this]() {
            histogram_tester_.ExpectUniqueSample(
                "Tutorial.SendTabToSelf.Completion", 1, 1);
          })));
}

IN_PROC_BROWSER_TEST_F(SendTabToSelfTutorialInteractiveUiTest,
                       TutorialDismissed) {
  UNCALLED_MOCK_CALLBACK(user_education::TutorialService::CompletedCallback,
                         completed);
  UNCALLED_MOCK_CALLBACK(user_education::TutorialService::AbortedCallback,
                         aborted);

  EXPECT_CALL_IN_SCOPE(
      aborted, Run,
      RunTestSequence(
          // Step 1: Start tutorial and verify bubble on the promo tab.
          SetPromoTab(0), StartTutorial(completed.Get(), aborted.Get()),
          WaitForShow(
              user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
          CheckHelpBubbleAnchoredToTab(browser(), 0),

          // Dismiss the tutorial via the close button on the help bubble.
          PressButton(user_education::HelpBubbleView::kCloseButtonIdForTesting),
          WaitForHide(
              user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
          Check(
              [this]() { return !GetTutorialService()->IsRunningTutorial(); }),
          Do([this]() {
            histogram_tester_.ExpectUniqueSample(
                "Tutorial.SendTabToSelf.Completion", 0, 1);
          })));
}

IN_PROC_BROWSER_TEST_F(SendTabToSelfTutorialInteractiveUiTest,
                       TutorialAbortsIfPromoTabClosed) {
  UNCALLED_MOCK_CALLBACK(user_education::TutorialService::CompletedCallback,
                         completed);
  UNCALLED_MOCK_CALLBACK(user_education::TutorialService::AbortedCallback,
                         aborted);

  EXPECT_CALL_IN_SCOPE(
      aborted, Run,
      RunTestSequence(
          // Record tab 0 as the promo tab, add a second tab, and close tab 0.
          SetPromoTab(0), Do([this]() {
            chrome::AddSelectedTabWithURL(browser(), GURL("about:blank"),
                                          ui::PAGE_TRANSITION_LINK);
            browser()->tab_strip_model()->CloseWebContentsAt(
                0, TabCloseTypes::CLOSE_USER_GESTURE);
          }),
          // Starting tutorial fails because promo tab was closed.
          StartTutorial(completed.Get(), aborted.Get()),
          EnsureNotPresent(
              user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
          Check([this]() {
            return !GetTutorialService()->IsRunningTutorial();
          })));
}

// -----------------------------------------------------------------------------
// SendTabToSelfVerticalTabsInteractiveUiTest
//
// Verifies promo tab anchoring for the Send Tab to Self tutorial when
// vertical tabs are enabled.
// -----------------------------------------------------------------------------
class SendTabToSelfVerticalTabsInteractiveUiTest
    : public VerticalTabsBrowserTestMixin<
          SendTabToSelfTutorialInteractiveUiTest> {};

IN_PROC_BROWSER_TEST_F(SendTabToSelfVerticalTabsInteractiveUiTest,
                       AnchorsToPromoTabView) {
  RunTestSequence(TestAnchorsToPromoTabSequence());
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
         send_tab_to_self::kSendTabToSelfPostSendToast,
         user_education::features::kRaiseMacHelpBubbleAboveMenus},
        {});
  }

  ~SendTabToSelfIphInteractiveUiTest() override = default;

  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    SendTabToSelfSyncServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindOnce([](content::BrowserContext* context)
                                    -> std::unique_ptr<KeyedService> {
          return std::make_unique<TestSendTabToSelfSyncService>();
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

  // Verifies that the help bubble anchors to the tab at `tab_index` in
  // `browser`.
  auto CheckHelpBubbleAnchoredToTab(BrowserWindowInterface* browser,
                                    int tab_index) {
    return CheckView(
        user_education::HelpBubbleView::kHelpBubbleElementIdForTesting,
        [browser, tab_index](user_education::HelpBubbleView* bubble) {
          auto* const browser_view =
              BrowserView::GetBrowserViewForBrowser(browser);
          tabs::TabInterface* const tab =
              browser->GetTabStripModel()->GetTabAtIndex(tab_index);
          return tab && bubble->GetAnchorView() ==
                            browser_view->tab_strip_view()->GetTabAnchorView(
                                tab->GetHandle());
        });
  }

  auto CheckHelpBubbleBodyText(int string_id) {
    return CheckViewProperty(
        user_education::HelpBubbleView::kBodyTextIdForTesting,
        &views::Label::GetText, l10n_util::GetStringUTF16(string_id));
  }

  void CloseTabContextMenu() {
    static_cast<BrowserTabStripController*>(
        BrowserView::GetBrowserViewForBrowser(browser())
            ->horizontal_tab_strip_for_testing()
            ->controller())
        ->CloseContextMenuForTesting();
  }

#if BUILDFLAG(IS_MAC)
  // Cocoa context menus (MenuControllerCocoa) run a modal tracking loop, do not
  // assign tags to TYPE_SUBMENU items, and do not expose views::MenuItemView
  // widgets, preventing Kombucha's SelectMenuItem from clicking submenu items.
  // This helper triggers the send command directly on the active tab and
  // dismisses the open context menu to exit modal tracking.
  void SendTabAndCloseContextMenu() {
    SendTabToSelfSubMenuModel::MaybeCreateForTab(
        browser()->tab_strip_model()->GetActiveWebContents(),
        ShareEntryPoint::kTabMenu)
        ->ExecuteCommand(IDC_CONTENT_CONTEXT_SEND_TAB_TO_SELF_DEVICE1, 0);

    CloseTabContextMenu();
  }
#endif

  // Selects the target device from the Send Tab to Self submenu. On macOS, this
  // delegates to SendTabAndCloseContextMenu() due to Cocoa menu tracking
  // limitations; on other platforms, it drives the live submenu UI interaction.
  auto SelectSendTabToSelfDeviceItem() {
#if BUILDFLAG(IS_MAC)
    return Steps(
        // Wait for Step 2's bubble to anchor to the context menu item.
        InAnyContext(WaitForShow(
            user_education::HelpBubbleView::kHelpBubbleElementIdForTesting)),
        // Send the tab and dismiss the native menu without failing visibility
        // checks when the menu closes before the async toast appears.
        InAnyContext(WithElement(kTabSendTabToSelfMenuItem,
                                 [this](ui::TrackedElement*) {
                                   SendTabAndCloseContextMenu();
                                 })
                         .SetMustRemainVisible(false)),
        // Ensure Step 2's bubble dismisses before awaiting Step 3's toast
        // bubble.
        InAnyContext(WaitForHide(
            user_education::HelpBubbleView::kHelpBubbleElementIdForTesting)));
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

// TODO(crbug.com/565062574): Re-enable on mac
#if BUILDFLAG(IS_MAC)
#define MAYBE_TutorialFlowCompletesOnTabSend \
  DISABLED_TutorialFlowCompletesOnTabSend
#else
#define MAYBE_TutorialFlowCompletesOnTabSend \
  TutorialFlowCompletesOnTabSend
#endif
IN_PROC_BROWSER_TEST_F(SendTabToSelfIphInteractiveUiTest,
                       MAYBE_TutorialFlowCompletesOnTabSend) {
  const GURL eligible_url =
      embedded_https_test_server().GetURL("example.com", "/title1.html");

  RunTestSequence(
      InstrumentTab(kTabId), NavigateWebContents(kTabId, eligible_url),
      WaitForPromo(feature_engagement::kIPHSendTabToSelfTutorialFeature),
      PressDefaultPromoButton(),
      // Step 1: Bubble is shown on the active tab.
      InAnyContext(WaitForShow(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting)),
      CheckHelpBubbleAnchoredToTab(browser(), 0),
      CheckHelpBubbleBodyText(IDS_TUTORIAL_SEND_TAB_TO_SELF_STEP_1_BODY),
      MoveMouseTo(kTabElementId), ClickMouse(ui_controls::RIGHT),
      // Step 2: Context menu open, bubble on Send Tab to Self menu item.
      InAnyContext(WaitForShow(kTabSendTabToSelfMenuItem)),
      SelectSendTabToSelfDeviceItem(),
      // Completion step: bubble anchored to the toast notification.
      InAnyContext(WaitForShow(toasts::ToastView::kToastViewId)),
      InAnyContext(WaitForShow(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting)));
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
      WaitForShow(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
      CheckHelpBubbleAnchoredToTab(browser(), 1),
      CheckHelpBubbleBodyText(IDS_TUTORIAL_SEND_TAB_TO_SELF_STEP_1_BODY),
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

IN_PROC_BROWSER_TEST_F(SendTabToSelfIphInteractiveUiTest,
                       Step2HelpBubbleAnchoredToContextMenuItem) {
  const GURL eligible_url =
      embedded_https_test_server().GetURL("example.com", "/title1.html");
  gfx::Rect menu_item_bounds;
#if !BUILDFLAG(IS_MAC)
  gfx::Rect submenu_item_bounds;
#endif

  RunTestSequence(
      InstrumentTab(kTabId), AddInstrumentedTab(kSecondTabId, eligible_url),
      WaitForPromo(feature_engagement::kIPHSendTabToSelfTutorialFeature),
      PressDefaultPromoButton(),
      // Step 1: Wait for the bubble on the active tab.
      WaitForShow(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
      NameDescendantViewByType<Tab>(kBrowserViewElementId, kSecondTabName, 1),
      MoveMouseTo(kSecondTabName), ClickMouse(ui_controls::RIGHT),
      // Step 2: Context menu open, bubble anchored to Send Tab to Self item.
      WaitForShow(kTabSendTabToSelfMenuItem),
      WithElement(kTabSendTabToSelfMenuItem,
                  [&menu_item_bounds](ui::TrackedElement* el) {
                    menu_item_bounds = el->GetScreenBounds();
                  }),
      WaitForShow(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
      CheckView(user_education::HelpBubbleView::kHelpBubbleElementIdForTesting,
                [&menu_item_bounds](user_education::HelpBubbleView* bubble) {
                  const gfx::Rect bubble_bounds = bubble->GetBoundsInScreen();
                  if (!bubble->GetWidget()->IsVisible() ||
                      bubble_bounds.IsEmpty()) {
                    return false;
                  }
#if BUILDFLAG(IS_MAC)
                  if (base::mac::MacOSMajorVersion() < 14) {
                    return true;
                  }
#endif
                  // Verify the bubble is positioned above the "Send to your
                  // devices" menu item and does not cover the menu item.
                  return bubble_bounds.bottom() <= menu_item_bounds.y() &&
                         !bubble_bounds.Intersects(menu_item_bounds);
                }),
#if !BUILDFLAG(IS_MAC)
      // Open the device submenu and verify the Step 2 help bubble remains
      // visible and does not overlap the submenu item.
      SelectMenuItem(kTabSendTabToSelfMenuItem),
      NameMenuItemWithTitle(kDeviceMenuItemName, kTargetDeviceName16),
      InAnyContext(WithView(kDeviceMenuItemName,
                            [&submenu_item_bounds](views::View* item) {
                              submenu_item_bounds = item->GetBoundsInScreen();
                            })),
      CheckView(user_education::HelpBubbleView::kHelpBubbleElementIdForTesting,
                [&submenu_item_bounds](user_education::HelpBubbleView* bubble) {
                  return bubble->GetWidget()->IsVisible() &&
                         !bubble->GetBoundsInScreen().Intersects(
                             submenu_item_bounds);
                }),
      InAnyContext(SelectMenuItem(kDeviceMenuItemName)),
#else
      SelectSendTabToSelfDeviceItem(),
#endif
      // Step 3: Completion bubble anchored to the toast notification.
      WaitForShow(toasts::ToastView::kToastViewId),
      WaitForShow(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting));
}

IN_PROC_BROWSER_TEST_F(SendTabToSelfIphInteractiveUiTest,
                       TutorialStep1StaysOnPromoTabAfterSwitchingToNtp) {
  const GURL eligible_url =
      embedded_https_test_server().GetURL("example.com", "/title1.html");

  RunTestSequence(
      InstrumentTab(kTabId), AddInstrumentedTab(kPromoTabId, eligible_url),
      WaitForPromo(feature_engagement::kIPHSendTabToSelfTutorialFeature),
      CheckHelpBubbleAnchoredToTab(browser(), 1),
      AddInstrumentedTab(kNtpTabId, GURL(chrome::kChromeUINewTabURL)),
      EnsurePresent(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
      PressDefaultPromoButton(),
      WaitForShow(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
      // Verify Step 1 help bubble points at the promo tab (index 1), not the
      // newly activated NTP tab (index 2).
      CheckHelpBubbleAnchoredToTab(browser(), 1),
      CheckHelpBubbleBodyText(IDS_TUTORIAL_SEND_TAB_TO_SELF_STEP_1_BODY),
      // Right-clicking the promo tab (index 1) advances the tutorial to Step 2.
      NameDescendantViewByType<Tab>(kBrowserViewElementId, kSecondTabName, 1),
      MoveMouseTo(kSecondTabName), ClickMouse(ui_controls::RIGHT),
      InAnyContext(WaitForShow(kTabSendTabToSelfMenuItem)),
      InAnyContext(WaitForShow(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting)),
      InAnyContext(
          CheckHelpBubbleBodyText(IDS_TUTORIAL_SEND_TAB_TO_SELF_STEP_2_BODY)),
      InAnyContext(
          WithElement(kTabSendTabToSelfMenuItem, [this](ui::TrackedElement*) {
            CloseTabContextMenu();
          }).SetMustRemainVisible(false)));
}

IN_PROC_BROWSER_TEST_F(
    SendTabToSelfIphInteractiveUiTest,
    TutorialStep1StaysOnPromoTabAfterSwitchingToAnotherEligibleTab) {
  const GURL eligible_url_a =
      embedded_https_test_server().GetURL("example.com", "/title1.html");
  const GURL eligible_url_b =
      embedded_https_test_server().GetURL("example.com", "/title2.html");

  RunTestSequence(
      InstrumentTab(kTabId), AddInstrumentedTab(kPromoTabId, eligible_url_a),
      WaitForPromo(feature_engagement::kIPHSendTabToSelfTutorialFeature),
      CheckHelpBubbleAnchoredToTab(browser(), 1),
      AddInstrumentedTab(kSecondEligibleTabId, eligible_url_b),
      EnsurePresent(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
      PressDefaultPromoButton(),
      WaitForShow(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
      // Step 1 bubble points at tab A (index 1), not tab B (index 2).
      CheckHelpBubbleAnchoredToTab(browser(), 1),
      CheckHelpBubbleBodyText(IDS_TUTORIAL_SEND_TAB_TO_SELF_STEP_1_BODY));
}

IN_PROC_BROWSER_TEST_F(SendTabToSelfIphInteractiveUiTest,
                       TutorialAbortsIfPromoTabBecomesIneligible) {
  const GURL eligible_url =
      embedded_https_test_server().GetURL("example.com", "/title1.html");

  RunTestSequence(
      InstrumentTab(kTabId), AddInstrumentedTab(kPromoTabId, eligible_url),
      WaitForPromo(feature_engagement::kIPHSendTabToSelfTutorialFeature),
      CheckHelpBubbleAnchoredToTab(browser(), 1),
      // Navigating the promo tab to a page without the Send Tab to Self entry
      // point leaves the promo showing.
      NavigateWebContents(kPromoTabId, GURL(chrome::kChromeUIVersionURL)),
      EnsurePresent(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
      PressDefaultPromoButton(),
      // The tutorial aborts instead of showing Step 1 on the ineligible tab.
      EnsureNotPresent(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
      Check([this]() {
        return !UserEducationServiceFactory::GetForBrowserContext(
                    browser()->GetProfile())
                    ->tutorial_service()
                    ->IsRunningTutorial();
      }));
}

IN_PROC_BROWSER_TEST_F(
    SendTabToSelfIphInteractiveUiTest,
    TutorialPromoAnchorsToActiveTabWhenFirstTabIsAlsoEligible) {
  const GURL eligible_url_a =
      embedded_https_test_server().GetURL("example.com", "/title1.html");
  const GURL eligible_url_b =
      embedded_https_test_server().GetURL("example.com", "/title2.html");

  RunTestSequence(
      InstrumentTab(kTabId),
      // Add a second tab while both are still at about:blank (ineligible),
      // ensuring the promo doesn't trigger prematurely on tab 0.
      AddInstrumentedTab(kSecondEligibleTabId, GURL("about:blank")),
      NavigateWebContents(kTabId, eligible_url_a),
      NavigateWebContents(kSecondEligibleTabId, eligible_url_b),
      WaitForPromo(feature_engagement::kIPHSendTabToSelfTutorialFeature),
      // Verify the intro promo bubble anchors to the second tab (active tab).
      CheckHelpBubbleAnchoredToTab(browser(), 1), PressDefaultPromoButton(),
      WaitForShow(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
      // Verify Step 1 bubble also points to the second tab.
      CheckHelpBubbleAnchoredToTab(browser(), 1),
      CheckHelpBubbleBodyText(IDS_TUTORIAL_SEND_TAB_TO_SELF_STEP_1_BODY));
}

IN_PROC_BROWSER_TEST_F(SendTabToSelfIphInteractiveUiTest,
                       TutorialPromoAnchorsToEligibleActiveTab) {
  const GURL eligible_url =
      embedded_https_test_server().GetURL("example.com", "/title1.html");

  RunTestSequence(
      InstrumentTab(kTabId),
      // Add a second tab while both are still at about:blank (ineligible),
      // ensuring the promo doesn't trigger prematurely on tab 0.
      AddInstrumentedTab(kSecondEligibleTabId, GURL("about:blank")),
      // Navigate the active tab 1 to an eligible URL.
      NavigateWebContents(kSecondEligibleTabId, eligible_url),
      // Verify the promo shows anchored to active tab 1, not tab 0.
      WaitForPromo(feature_engagement::kIPHSendTabToSelfTutorialFeature),
      CheckHelpBubbleAnchoredToTab(browser(), 1));
}

// -----------------------------------------------------------------------------
// SendTabToSelfVerticalTabsIphInteractiveUiTest
//
// Verifies Send Tab to Self IPH promo and tutorial anchoring when vertical
// tabs are enabled.
// -----------------------------------------------------------------------------
class SendTabToSelfVerticalTabsIphInteractiveUiTest
    : public VerticalTabsBrowserTestMixin<SendTabToSelfIphInteractiveUiTest> {
 public:
  // Bypass VerticalTabsBrowserTestMixin::SetUpCommandLine so its
  // ScopedFeatureList is not initialized after
  // InteractiveFeaturePromoTestMixin::SetUp() and torn down out of LIFO order.
  void SetUpCommandLine(base::CommandLine* command_line) override {
    SendTabToSelfIphInteractiveUiTest::SetUpCommandLine(command_line);
  }
};

IN_PROC_BROWSER_TEST_F(
    SendTabToSelfVerticalTabsIphInteractiveUiTest,
    TutorialPromoAnchorsToActiveTabWhenFirstTabIsAlsoEligible_VerticalTabs) {
  const GURL eligible_url_a =
      embedded_https_test_server().GetURL("example.com", "/title1.html");
  const GURL eligible_url_b =
      embedded_https_test_server().GetURL("example.com", "/title2.html");

  RunTestSequence(
      InstrumentTab(kTabId),
      // Add a second tab while both are still at about:blank (ineligible),
      // ensuring the promo doesn't trigger prematurely on tab 0.
      AddInstrumentedTab(kSecondEligibleTabId, GURL("about:blank")),
      NavigateWebContents(kTabId, eligible_url_a),
      NavigateWebContents(kSecondEligibleTabId, eligible_url_b),
      WaitForPromo(feature_engagement::kIPHSendTabToSelfTutorialFeature),
      // Verify the intro promo bubble anchors to the second tab (active tab).
      CheckHelpBubbleAnchoredToTab(browser(), 1), PressDefaultPromoButton(),
      WaitForShow(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
      // Verify Step 1 bubble also points to the second tab on vertical tabs.
      CheckHelpBubbleAnchoredToTab(browser(), 1),
      CheckHelpBubbleBodyText(IDS_TUTORIAL_SEND_TAB_TO_SELF_STEP_1_BODY));
}

IN_PROC_BROWSER_TEST_F(
    SendTabToSelfVerticalTabsIphInteractiveUiTest,
    TutorialStep1StaysOnPromoTabAfterSwitchingToNtp_VerticalTabs) {
  const GURL eligible_url =
      embedded_https_test_server().GetURL("example.com", "/title1.html");

  RunTestSequence(
      InstrumentTab(kTabId), AddInstrumentedTab(kPromoTabId, eligible_url),
      WaitForPromo(feature_engagement::kIPHSendTabToSelfTutorialFeature),
      CheckHelpBubbleAnchoredToTab(browser(), 1),
      AddInstrumentedTab(kNtpTabId, GURL(chrome::kChromeUINewTabURL)),
      EnsurePresent(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
      PressDefaultPromoButton(),
      WaitForShow(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
      // Step 1 bubble points at the promo tab (index 1), not the NTP (index 2).
      CheckHelpBubbleAnchoredToTab(browser(), 1),
      CheckHelpBubbleBodyText(IDS_TUTORIAL_SEND_TAB_TO_SELF_STEP_1_BODY));
}

}  // namespace send_tab_to_self
