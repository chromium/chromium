// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/desktop_to_mobile_promos/ios_promos_utils.h"

#include <memory>
#include <string>
#include <utility>

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/desktop_to_mobile_promos/promos_pref_names.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/segmentation_platform/segmentation_platform_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/browser/ui/views/promos/ios_promo_bubble.h"
#include "chrome/browser/ui/views/toolbar/webui_test_utils.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/desktop_to_mobile_promos/promos_types.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/test/mock_tracker.h"
#include "components/feature_engagement/test/scoped_iph_feature_list.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/prefs/pref_service.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/input_context.h"
#include "components/segmentation_platform/public/prediction_options.h"
#include "components/segmentation_platform/public/result.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"
#include "components/segmentation_platform/public/testing/mock_segmentation_platform_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/test/fake_server_network_resources.h"
#include "components/sync/test/test_sync_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"

class IOSPromosUtilsTest : public SyncTest {
 public:
  IOSPromosUtilsTest() : SyncTest(SINGLE_CLIENT) {
    scoped_iph_feature_list_.InitAndEnableFeatures(
        {feature_engagement::kIPHiOSPasswordPromoDesktopFeature,
         feature_engagement::kIPHiOSPaymentPromoDesktopFeature,
         feature_engagement::kIPHiOSAddressPromoDesktopFeature});
  }

  IOSPromosUtilsTest(const IOSPromosUtilsTest&) = delete;

  IOSPromosUtilsTest& operator=(const IOSPromosUtilsTest&) = delete;

  void SetupSyncForAccount() {
    ASSERT_TRUE(SetupClients());

    // Sign the profile in.
    ASSERT_TRUE(GetClient(0)->SignInNoWaitForCompletion());

    CoreAccountInfo current_info =
        IdentityManagerFactory::GetForProfile(GetProfile(0))
            ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
    // Need to update hosted domain since it is not populated.
    AccountInfo account_info = AccountInfo::Builder(current_info)
                                   .SetHostedDomain(std::string())
                                   .Build();
    signin::UpdateAccountInfoForAccount(
        IdentityManagerFactory::GetForProfile(GetProfile(0)), account_info);

    // TODO(crbug.com/417921582): Use SetupClientsAndSignIn() once it exists.
    ASSERT_TRUE(SetupSync());
  }

  feature_engagement::test::ScopedIphFeatureList scoped_iph_feature_list_;
};

IN_PROC_BROWSER_TEST_F(IOSPromosUtilsTest, InvokeUi_passwords) {
  SetupSyncForAccount();

  ios_promos_utils::VerifyIOSPromoEligibility(
      desktop_to_mobile_promos::PromoType::kPassword, GetBrowser(0));
}

IN_PROC_BROWSER_TEST_F(IOSPromosUtilsTest, InvokeUi_addresses) {
  SetupSyncForAccount();

  ios_promos_utils::VerifyIOSPromoEligibility(
      desktop_to_mobile_promos::PromoType::kAddress, browser());
}

IN_PROC_BROWSER_TEST_F(IOSPromosUtilsTest, InvokeUi_payments) {
  SetupSyncForAccount();

  ios_promos_utils::VerifyIOSPromoEligibility(
      desktop_to_mobile_promos::PromoType::kPayment, browser());
}

class IOSPromosUtilsWebUIAvatarOverflowTest : public InProcessBrowserTest {
 public:
  IOSPromosUtilsWebUIAvatarOverflowTest() {
    feature_list_.InitWithFeatures(
        {features::kInitialWebUI, features::kWebUIToolbar,
         features::kSkipIPCChannelPausingForNonGuests,
         features::kWebUIInProcessResourceLoadingV2},
        {});
    scoped_iph_feature_list_.InitAndEnableFeatures(
        {feature_engagement::kIPHiOSTabGroupsDesktopFeature,
         feature_engagement::kIPHiOSPriceTrackingDesktopFeature});
  }

