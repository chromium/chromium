// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/privacy_sandbox/privacy_sandbox_dialog_view.h"

#include "base/synchronization/waitable_event.h"
#include "base/test/run_until.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/privacy_sandbox/mock_privacy_sandbox_service.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/privacy_sandbox/privacy_sandbox_prompt.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "privacy_sandbox_dialog_view.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"

namespace {
constexpr int kAverageBrowserWidth = 800;
constexpr int kAverageBrowserHeight = 700;
constexpr base::TimeDelta kMaxWaitTime = base::Seconds(30);

std::string ScrollToBottomScript() {
  return R"```(
    (async () => {
      return new Promise(async (resolve) => {
        requestIdleCallback(async () => {
          let dialogElement = document.querySelector("body > "+$1);
          if ($2 !== "") dialogElement = dialogElement.shadowRoot.querySelector($2);
          const scrollable = dialogElement.shadowRoot.querySelector('[scrollable]');
          scrollFunction = () => new Promise(scrollResolve => {
            let timeout = setTimeout(() => {
              scrollable.removeEventListener('scrollend', scrollEndCallback);
              scrollResolve();
            }, 2000);
            const scrollEndCallback = () => {
              clearTimeout(timeout);
              scrollable.removeEventListener('scrollend', scrollEndCallback);
              scrollResolve();
            };
            scrollable.addEventListener('scrollend', scrollEndCallback);
            scrollable.scrollTop = scrollable.scrollHeight;
          });
          waitUntilHidden = (el) => {
            return new Promise(overlayResolve => {
              const observer = new MutationObserver(mutations =>
                mutations.some(mutation => mutation.type === 'attributes' &&
                 mutation.attributeName === 'hidden' && el.hasAttribute('hidden')) &&
                requestAnimationFrame(() => (observer.disconnect(), overlayResolve()))
              );
              el.hasAttribute('hidden')
                ? requestAnimationFrame(overlayResolve)
                : observer.observe(el, { attributes: true, attributeFilter: ['hidden'] });
            });
          };
          await scrollFunction();
          await waitUntilHidden(dialogElement.shadowRoot.querySelector('#showMoreOverlay'));
          requestAnimationFrame(resolve);
        });
      });
    })();
  )```";
}

std::string ClickLearnMoreButton3TimesScript() {
  return R"(
    (async () => {
     return new Promise(async (resolve) => {
      requestAnimationFrame(async () => {
        dialogElement = document.querySelector("body > "+$1);
        if($2 !== "") dialogElement = dialogElement.shadowRoot.querySelector($2);
        const learnMoreElement = dialogElement.shadowRoot.querySelector($3);
        const expandButtonElement = learnMoreElement.shadowRoot.querySelector('div > cr-expand-button');
        const scrollable = dialogElement.shadowRoot.querySelector('[scrollable]');
        waitForEndScroll = (el) => new Promise(scrollResolve => {
          let timeout = setTimeout(() => {
            el.removeEventListener('scrollend', scrollEndCallback);
            scrollResolve();
          }, 2000);
          const scrollEndCallback = () => {
            clearTimeout(timeout);
            el.removeEventListener('scrollend', scrollEndCallback);
            scrollResolve();
          };
          el.addEventListener('scrollend', scrollEndCallback);
          expandButtonElement.click();
        });
        await waitForEndScroll(scrollable);
        expandButtonElement.click();
        await waitForEndScroll(scrollable);
        expandButtonElement.blur();
        setTimeout(resolve,0);
      });
     });
    })();
  )";
}

}  // namespace

class PrivacySandboxDialogViewBrowserTest : public DialogBrowserTest {
 public:
  PrivacySandboxDialogViewBrowserTest() {
    scoped_feature_list_.InitAndDisableFeature(
        privacy_sandbox::kPrivacySandboxAdsApiUxEnhancements);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");

    ASSERT_TRUE(embedded_test_server()->Start());
    mock_service_ = static_cast<MockPrivacySandboxService*>(
        PrivacySandboxServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            browser()->profile(),
            base::BindRepeating(&BuildMockPrivacySandboxService)));
  }

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    PrivacySandboxService::PromptType prompt_type =
        PrivacySandboxService::PromptType::kNone;
    if (name == "Consent") {
      prompt_type = PrivacySandboxService::PromptType::kM1Consent;
    }
    if (name == "Notice") {
      prompt_type = PrivacySandboxService::PromptType::kM1NoticeROW;
    }
    if (name == "RestrictedNotice") {
      prompt_type = PrivacySandboxService::PromptType::kM1NoticeRestricted;
    }
    ASSERT_NE(prompt_type, PrivacySandboxService::PromptType::kNone);

    // Resize the browser window to guarantee enough space for the dialog.
    BrowserView::GetBrowserViewForBrowser(browser())->GetWidget()->SetBounds(
        {0, 0, kAverageBrowserWidth, kAverageBrowserHeight});

    views::NamedWidgetShownWaiter waiter(
        views::test::AnyWidgetTestPasskey{},
        PrivacySandboxDialogView::kViewClassName);
    ShowPrivacySandboxDialog(browser(), prompt_type);
    waiter.WaitIfNeededAndGet();
  }

  MockPrivacySandboxService* mock_service() { return mock_service_; }

 private:
  raw_ptr<MockPrivacySandboxService, DanglingUntriaged> mock_service_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

#if !BUILDFLAG(IS_LINUX)
IN_PROC_BROWSER_TEST_F(PrivacySandboxDialogViewBrowserTest, InvokeUi_Consent) {
  base::WaitableEvent shown_waiter;
  base::WaitableEvent closed_waiter;

  EXPECT_CALL(
      *mock_service(),
      PromptActionOccurred(PrivacySandboxService::PromptAction::kConsentShown,
                           PrivacySandboxService::SurfaceType::kDesktop))
      .WillOnce([&shown_waiter] { shown_waiter.Signal(); });
  EXPECT_CALL(*mock_service(),
              PromptActionOccurred(
                  PrivacySandboxService::PromptAction::kConsentClosedNoDecision,
                  PrivacySandboxService::SurfaceType::kDesktop))
      .WillOnce([&closed_waiter] { closed_waiter.Signal(); });
  ShowAndVerifyUi();

  LOG(INFO) << "Waiting for callback";
  shown_waiter.TimedWait(kMaxWaitTime);
  closed_waiter.TimedWait(kMaxWaitTime);
}

IN_PROC_BROWSER_TEST_F(PrivacySandboxDialogViewBrowserTest, InvokeUi_Notice) {
  base::WaitableEvent shown_waiter;
  base::WaitableEvent closed_waiter;

  EXPECT_CALL(
      *mock_service(),
      PromptActionOccurred(PrivacySandboxService::PromptAction::kNoticeShown,
                           PrivacySandboxService::SurfaceType::kDesktop))
      .WillOnce([&shown_waiter] { shown_waiter.Signal(); });
  EXPECT_CALL(
      *mock_service(),
      PromptActionOccurred(
          PrivacySandboxService::PromptAction::kNoticeClosedNoInteraction,
          PrivacySandboxService::SurfaceType::kDesktop))
      .WillOnce([&closed_waiter] { closed_waiter.Signal(); });
  ShowAndVerifyUi();

  LOG(INFO) << "Waiting for callback";
  shown_waiter.TimedWait(kMaxWaitTime);
  closed_waiter.TimedWait(kMaxWaitTime);
}

IN_PROC_BROWSER_TEST_F(PrivacySandboxDialogViewBrowserTest,
                       InvokeUi_RestrictedNotice) {
  base::WaitableEvent shown_waiter;
  base::WaitableEvent closed_waiter;

  EXPECT_CALL(*mock_service(),
              PromptActionOccurred(
                  PrivacySandboxService::PromptAction::kRestrictedNoticeShown,
                  PrivacySandboxService::SurfaceType::kDesktop))
      .WillOnce([&shown_waiter] { shown_waiter.Signal(); });
  EXPECT_CALL(
      *mock_service(),
      PromptActionOccurred(PrivacySandboxService::PromptAction::
                               kRestrictedNoticeClosedNoInteraction,
                           PrivacySandboxService::SurfaceType::kDesktop))
      .WillOnce([&closed_waiter] { closed_waiter.Signal(); });
  ShowAndVerifyUi();

  LOG(INFO) << "Waiting for callback";
  shown_waiter.TimedWait(kMaxWaitTime);
  closed_waiter.TimedWait(kMaxWaitTime);
}
#endif

class PrivacySandboxDialogViewAdsApiUxEnhancementBrowserTest
    : public PrivacySandboxDialogViewBrowserTest {
 public:
  PrivacySandboxDialogViewAdsApiUxEnhancementBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        // Enabled Features
        {privacy_sandbox::kPrivacySandboxAdsApiUxEnhancements},
        // Disabled Features
        {privacy_sandbox::kPrivacySandboxAdTopicsContentParity});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(PrivacySandboxDialogViewAdsApiUxEnhancementBrowserTest,
                       InvokeUi_Consent) {
  base::WaitableEvent shown_waiter;
  base::WaitableEvent closed_waiter;

  EXPECT_CALL(
      *mock_service(),
      PromptActionOccurred(PrivacySandboxService::PromptAction::kConsentShown,
                           PrivacySandboxService::SurfaceType::kDesktop))
      .WillOnce([&shown_waiter] { shown_waiter.Signal(); });
  EXPECT_CALL(*mock_service(),
              PromptActionOccurred(
                  PrivacySandboxService::PromptAction::kConsentClosedNoDecision,
                  PrivacySandboxService::SurfaceType::kDesktop))
      .WillOnce([&closed_waiter] { closed_waiter.Signal(); });
  ShowAndVerifyUi();

  shown_waiter.TimedWait(kMaxWaitTime);
  closed_waiter.TimedWait(kMaxWaitTime);
}

// TODO(crbug.com/396446633): Add pixel tests for other dialogs with ads api ux
// enhancements and ad topics content parity.

class PrivacySandboxDialogViewPrivacyPolicyBrowserTest
    : public PrivacySandboxDialogViewBrowserTest {
 public:
  PrivacySandboxDialogViewPrivacyPolicyBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        // Enabled
        {},
        // Disabled
        {privacy_sandbox::kPrivacySandboxAdsApiUxEnhancements});
  }

  virtual std::string GetPrivacyPolicyLinkElementId() {
    return "#privacyPolicyLink";
  }

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    PrivacySandboxService::PromptType prompt_type =
        PrivacySandboxService::PromptType::kM1Consent;

    // Resize the browser window to guarantee enough space for the dialog.
    BrowserView::GetBrowserViewForBrowser(browser())->GetWidget()->SetBounds(
        {0, 0, kAverageBrowserWidth, kAverageBrowserHeight});

    views::NamedWidgetShownWaiter waiter(
        views::test::AnyWidgetTestPasskey{},
        PrivacySandboxDialogView::kViewClassName);
    ShowPrivacySandboxDialog(browser(), prompt_type);
    views::Widget* dialog_widget = waiter.WaitIfNeededAndGet();
    views::test::WidgetVisibleWaiter(dialog_widget).Wait();
    ASSERT_TRUE(dialog_widget->IsVisible());

    auto* privacy_sandbox_dialog_view = static_cast<PrivacySandboxDialogView*>(
        dialog_widget->widget_delegate()->GetContentsView());

    // Click expand button.
    EXPECT_TRUE(
        content::ExecJs(privacy_sandbox_dialog_view->GetWebContentsForTesting(),
                        "document.querySelector('body > "
                        "privacy-sandbox-combined-dialog-app').shadowRoot."
                        "querySelector('#consent').shadowRoot.querySelector('"
                        "privacy-sandbox-dialog-learn-more').shadowRoot."
                        "querySelector('div > cr-expand-button').click()"));

    // Click Privacy Policy link.
    EXPECT_TRUE(
        content::ExecJs(privacy_sandbox_dialog_view->GetWebContentsForTesting(),
                        "document.querySelector('body > "
                        "privacy-sandbox-combined-dialog-app')"
                        ".shadowRoot.querySelector('#consent')"
                        ".shadowRoot.querySelector('" +
                            GetPrivacyPolicyLinkElementId() +
                            "')"
                            ".click()"));

    // Intentionally navigate to some blocked content to avoid flakiness.
    auto script = content::JsReplace(
        "document.querySelector('body > "
        "privacy-sandbox-combined-dialog-app')"
        ".shadowRoot.querySelector('#consent')"
        ".shadowRoot.querySelector('privacy-sandbox-privacy-policy-dialog')"
        ".shadowRoot.querySelector('#privacyPolicy').src = $1;",
        embedded_test_server()->GetURL("/blue.html"));
    EXPECT_TRUE(content::ExecJs(
        privacy_sandbox_dialog_view->GetWebContentsForTesting(), script));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(PrivacySandboxDialogViewPrivacyPolicyBrowserTest,
                       InvokeUi_PrivacyPolicy) {
  ShowAndVerifyUi();
}

class PrivacySandboxDialogViewAdsApiUxEnhancementPrivacyPolicyBrowserTest
    : public PrivacySandboxDialogViewPrivacyPolicyBrowserTest {
 public:
  PrivacySandboxDialogViewAdsApiUxEnhancementPrivacyPolicyBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        // Enabled Features
        {privacy_sandbox::kPrivacySandboxAdsApiUxEnhancements},
        // Disabled Features
        {privacy_sandbox::kPrivacySandboxAdTopicsContentParity});
  }

  std::string GetPrivacyPolicyLinkElementId() override {
    return "#privacyPolicyLinkV2";
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    PrivacySandboxDialogViewAdsApiUxEnhancementPrivacyPolicyBrowserTest,
    InvokeUi_PrivacyPolicy) {
  ShowAndVerifyUi();
}

// TODO(crbug.com/378886088): Refactor file to reduce code duplication between
// different test classes.
class PrivacySandboxDialogViewAdsApiUxEnhancementsLearnMoreBrowserTest
    : public PrivacySandboxDialogViewBrowserTest {
 public:
  PrivacySandboxDialogViewAdsApiUxEnhancementsLearnMoreBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        // Enabled Features
        {privacy_sandbox::kPrivacySandboxAdsApiUxEnhancements},
        // Disabled Features
        {privacy_sandbox::kPrivacySandboxAdTopicsContentParity,
         privacy_sandbox::kPrivacySandboxEqualizedPromptButtons});
  }

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    // Resize the browser window to guarantee enough space for the dialog.
    BrowserView::GetBrowserViewForBrowser(browser())->GetWidget()->SetBounds(
        {0, 0, kAverageBrowserWidth, kAverageBrowserHeight});

    views::NamedWidgetShownWaiter waiter(
        views::test::AnyWidgetTestPasskey{},
        PrivacySandboxDialogView::kViewClassName);
    ShowPrivacySandboxDialog(browser(), GetPromptType(name));
    views::Widget* dialog_widget = waiter.WaitIfNeededAndGet();
    views::test::WidgetVisibleWaiter(dialog_widget).Wait();
    ASSERT_TRUE(dialog_widget->IsVisible());

    auto* privacy_sandbox_dialog_view = static_cast<PrivacySandboxDialogView*>(
        dialog_widget->widget_delegate()->GetContentsView());

    auto [primary_selector, secondary_selector] =
        GetDialogElementSelector(name);

    // Open, close, and reopen the learn more section. This ensures that there
    // is no behind the scenes rendering that could cause a layout issue. This
    // adds test coverage for a past regression where only the second expand
    // caused a layout issue (crbug.com/388420268).
    EXPECT_TRUE(
        content::ExecJs(privacy_sandbox_dialog_view->GetWebContentsForTesting(),
                        content::JsReplace(ClickLearnMoreButton3TimesScript(),
                                           primary_selector, secondary_selector,
                                           GetLearnMoreElementSelector(name))));

    // Scroll the view to the bottom before taking a screenshot.
    EXPECT_TRUE(content::ExecJs(
        privacy_sandbox_dialog_view->GetWebContentsForTesting(),
        content::JsReplace(ScrollToBottomScript(), primary_selector,
                           secondary_selector)));
  }

 private:
  PrivacySandboxService::PromptType GetPromptType(std::string_view name) {
    if (name == "ConsentEEA") {
      return PrivacySandboxService::PromptType::kM1Consent;
    }
    if (name == "NoticeEEAsiteSuggestedAds" ||
        name == "NoticeEEAadsMeasurementLearnMore") {
      return PrivacySandboxService::PromptType::kM1NoticeEEA;
    }
    if (name == "NoticeROW") {
      return PrivacySandboxService::PromptType::kM1NoticeROW;
    }
    NOTREACHED();
  }

  std::pair<std::string, std::string> GetDialogElementSelector(
      const std::string& name) {
    if (name == "ConsentEEA") {
      return std::make_pair("privacy-sandbox-combined-dialog-app", "#consent");
    }
    if (name == "NoticeEEAsiteSuggestedAds" ||
        name == "NoticeEEAadsMeasurementLearnMore") {
      return std::make_pair("privacy-sandbox-combined-dialog-app", "#notice");
    }
    if (name == "NoticeROW") {
      return std::make_pair("privacy-sandbox-notice-dialog-app", "");
    }
    NOTREACHED();
  }

  std::string GetLearnMoreElementSelector(const std::string& name) {
    if (name == "NoticeEEAsiteSuggestedAds") {
      return "#siteSuggestedAdsLearnMore";
    }
    if (name == "NoticeEEAadsMeasurementLearnMore") {
      return "#adsMeasurementLearnMore";
    }
    if (name == "ConsentEEA" || name == "NoticeROW") {
      return "privacy-sandbox-dialog-learn-more";
    }
    NOTREACHED();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    PrivacySandboxDialogViewAdsApiUxEnhancementsLearnMoreBrowserTest,
    InvokeUi_ConsentEEA) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(
    PrivacySandboxDialogViewAdsApiUxEnhancementsLearnMoreBrowserTest,
    InvokeUi_NoticeEEAsiteSuggestedAds) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(
    PrivacySandboxDialogViewAdsApiUxEnhancementsLearnMoreBrowserTest,
    InvokeUi_NoticeEEAadsMeasurementLearnMore) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(
    PrivacySandboxDialogViewAdsApiUxEnhancementsLearnMoreBrowserTest,
    InvokeUi_NoticeROW) {
  ShowAndVerifyUi();
}
