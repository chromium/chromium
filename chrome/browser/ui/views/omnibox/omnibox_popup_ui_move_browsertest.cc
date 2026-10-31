// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_state_manager.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_aim_presenter.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_base.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_base_content.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_handler.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_ui.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_web_contents_helper.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/contextual_search/contextual_search_session_handle.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/omnibox/browser/aim_eligibility_service.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/mock_aim_eligibility_service.h"
#include "components/variations/service/variations_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "ui/gfx/range/range.h"

namespace {

// Returns the input of the last result that the classic popup page received.
constexpr char kPageResultInputScript[] = R"(
  (() => {
    const result = document.querySelector('omnibox-popup-app').result_;
    return result ? result.input : '';
  })();
)";

}  // namespace

// Tests moving the classic WebUI popup's WebContents to another browser
// window with `OmniboxPopupUI::DetachFromWindow()` and `AttachToWindow()`.
class OmniboxPopupUIMoveBrowserTest : public InProcessBrowserTest {
 public:
  OmniboxPopupUIMoveBrowserTest() {
    feature_list_.InitWithFeatures({omnibox::internal::kWebUIOmniboxPopup},
                                   {features::kWebUILocationBar});
  }

 protected:
  static LocationBarView* GetLocationBar(BrowserWindowInterface* browser) {
    return BrowserView::GetBrowserViewForBrowser(browser)
        ->toolbar()
        ->location_bar_view();
  }

  static OmniboxController* GetOmniboxController(
      BrowserWindowInterface* browser) {
    return GetLocationBar(browser)->GetOmniboxController();
  }

  // Runs an omnibox query in `browser` whose synchronous matches open the
  // suggestion popup.
  static void StartAutocomplete(BrowserWindowInterface* browser,
                                const std::u16string& text) {
    OmniboxController* controller = GetOmniboxController(browser);
    controller->edit_model()->SetUserText(text);
    AutocompleteInput input(
        text, metrics::OmniboxEventProto::BLANK,
        ChromeAutocompleteSchemeClassifier(browser->GetProfile()));
    input.set_omit_asynchronous_matches(true);
    controller->StartAutocomplete(input);
  }

  // Opens and closes the suggestion popup of `browser()`, and returns its
  // loaded WebContents.
  content::WebContents* CreateLoadedPopupContents() {
    StartAutocomplete(browser(), u"foo");
    OmniboxPopupPresenterBase* presenter =
        GetLocationBar(browser())->GetOmniboxPopupView()->presenter();
    OmniboxPopupWebUIBaseContent* content = presenter->GetWebUIContent();
    CHECK(content);
    content::WebContents* contents = content->GetWrappedWebContents();
    CHECK(content::WaitForLoadStop(contents));
    GetOmniboxController(browser())->StopAutocomplete(/*clear_result=*/true);
    return contents;
  }

  static OmniboxPopupUI* GetPopupUI(content::WebContents* contents) {
    return contents->GetWebUI()->GetController()->GetAs<OmniboxPopupUI>();
  }

  // Moves `contents` to `target` as the popup's views layer does.
  void MovePopupContents(content::WebContents* contents,
                         BrowserWindowInterface* target) {
    GetPopupUI(contents)->DetachFromWindow();
    AttachPopupContents(contents, target);
  }

  // Attaches detached `contents` to `target`.
  void AttachPopupContents(content::WebContents* contents,
                           BrowserWindowInterface* target) {
    webui::SetBrowserWindowInterface(contents, target);
    GetPopupUI(contents)->AttachToWindow(GetOmniboxController(target));
    moved_contents_ = contents->GetWeakPtr();
  }

  static std::string GetPageResultInput(content::WebContents* contents) {
    return content::EvalJs(contents, kPageResultInputScript).ExtractString();
  }

  // The moved popup is still owned by the first window, which closes last.
  // Move it back so that its handlers don't point into a closed window.
  void TearDownOnMainThread() override {
    if (moved_contents_) {
      MovePopupContents(moved_contents_.get(), browser());
    }
    InProcessBrowserTest::TearDownOnMainThread();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  base::WeakPtr<content::WebContents> moved_contents_;
};

// After the move, the page shows results from the new window's omnibox.
IN_PROC_BROWSER_TEST_F(OmniboxPopupUIMoveBrowserTest,
                       PageFollowsNewWindowOmnibox) {
  content::WebContents* contents = CreateLoadedPopupContents();
  OmniboxPopupUI* popup_ui = GetPopupUI(contents);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return popup_ui->omnibox_handler() && popup_ui->popup_handler();
  }));
  BrowserWindowInterface* other_browser =
      CreateBrowser(browser()->GetProfile());

  MovePopupContents(contents, other_browser);

  StartAutocomplete(other_browser, u"bar");
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return GetPageResultInput(contents) == "bar"; }));
}

// The move sends the new window's current results to the page right away.
IN_PROC_BROWSER_TEST_F(OmniboxPopupUIMoveBrowserTest,
                       SendsNewWindowResultsOnAttach) {
  content::WebContents* contents = CreateLoadedPopupContents();
  OmniboxPopupUI* popup_ui = GetPopupUI(contents);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return popup_ui->omnibox_handler() && popup_ui->popup_handler();
  }));
  BrowserWindowInterface* other_browser =
      CreateBrowser(browser()->GetProfile());
  StartAutocomplete(other_browser, u"bar");

  MovePopupContents(contents, other_browser);

  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return GetPageResultInput(contents) == "bar"; }));
}