  void SetUpInProcessBrowserTestFixture() override {
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating(&IOSPromosUtilsWebUIAvatarOverflowTest::
                                        OnWillCreateBrowserContextServices,
                                    base::Unretained(this)));
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    // Prevent concurrent browser_tests windows sharing Xvfb from stealing X11
    // activation and dismissing the promo bubble while test RunLoops (such as
    // RunUntil or EvalJs) are spinning.
    promo_bubble_shown_subscription_ =
        ui::ElementTracker::GetElementTracker()
            ->AddElementShownInAnyContextCallback(
                kIOSPromoBubbleElementId,
                base::BindRepeating([](ui::TrackedElement* element) {
                  if (!element) {
                    return;
                  }
                  auto* tracked_view =
                      element->AsA<views::TrackedElementViews>();
                  if (!tracked_view || !tracked_view->view()->GetWidget()) {
                    return;
                  }
                  if (auto* bubble_delegate = tracked_view->view()
                                                  ->GetWidget()
                                                  ->widget_delegate()
                                                  ->AsBubbleDialogDelegate()) {
                    bubble_delegate->set_close_on_deactivate(false);
                  }
                }));
  }

  void TearDownOnMainThread() override {
    promo_bubble_shown_subscription_ = {};
    InProcessBrowserTest::TearDownOnMainThread();
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    SyncServiceFactory::GetInstance()->SetTestingFactory(
        context,
        base::BindRepeating(
            [](content::BrowserContext*) -> std::unique_ptr<KeyedService> {
              return std::make_unique<syncer::TestSyncService>();
            }));
    segmentation_platform::SegmentationPlatformServiceFactory::GetInstance()
        ->SetTestingFactory(
            context, base::BindRepeating([](content::BrowserContext*)
                                             -> std::unique_ptr<KeyedService> {
              auto service = std::make_unique<testing::NiceMock<
                  segmentation_platform::MockSegmentationPlatformService>>();
              ON_CALL(*service, GetClassificationResult(
                                    segmentation_platform::kDeviceSwitcherKey,
                                    testing::_, testing::_, testing::_))
                  .WillByDefault(
                      [](const std::string&,
                         const segmentation_platform::PredictionOptions&,
                         scoped_refptr<segmentation_platform::InputContext>,
                         segmentation_platform::ClassificationResultCallback
                             callback) {
                        std::move(callback).Run(
                            segmentation_platform::ClassificationResult(
                                segmentation_platform::PredictionStatus::
                                    kSucceeded));
                      });
              return service;
            }));
    feature_engagement::TrackerFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating([](content::BrowserContext*)
                                         -> std::unique_ptr<KeyedService> {
          auto tracker = std::make_unique<
              testing::NiceMock<feature_engagement::test::MockTracker>>();
          ON_CALL(*tracker, IsInitialized).WillByDefault(testing::Return(true));
          ON_CALL(*tracker, ShouldTriggerHelpUI)
              .WillByDefault(testing::Return(true));
          return tracker;
        }));
  }

  bool WaitForAvatarElementVisible(BrowserWindowInterface* target_browser,
                                   bool expected_visible) {
    return base::test::RunUntil([&]() {
      return (BrowserElements::From(target_browser)
                  ->GetElement(kToolbarAvatarButtonElementId) != nullptr) ==
             expected_visible;
    });
  }

  void WaitForWebUIToolbarReady(BrowserWindowInterface* target_browser) {
    WaitForInitialWebUIToolbar(target_browser);

    // Move focus (or stored focus if the widget is inactive on X11) to the
    // active tab's WebContents so any pending startup focus request for the
    // location bar (`FocusRequestTarget::kLocationBar`) is superseded.
    content::WebContents* tab_contents =
        target_browser->GetTabStripModel()->GetActiveWebContents();
    ASSERT_TRUE(tab_contents);
    tab_contents->Focus();

    WebUIToolbarWebView* webui_toolbar = GetWebUIToolbarWebView(target_browser);
    ASSERT_TRUE(webui_toolbar);
    content::WebContents* toolbar_web_contents =
        webui_toolbar->GetWebViewForTesting()->GetWebContents();
    ASSERT_TRUE(toolbar_web_contents);

    // Wait for `lazy_load.js` to finish upgrading toolbar controls (such as
    // `<avatar-button>` and `<readonly-omnibox>`) before flushing Mojo state
    // and clearing the omnibox's startup `"about:blank"` range selection.
    // Leaving a range selection active in `<readonly-omnibox>` while lazy
    // upgrades or state updates dirty Shadow DOM slot assignments hits
    // `DCHECK(IsFlatTreeClean(node))` in `LayoutSelection` when the promo
    // bubble activates (b/586727469).
    ASSERT_TRUE(content::ExecJs(toolbar_web_contents, R"((async () => {
      await Promise.all([
        customElements.whenDefined('avatar-button'),
        customElements.whenDefined('location-bar'),
        customElements.whenDefined('readonly-omnibox'),
      ]);
      const app = document.querySelector('toolbar-app');
      await app.updateComplete;
    })())"));

    ASSERT_TRUE(FlushWebUIToolbarPipes(target_browser));

    ASSERT_TRUE(content::ExecJs(toolbar_web_contents, R"((async () => {
      const app = document.querySelector('toolbar-app');
      await app.updateComplete;
      const omnibox = app.shadowRoot?.querySelector('location-bar')
          ?.shadowRoot?.querySelector('readonly-omnibox');
      if (omnibox) {
        await omnibox.updateComplete;
        const input = omnibox.shadowRoot?.querySelector('#textInput')
            ?.inputElement;
        if (input) {
          input.setSelectionRange(0, 0);
          input.blur();
        }
      }
      let activeEl = document.activeElement;
      while (activeEl?.shadowRoot?.activeElement) {
        activeEl = activeEl.shadowRoot.activeElement;
      }
      activeEl?.blur();
      document.getSelection()?.removeAllRanges();
    })())"));
    ASSERT_TRUE(FinishWebUIAnimations(toolbar_web_contents));
