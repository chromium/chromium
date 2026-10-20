// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/origin_gating/content/origin_gating_tab_controller.h"

#include <memory>

#include "base/memory/weak_ptr.h"
#include "components/origin_gating/core/gateable_event.h"
#include "components/origin_gating/core/origin_gating_checker.h"
#include "components/origin_gating/core/origin_gating_configuration.h"
#include "components/origin_gating/core/origin_gating_registration.h"
#include "components/origin_gating/core/origin_gating_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace origin_gating {
namespace {

using ::testing::NiceMock;

class MockDelegate : public OriginGatingChecker::Delegate {
 public:
  MockDelegate() = default;
  ~MockDelegate() override = default;

  MOCK_METHOD(void,
              DoesOriginRequireUserConfirmation,
              (GatingDecisionContext * context,
               const GateableEvent& event,
               DoesOriginRequireUserConfirmationCallback callback),
              (const, override));
  MOCK_METHOD(void,
              EvaluateEnterprisePolicy,
              (const GURL& destination,
               EvaluateEnterprisePolicyCallback callback),
              (const, override));
  MOCK_METHOD(void,
              OnNoVerdict,
              (GatingDecisionContext * context,
               const GateableEvent& event,
               bool requires_user_confirmation,
               base::OnceCallback<void(NoVerdictResult)> callback),
              (override));

  base::WeakPtr<MockDelegate> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockDelegate> weak_ptr_factory_{this};
};

class OriginGatingTabControllerBrowserTest
    : public content::ContentBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    content::ContentBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  std::unique_ptr<OriginGatingRegistration> RegisterChecker() {
    return service_->CreateAndRegisterChecker(
        delegate_.GetWeakPtr(),
        OriginGatingConfiguration(
            {}, OriginGatingConfiguration::CacheScope::kOrigin));
  }

  NiceMock<MockDelegate> delegate_;
  std::unique_ptr<OriginGatingService> service_ =
      OriginGatingService::CreateForTesting();
};

IN_PROC_BROWSER_TEST_F(OriginGatingTabControllerBrowserTest,
                       WindowOpenPropagatesActiveCheckerToPopup) {
  ASSERT_TRUE(content::NavigateToURL(
      shell(), embedded_test_server()->GetURL("/title1.html")));

  std::unique_ptr<OriginGatingRegistration> registration = RegisterChecker();
  auto* controller = OriginGatingTabController::GetOrCreateForWebContents(
      shell()->web_contents(), *service_);
  controller->SetCheckerId(registration->id());

  content::ShellAddedObserver new_shell_observer;
  const GURL popup_url = embedded_test_server()->GetURL("/title2.html");
  ASSERT_TRUE(content::ExecJs(
      shell(), content::JsReplace("window.open($1);", popup_url)));
  content::Shell* popup_shell = new_shell_observer.GetShell();
  ASSERT_TRUE(popup_shell);

  auto* popup_controller =
      OriginGatingTabController::FromWebContents(popup_shell->web_contents());
  ASSERT_TRUE(popup_controller);
  EXPECT_EQ(popup_controller->GetChecker(), controller->GetChecker());
}

IN_PROC_BROWSER_TEST_F(
    OriginGatingTabControllerBrowserTest,
    TargetBlankNoopenerLinkPropagatesActiveCheckerToNewWindow) {
  ASSERT_TRUE(content::NavigateToURL(
      shell(), embedded_test_server()->GetURL("/title1.html")));

  std::unique_ptr<OriginGatingRegistration> registration = RegisterChecker();
  auto* controller = OriginGatingTabController::GetOrCreateForWebContents(
      shell()->web_contents(), *service_);
  controller->SetCheckerId(registration->id());

  content::ShellAddedObserver new_shell_observer;
  const GURL target_url = embedded_test_server()->GetURL("/title2.html");
  ASSERT_TRUE(content::ExecJs(
      shell(), content::JsReplace("const a = document.createElement('a');"
                                  "a.href = $1;"
                                  "a.target = '_blank';"
                                  "a.rel = 'noopener';"
                                  "document.body.appendChild(a);"
                                  "a.click();",
                                  target_url)));
  content::Shell* new_shell = new_shell_observer.GetShell();
  ASSERT_TRUE(new_shell);

  auto* new_controller =
      OriginGatingTabController::FromWebContents(new_shell->web_contents());
  ASSERT_TRUE(new_controller);
  EXPECT_EQ(new_controller->GetChecker(), controller->GetChecker());
}

IN_PROC_BROWSER_TEST_F(OriginGatingTabControllerBrowserTest,
                       WindowOpenDoesNotPropagateWhenCheckerCleared) {
  ASSERT_TRUE(content::NavigateToURL(
      shell(), embedded_test_server()->GetURL("/title1.html")));

  std::unique_ptr<OriginGatingRegistration> registration = RegisterChecker();
  auto* controller = OriginGatingTabController::GetOrCreateForWebContents(
      shell()->web_contents(), *service_);
  controller->SetCheckerId(registration->id());
  controller->ClearCheckerId();

  content::ShellAddedObserver new_shell_observer;
  const GURL popup_url = embedded_test_server()->GetURL("/title2.html");
  ASSERT_TRUE(content::ExecJs(
      shell(), content::JsReplace("window.open($1);", popup_url)));
  content::Shell* popup_shell = new_shell_observer.GetShell();
  ASSERT_TRUE(popup_shell);

  EXPECT_EQ(
      OriginGatingTabController::FromWebContents(popup_shell->web_contents()),
      nullptr);
}

}  // namespace
}  // namespace origin_gating