// The move drops the text state that the page reported in the old window, and
// the contextual session that holds the old window's context.
IN_PROC_BROWSER_TEST_F(OmniboxPopupUIMoveBrowserTest, DropsOldWindowState) {
  content::WebContents* contents = CreateLoadedPopupContents();
  OmniboxPopupUI* popup_ui = GetPopupUI(contents);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return popup_ui->omnibox_handler() && popup_ui->popup_handler();
  }));
  popup_ui->popup_handler()->OnSelectionChanged(gfx::Range(1, 2),
                                                /*sequence_number=*/0,
                                                /*show_full_url=*/true);
  contextual_search::ContextualSearchSessionHandle* session =
      popup_ui->GetOrCreateContextualSessionHandle();
  ASSERT_TRUE(session);
  base::WeakPtr<contextual_search::ContextualSearchSessionHandle> old_session =
      session->AsWeakPtr();
  BrowserWindowInterface* other_browser =
      CreateBrowser(browser()->GetProfile());

  MovePopupContents(contents, other_browser);

  EXPECT_FALSE(old_session);
  EXPECT_EQ(popup_ui->popup_handler()->latest_selection(), gfx::Range());
  EXPECT_FALSE(popup_ui->popup_handler()->show_full_url());
}

// Detaching leaves no pointer into the old window, so the old window can close
// before the popup is attached to another one.
IN_PROC_BROWSER_TEST_F(OmniboxPopupUIMoveBrowserTest,
                       OldWindowClosesWhileDetached) {
  content::WebContents* contents = CreateLoadedPopupContents();
  OmniboxPopupUI* popup_ui = GetPopupUI(contents);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return popup_ui->omnibox_handler() && popup_ui->popup_handler();
  }));
  BrowserWindowInterface* other_browser =
      CreateBrowser(browser()->GetProfile());
  MovePopupContents(contents, other_browser);

  popup_ui->DetachFromWindow();
  EXPECT_FALSE(OmniboxPopupWebContentsHelper::FromWebContents(contents)
                   ->get_omnibox_controller());
  CloseBrowserSynchronously(other_browser);
  AttachPopupContents(contents, browser());

  StartAutocomplete(browser(), u"baz");
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return GetPageResultInput(contents) == "baz"; }));
}

// Tests moving the AI Mode popup, whose handler runs its own autocomplete.
class OmniboxPopupUIAimMoveBrowserTest : public OmniboxPopupUIMoveBrowserTest {
 public:
  OmniboxPopupUIAimMoveBrowserTest() {
    aim_feature_list_.InitAndEnableFeature(
        omnibox::internal::kWebUIOmniboxAimPopup);
  }

  void SetUpInProcessBrowserTestFixture() override {
    OmniboxPopupUIMoveBrowserTest::SetUpInProcessBrowserTestFixture();
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
    OmniboxPopupUIMoveBrowserTest::SetUpOnMainThread();
    g_browser_process->variations_service()->OverrideStoredPermanentCountry(
        "us");
  }

 protected:
  // Opens and closes the AI Mode popup of `browser()`, and returns its loaded
  // WebContents once its composebox handler is bound.
  content::WebContents* CreateLoadedAimPopupContents() {
    OmniboxController* controller = GetOmniboxController(browser());
    controller->popup_state_manager()->SetPopupState(OmniboxPopupState::kAim);
    OmniboxPopupAimPresenter* presenter =
        GetLocationBar(browser())->GetOmniboxPopupAimPresenter();
    CHECK(presenter);
    presenter->Show();
    OmniboxPopupWebUIBaseContent* content = presenter->GetWebUIContent();
    CHECK(content);
    content::WebContents* contents = content->GetWrappedWebContents();
    CHECK(content::WaitForLoadStop(contents));
    CHECK(base::test::RunUntil(
        [&]() { return GetPopupUI(contents)->composebox_handler(); }));
    controller->popup_state_manager()->SetPopupState(OmniboxPopupState::kNone);
    return contents;
  }

 private:
  static std::unique_ptr<KeyedService> BuildMockAimEligibilityService(
      content::BrowserContext* context) {
    auto service =
        std::make_unique<testing::NiceMock<MockAimEligibilityService>>(
            *Profile::FromBrowserContext(context)->GetPrefs(),
            /*template_url_service=*/nullptr,
            /*url_loader_factory=*/nullptr, /*identity_manager=*/nullptr,
            AimEligibilityService::Configuration{});
    ON_CALL(*service, IsAimEligible()).WillByDefault(testing::Return(true));
    ON_CALL(*service, GetLocaleImpl()).WillByDefault(testing::Return("en-US"));
    return service;
  }

  base::test::ScopedFeatureList aim_feature_list_;
  base::CallbackListSubscription create_services_subscription_;
};

// The AI Mode popup moves like the classic one, and its old window can close
// while it is detached.
IN_PROC_BROWSER_TEST_F(OmniboxPopupUIAimMoveBrowserTest,
                       MovesAndSurvivesOldWindowClose) {
  content::WebContents* contents = CreateLoadedAimPopupContents();
  BrowserWindowInterface* other_browser =
      CreateBrowser(browser()->GetProfile());
  MovePopupContents(contents, other_browser);
  EXPECT_EQ(OmniboxPopupWebContentsHelper::FromWebContents(contents)
                ->get_omnibox_controller(),
            GetOmniboxController(other_browser));

  GetPopupUI(contents)->DetachFromWindow();
  CloseBrowserSynchronously(other_browser);
  AttachPopupContents(contents, browser());

  EXPECT_TRUE(GetPopupUI(contents)->composebox_handler());
  EXPECT_EQ(OmniboxPopupWebContentsHelper::FromWebContents(contents)
                ->get_omnibox_controller(),
            GetOmniboxController(browser()));
}