#if !BUILDFLAG(IS_CHROMEOS)
    ASSERT_TRUE(WaitForAvatarElementVisible(target_browser, true));
#endif
  }

#if !BUILDFLAG(IS_CHROMEOS)
  void OverflowWebUIToolbar(BrowserWindowInterface* target_browser) {
    ASSERT_NO_FATAL_FAILURE(WaitForWebUIToolbarReady(target_browser));
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(target_browser);
    WebUIToolbarWebView* webui_toolbar = GetWebUIToolbarWebView(target_browser);
    ASSERT_TRUE(webui_toolbar);
    const int spacer_width = browser_view->GetBounds().width();
    ASSERT_EQ(true, content::EvalJs(
                        webui_toolbar->GetWebViewForTesting()->GetWebContents(),
                        content::JsReplace(
                            R"((() => {
            const app = document.querySelector('toolbar-app');
            let spacer = app.shadowRoot.querySelector('#test-spacer');
            if (!spacer) {
              spacer = document.createElement('div');
              spacer.id = 'test-spacer';
              spacer.style.flexShrink = '0';
              app.shadowRoot.appendChild(spacer);
            }
            spacer.style.width = $1 + 'px';
            app.layoutResponsiveControls();
            return true;
          })();)",
                            spacer_width)));
    ASSERT_TRUE(WaitForAvatarElementVisible(target_browser, false));
  }

  bool IsAvatarHighlighted(BrowserWindowInterface* target_browser) {
    WebUIToolbarWebView* webui_toolbar = GetWebUIToolbarWebView(target_browser);
    return content::EvalJs(
               webui_toolbar->GetWebViewForTesting()->GetWebContents(),
               R"((() => {
            const app = document.querySelector('toolbar-app');
            const avatar = app.shadowRoot.querySelector('#avatar');
            return avatar.classList.contains('anchor-highlight') ||
                   Boolean(avatar.highlightTracker &&
                           avatar.highlightTracker.highlighted);
          })();)")
        .ExtractBool();
  }

  void VerifyPromoWhenAvatarOverflowed(
      desktop_to_mobile_promos::PromoType promo_type) {
    BrowserWindowInterface* target_browser = browser();
    ASSERT_NO_FATAL_FAILURE(OverflowWebUIToolbar(target_browser));

    ios_promos_utils::VerifyIOSPromoEligibility(promo_type, target_browser);
    ASSERT_TRUE(base::test::RunUntil(
        [&]() { return IOSPromoBubble::IsPromoTypeVisible(promo_type); }));

    ui::TrackedElement* avatar_element =
        BrowserElements::From(target_browser)
            ->GetElement(kToolbarAvatarButtonElementId);
    ASSERT_TRUE(avatar_element);

    views::View* bubble_view = BrowserElementsViews::From(target_browser)
                                   ->GetView(kIOSPromoBubbleElementId);
    ASSERT_TRUE(bubble_view);
    views::BubbleDialogDelegate* bubble_delegate =
        bubble_view->GetWidget()->widget_delegate()->AsBubbleDialogDelegate();
    ASSERT_TRUE(bubble_delegate);
    EXPECT_EQ(bubble_delegate->GetAnchorRect(),
              avatar_element->GetScreenBounds());
    EXPECT_FALSE(IsAvatarHighlighted(target_browser));

    views::test::WidgetDestroyedWaiter destroyed_waiter(
        bubble_view->GetWidget());
    IOSPromoBubble::Hide();
    destroyed_waiter.Wait();

    EXPECT_FALSE(IOSPromoBubble::IsPromoTypeVisible(promo_type));
    EXPECT_TRUE(WaitForAvatarElementVisible(target_browser, false));
  }
