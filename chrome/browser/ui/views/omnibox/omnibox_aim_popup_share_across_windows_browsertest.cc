// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_state_manager.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_aim_presenter.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_base.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_delegate.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_base_content.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_pool.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_pool_factory.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_aim_handler.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_ui.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/omnibox/browser/aim_eligibility_service.h"
#include "components/omnibox/browser/mock_aim_eligibility_service.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/variations/service/variations_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

std::unique_ptr<KeyedService> BuildMockAimEligibilityService(
    content::BrowserContext* context) {
  auto service = std::make_unique<testing::NiceMock<MockAimEligibilityService>>(
      *Profile::FromBrowserContext(context)->GetPrefs(),
      /*template_url_service=*/nullptr,
      /*url_loader_factory=*/nullptr, /*identity_manager=*/nullptr,
      AimEligibilityService::Configuration{});
  ON_CALL(*service, IsAimEligible()).WillByDefault(testing::Return(true));
  ON_CALL(*service, GetLocaleImpl()).WillByDefault(testing::Return("en-US"));
  return service;
}

}  // namespace

// Tests that browser windows of a profile share the AI Mode WebUI popup with
// `omnibox::kOmniboxWebUIPopupShareAcrossWindows`.
class OmniboxAimPopupShareAcrossWindowsBrowserTest
    : public InProcessBrowserTest {
 public:
  OmniboxAimPopupShareAcrossWindowsBrowserTest() {
    feature_list_.InitWithFeatures(
        {omnibox::internal::kWebUIOmniboxPopup,
         omnibox::internal::kWebUIOmniboxAimPopup,
         omnibox::kOmniboxWebUIPopupShareAcrossWindows},
        {features::kWebUILocationBar});
  }

  void SetUpInProcessBrowserTestFixture() override {
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating([](content::BrowserContext* context) {
                  AimEligibilityServiceFactory::GetInstance()
                      ->SetTestingFactory(
                          context,
                          base::BindRepeating(&BuildMockAimEligibilityService));
                }));
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    g_browser_process->variations_service()->OverrideStoredPermanentCountry(
        "us");
  }

 protected:
  static LocationBar* GetLocationBar(BrowserWindowInterface* browser) {
    return BrowserView::GetBrowserViewForBrowser(browser)->GetLocationBar();
  }

  static OmniboxPopupAimPresenter* GetAimPresenter(
      BrowserWindowInterface* browser) {
    return GetLocationBar(browser)
        ->GetPresenterDelegate()
        ->GetOmniboxPopupAimPresenter();
  }

  static content::WebContents* GetContents(
      OmniboxPopupPresenterBase* presenter) {
    OmniboxPopupWebUIBaseContent* content = presenter->GetWebUIContent();
    return content ? content->GetWrappedWebContents() : nullptr;
  }

  static void SetPopupState(BrowserWindowInterface* browser,
                            OmniboxPopupState state) {
    GetLocationBar(browser)
        ->GetOmniboxController()
        ->popup_state_manager()
        ->SetPopupState(state);
  }

  static OmniboxPopupUI* GetPopupUI(content::WebContents* contents) {
    return contents->GetWebUI()->GetController()->GetAs<OmniboxPopupUI>();
  }

  // Shows the AI Mode popup of `browser` and waits until its page has bound
  // its handlers. Returns the popup's WebContents.
  static content::WebContents* ShowLoadedAimPopup(
      BrowserWindowInterface* browser) {
    SetPopupState(browser, OmniboxPopupState::kAim);
    OmniboxPopupAimPresenter* presenter = GetAimPresenter(browser);
    CHECK(presenter);
    presenter->Show();
    content::WebContents* contents = GetContents(presenter);
    CHECK(contents);
    CHECK(content::WaitForLoadStop(contents));
    OmniboxPopupUI* popup_ui = GetPopupUI(contents);
    CHECK(base::test::RunUntil([&]() {
      return popup_ui->popup_aim_handler() && popup_ui->composebox_handler();
    }));
    return contents;
  }

  // Hides the AI Mode popup of `browser`. The popup is busy until the page
  // replies to `ClearPopup()`.
  static void HideAimPopup(BrowserWindowInterface* browser) {
    SetPopupState(browser, OmniboxPopupState::kNone);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  base::CallbackListSubscription create_services_subscription_;
};

// Showing the AI Mode popup in a second window moves the idle AI Mode popup of
// the first window there.
IN_PROC_BROWSER_TEST_F(OmniboxAimPopupShareAcrossWindowsBrowserTest,
                       ShowTakesIdleAimPopup) {
  BrowserWindowInterface* other_browser =
      CreateBrowser(browser()->GetProfile());
  content::WebContents* contents = ShowLoadedAimPopup(browser());
  HideAimPopup(browser());
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return GetAimPresenter(browser())->CanReleaseWebUIContents(); }));

  SetPopupState(other_browser, OmniboxPopupState::kAim);
  GetAimPresenter(other_browser)->Show();

  EXPECT_EQ(GetContents(GetAimPresenter(other_browser)), contents);
  EXPECT_FALSE(GetAimPresenter(browser())->GetWebUIContent());
}

// The classic popup's context menu needs the AI Mode page, and takes an idle
// one from another window.
IN_PROC_BROWSER_TEST_F(OmniboxAimPopupShareAcrossWindowsBrowserTest,
                       ContextMenuTakesIdleAimPopup) {
  BrowserWindowInterface* other_browser =
      CreateBrowser(browser()->GetProfile());
  content::WebContents* contents = ShowLoadedAimPopup(browser());
  HideAimPopup(browser());
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return GetAimPresenter(browser())->CanReleaseWebUIContents(); }));

  GetAimPresenter(other_browser)
      ->GetOrCreateWebUIContent(
          OmniboxPopupPresenterBase::CreationReason::kContextMenu);

  EXPECT_EQ(GetContents(GetAimPresenter(other_browser)), contents);
}

// A classic popup and an AI Mode popup never take each other's page.
IN_PROC_BROWSER_TEST_F(OmniboxAimPopupShareAcrossWindowsBrowserTest,
                       KindsDoNotMix) {
  BrowserWindowInterface* other_browser =
      CreateBrowser(browser()->GetProfile());
  content::WebContents* aim_contents = ShowLoadedAimPopup(browser());
  HideAimPopup(browser());
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return GetAimPresenter(browser())->CanReleaseWebUIContents(); }));

  OmniboxPopupPresenterBase* other_classic_presenter =
      GetLocationBar(other_browser)->GetOmniboxPopupView()->presenter();
  other_classic_presenter->GetOrCreateWebUIContent(
      OmniboxPopupPresenterBase::CreationReason::kShow);

  content::WebContents* classic_contents = GetContents(other_classic_presenter);
  ASSERT_TRUE(classic_contents);
  EXPECT_NE(classic_contents, aim_contents);
  EXPECT_EQ(GetContents(GetAimPresenter(browser())), aim_contents);
}

// An AI Mode popup waiting for the page's `ClearPopup()` reply stays in its
// window, since the reply applies the draft to that window's omnibox.
IN_PROC_BROWSER_TEST_F(OmniboxAimPopupShareAcrossWindowsBrowserTest,
                       DoesNotTakeAimPopupWhileClearIsPending) {
  BrowserWindowInterface* other_browser =
      CreateBrowser(browser()->GetProfile());
  content::WebContents* contents = ShowLoadedAimPopup(browser());

  // The reply cannot arrive within this task.
  HideAimPopup(browser());
  EXPECT_FALSE(GetAimPresenter(browser())->CanReleaseWebUIContents());
  GetAimPresenter(other_browser)
      ->GetOrCreateWebUIContent(
          OmniboxPopupPresenterBase::CreationReason::kShow);

  EXPECT_EQ(GetContents(GetAimPresenter(browser())), contents);
  content::WebContents* other_contents =
      GetContents(GetAimPresenter(other_browser));
  ASSERT_TRUE(other_contents);
  EXPECT_NE(other_contents, contents);
}

// An AI Mode popup that keeps its context for its window's next show stays in
// that window.
IN_PROC_BROWSER_TEST_F(OmniboxAimPopupShareAcrossWindowsBrowserTest,
                       DoesNotTakeAimPopupThatPreservesContext) {
  BrowserWindowInterface* other_browser =
      CreateBrowser(browser()->GetProfile());
  content::WebContents* contents = ShowLoadedAimPopup(browser());
  HideAimPopup(browser());
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return GetAimPresenter(browser())->CanReleaseWebUIContents(); }));

  GetPopupUI(contents)->popup_aim_handler()->SetPreserveContextOnClose(true);
  EXPECT_FALSE(GetAimPresenter(browser())->CanReleaseWebUIContents());

  GetAimPresenter(other_browser)
      ->GetOrCreateWebUIContent(
          OmniboxPopupPresenterBase::CreationReason::kShow);

  EXPECT_EQ(GetContents(GetAimPresenter(browser())), contents);
  EXPECT_NE(GetContents(GetAimPresenter(other_browser)), contents);
}

// A page that crashed before replying to `ClearPopup()` does not keep the
// popup busy: the next show starts a new session.
IN_PROC_BROWSER_TEST_F(OmniboxAimPopupShareAcrossWindowsBrowserTest,
                       DoesNotStayBusyAfterCrashWithClearPending) {
  content::WebContents* contents = ShowLoadedAimPopup(browser());
  HideAimPopup(browser());
  content::CrashTab(contents);

  // Showing again reloads the page. Hiding before the new page binds its
  // handler sends no `ClearPopup()`.
  SetPopupState(browser(), OmniboxPopupState::kAim);
  OmniboxPopupAimPresenter* presenter = GetAimPresenter(browser());
  presenter->Show();
  content::WebContents* new_contents = GetContents(presenter);
  ASSERT_TRUE(new_contents);
  HideAimPopup(browser());
  ASSERT_TRUE(content::WaitForLoadStop(new_contents));

  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return presenter->CanReleaseWebUIContents(); }));
}

// Memory pressure drops the idle AI Mode popup, even in the window that was
// activated last.
IN_PROC_BROWSER_TEST_F(OmniboxAimPopupShareAcrossWindowsBrowserTest,
                       DropIdlePopupsDropsAimPopup) {
  ShowLoadedAimPopup(browser());
  HideAimPopup(browser());
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return GetAimPresenter(browser())->CanReleaseWebUIContents(); }));
  BrowserView::GetBrowserViewForBrowser(browser())
      ->GetWidget()
      ->OnNativeWidgetActivationChanged(/*active=*/true);

  OmniboxPopupWebUIPoolFactory::GetForProfile(browser()->GetProfile())
      ->DropIdleWebUIContents();

  EXPECT_FALSE(GetAimPresenter(browser())->GetWebUIContent());
}