#endif  // !BUILDFLAG(IS_CHROMEOS)

 private:
  base::test::ScopedFeatureList feature_list_;
  feature_engagement::test::ScopedIphFeatureList scoped_iph_feature_list_;
  base::CallbackListSubscription create_services_subscription_;
  ui::ElementTracker::Subscription promo_bubble_shown_subscription_;
};

#if BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(IOSPromosUtilsWebUIAvatarOverflowTest,
                       TabGroupsPromoOnChromeOSAnchorsToToolbarFallback) {
  BrowserWindowInterface* target_browser = browser();
  ASSERT_NO_FATAL_FAILURE(WaitForWebUIToolbarReady(target_browser));
  EXPECT_FALSE(BrowserElements::From(target_browser)
                   ->GetElement(kToolbarAvatarButtonElementId));

  ios_promos_utils::VerifyIOSPromoEligibility(
      desktop_to_mobile_promos::PromoType::kTabGroups, target_browser);
  EXPECT_TRUE(IOSPromoBubble::IsPromoTypeVisible(
      desktop_to_mobile_promos::PromoType::kTabGroups));

  views::View* bubble_view = BrowserElementsViews::From(target_browser)
                                 ->GetView(kIOSPromoBubbleElementId);
  ASSERT_TRUE(bubble_view);
  views::BubbleDialogDelegate* bubble_delegate =
      bubble_view->GetWidget()->widget_delegate()->AsBubbleDialogDelegate();
  ASSERT_TRUE(bubble_delegate);
  WebUIToolbarWebView* webui_toolbar = GetWebUIToolbarWebView(target_browser);
  ASSERT_TRUE(webui_toolbar);
  EXPECT_EQ(bubble_delegate->GetAnchorView(), webui_toolbar->parent());

  views::test::WidgetDestroyedWaiter destroyed_waiter(bubble_view->GetWidget());
  IOSPromoBubble::Hide();
  destroyed_waiter.Wait();
  EXPECT_FALSE(IOSPromoBubble::IsPromoTypeVisible(
      desktop_to_mobile_promos::PromoType::kTabGroups));
}
#else
IN_PROC_BROWSER_TEST_F(IOSPromosUtilsWebUIAvatarOverflowTest,
                       TabGroupsPromoWhenAvatarVisible) {
  BrowserWindowInterface* target_browser = browser();
  ASSERT_NO_FATAL_FAILURE(WaitForWebUIToolbarReady(target_browser));

  ios_promos_utils::VerifyIOSPromoEligibility(
      desktop_to_mobile_promos::PromoType::kTabGroups, target_browser);
  EXPECT_TRUE(IOSPromoBubble::IsPromoTypeVisible(
      desktop_to_mobile_promos::PromoType::kTabGroups));

  ui::TrackedElement* avatar_element =
      BrowserElements::From(target_browser)
          ->GetElement(kToolbarAvatarButtonElementId);
  ASSERT_TRUE(avatar_element);
  views::View* bubble_view = BrowserElementsViews::From(target_browser)
                                 ->GetView(kIOSPromoBubbleElementId);
  ASSERT_TRUE(bubble_view);
  views::BubbleDialogDelegate* bubble_delegate =
      bubble_view->GetWidget()->widget_delegate()->AsBubbleDialogDelegate();
  ASSERT_TRUE(bubble_delegate);
  EXPECT_EQ(bubble_delegate->GetAnchorRect(),
            avatar_element->GetScreenBounds());
  EXPECT_FALSE(IsAvatarHighlighted(target_browser));

  views::test::WidgetDestroyedWaiter destroyed_waiter(bubble_view->GetWidget());
  IOSPromoBubble::Hide();
  destroyed_waiter.Wait();
  EXPECT_FALSE(IOSPromoBubble::IsPromoTypeVisible(
      desktop_to_mobile_promos::PromoType::kTabGroups));
}

IN_PROC_BROWSER_TEST_F(
    IOSPromosUtilsWebUIAvatarOverflowTest,
    TabGroupsPromoWhenAvatarOverflowedPopsOutAndRestoresOnClose) {
  VerifyPromoWhenAvatarOverflowed(
      desktop_to_mobile_promos::PromoType::kTabGroups);
}

IN_PROC_BROWSER_TEST_F(
    IOSPromosUtilsWebUIAvatarOverflowTest,
    PriceTrackingPromoWhenAvatarOverflowedPopsOutAndRestoresOnClose) {
  VerifyPromoWhenAvatarOverflowed(
      desktop_to_mobile_promos::PromoType::kPriceTracking);
}

IN_PROC_BROWSER_TEST_F(
    IOSPromosUtilsWebUIAvatarOverflowTest,
    SecondPromoWhileShowingReleasesExtraOverflowLockOnEarlyReturn) {
  BrowserWindowInterface* target_browser = browser();
  ASSERT_NO_FATAL_FAILURE(OverflowWebUIToolbar(target_browser));

  ios_promos_utils::VerifyIOSPromoEligibility(
      desktop_to_mobile_promos::PromoType::kTabGroups, target_browser);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return IOSPromoBubble::IsPromoTypeVisible(
        desktop_to_mobile_promos::PromoType::kTabGroups);
  }));

  // Clear the cooldown timestamp so a second promo passes eligibility while the
  // first promo bubble is still open. ShowPromoBubble() returns early and
  // immediately releases the second visibility lock.
  target_browser->GetProfile()->GetPrefs()->ClearPref(
      promos_prefs::kDesktopToiOSTabGroupsPromoLastImpressionTimestamp);
  ios_promos_utils::VerifyIOSPromoEligibility(
      desktop_to_mobile_promos::PromoType::kPriceTracking, target_browser);
  EXPECT_TRUE(IOSPromoBubble::IsPromoTypeVisible(
      desktop_to_mobile_promos::PromoType::kTabGroups));
  EXPECT_FALSE(IOSPromoBubble::IsPromoTypeVisible(
      desktop_to_mobile_promos::PromoType::kPriceTracking));

  views::View* bubble_view = BrowserElementsViews::From(target_browser)
                                 ->GetView(kIOSPromoBubbleElementId);
  ASSERT_TRUE(bubble_view);
  views::test::WidgetDestroyedWaiter destroyed_waiter(bubble_view->GetWidget());
  IOSPromoBubble::Hide();
  destroyed_waiter.Wait();

  EXPECT_FALSE(IOSPromoBubble::IsPromoTypeVisible(
      desktop_to_mobile_promos::PromoType::kTabGroups));
  EXPECT_TRUE(WaitForAvatarElementVisible(target_browser, false));
}

IN_PROC_BROWSER_TEST_F(
    IOSPromosUtilsWebUIAvatarOverflowTest,
    CloseBrowserWhileAvatarVisibilityPendingAndWhileShowing) {
  BrowserWindowInterface* pending_browser =
      CreateBrowser(browser()->GetProfile());
  ASSERT_NO_FATAL_FAILURE(OverflowWebUIToolbar(pending_browser));

  ios_promos_utils::VerifyIOSPromoEligibility(
      desktop_to_mobile_promos::PromoType::kTabGroups, pending_browser);
  EXPECT_EQ(1,
            pending_browser->GetProfile()->GetPrefs()->GetInteger(
                promos_prefs::kDesktopToiOSTabGroupsPromoImpressionsCounter));
  EXPECT_FALSE(IOSPromoBubble::IsPromoTypeVisible(
      desktop_to_mobile_promos::PromoType::kTabGroups));

  CloseBrowserSynchronously(pending_browser);
  EXPECT_FALSE(IOSPromoBubble::IsPromoTypeVisible(
      desktop_to_mobile_promos::PromoType::kTabGroups));

  BrowserWindowInterface* showing_browser = browser();
  showing_browser->GetProfile()->GetPrefs()->ClearPref(
      promos_prefs::kDesktopToiOSTabGroupsPromoLastImpressionTimestamp);
  ASSERT_NO_FATAL_FAILURE(OverflowWebUIToolbar(showing_browser));

  ios_promos_utils::VerifyIOSPromoEligibility(
      desktop_to_mobile_promos::PromoType::kTabGroups, showing_browser);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return IOSPromoBubble::IsPromoTypeVisible(
        desktop_to_mobile_promos::PromoType::kTabGroups);
  }));

  CloseBrowserSynchronously(showing_browser);
  EXPECT_FALSE(IOSPromoBubble::IsPromoTypeVisible(
      desktop_to_mobile_promos::PromoType::kTabGroups));
}
#endif  // BUILDFLAG(IS_CHROMEOS)
