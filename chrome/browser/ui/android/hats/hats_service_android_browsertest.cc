// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/hats/hats_service_android.h"

#include <memory>

#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace hats {

namespace {

const char kTestSurveyTrigger[] = "testing";

class SurveyObserver {
 public:
  SurveyObserver() = default;

  void Accept() { accepted_ = true; }

  void Dismiss() { dismissed_ = true; }

  bool IsAccepted() { return accepted_; }

  bool IsDismissed() { return dismissed_; }

  base::WeakPtr<SurveyObserver> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  bool accepted_ = false;
  bool dismissed_ = false;

  base::WeakPtrFactory<SurveyObserver> weak_ptr_factory_{this};
};

}  // namespace

class HatsServiceAndroidBrowserTest : public AndroidBrowserTest {
 public:
  HatsServiceAndroidBrowserTest() = default;
  ~HatsServiceAndroidBrowserTest() override = default;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  HatsServiceAndroid* GetHatsService() {
    HatsServiceAndroid* service =
        static_cast<HatsServiceAndroid*>(HatsServiceFactory::GetForProfile(
            Profile::FromBrowserContext(web_contents()->GetBrowserContext()),
            true));
    return service;
  }

  bool NavigateTo(std::string_view hostname, std::string_view relative_url) {
    auto* web_contents = this->web_contents();
    content::TestNavigationObserver observer(web_contents);
    bool is_same_url = content::NavigateToURL(
        web_contents, embedded_test_server()->GetURL(hostname, relative_url));
    observer.Wait();
    return is_same_url;
  }
};

IN_PROC_BROWSER_TEST_F(HatsServiceAndroidBrowserTest,
                       NavigationBehavior_AllowAny) {
  SurveyObserver observer;

  ASSERT_TRUE(NavigateTo("a.test", "/empty.html"));

  GetHatsService()->LaunchDelayedSurveyForWebContents(
      kTestSurveyTrigger, web_contents(), 42000, {}, {},
      /*navigation_behaviour=*/
      HatsService::NavigationBehaviour::ALLOW_ANY,
      /*success_callback=*/
      base::BindOnce(&SurveyObserver::Accept, observer.GetWeakPtr()),
      /*failure_callback=*/
      base::BindOnce(&SurveyObserver::Dismiss, observer.GetWeakPtr()));

  EXPECT_TRUE(GetHatsService()->HasPendingTasksForTesting());

  ASSERT_TRUE(NavigateTo("b.test", "/empty.html"));

  EXPECT_TRUE(GetHatsService()->HasPendingTasksForTesting());
  EXPECT_FALSE(observer.IsAccepted());
  EXPECT_FALSE(observer.IsDismissed());
}

IN_PROC_BROWSER_TEST_F(HatsServiceAndroidBrowserTest,
                       NavigationBehavior_RequireSameOrigin) {
  SurveyObserver observer;

  ASSERT_TRUE(NavigateTo("a.test", "/empty.html"));

  GetHatsService()->LaunchDelayedSurveyForWebContents(
      kTestSurveyTrigger, web_contents(), 42000, {}, {},
      /*navigation_behaviour=*/
      HatsService::NavigationBehaviour::REQUIRE_SAME_ORIGIN,
      /*success_callback=*/
      base::BindOnce(&SurveyObserver::Accept, observer.GetWeakPtr()),
      /*failure_callback=*/
      base::BindOnce(&SurveyObserver::Dismiss, observer.GetWeakPtr()));

  EXPECT_TRUE(GetHatsService()->HasPendingTasksForTesting());

  ASSERT_TRUE(NavigateTo("a.test", "/empty_script.html"));

  EXPECT_TRUE(GetHatsService()->HasPendingTasksForTesting());
  EXPECT_FALSE(observer.IsAccepted());
  EXPECT_FALSE(observer.IsDismissed());

  ASSERT_TRUE(NavigateTo("b.test", "/empty.html"));

  EXPECT_FALSE(GetHatsService()->HasPendingTasksForTesting());
  EXPECT_FALSE(observer.IsAccepted());
  EXPECT_TRUE(observer.IsDismissed());
}

IN_PROC_BROWSER_TEST_F(HatsServiceAndroidBrowserTest,
                       NavigationBehavior_RequireSameDocument) {
  SurveyObserver observer;

  ASSERT_TRUE(NavigateTo("a.test", "/empty.html"));

  GetHatsService()->LaunchDelayedSurveyForWebContents(
      kTestSurveyTrigger, web_contents(), 42000, {}, {},
      /*navigation_behaviour=*/
      HatsService::NavigationBehaviour::REQUIRE_SAME_DOCUMENT,
      /*success_callback=*/
      base::BindOnce(&SurveyObserver::Accept, observer.GetWeakPtr()),
      /*failure_callback=*/
      base::BindOnce(&SurveyObserver::Dismiss, observer.GetWeakPtr()));

  EXPECT_TRUE(GetHatsService()->HasPendingTasksForTesting());

  // Same-document navigation
  web_contents()->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
      u"document.location='#';", base::NullCallback(),
      content::ISOLATED_WORLD_ID_GLOBAL);
  content::RunAllTasksUntilIdle();

  EXPECT_TRUE(GetHatsService()->HasPendingTasksForTesting());
  EXPECT_FALSE(observer.IsAccepted());
  EXPECT_FALSE(observer.IsDismissed());

  ASSERT_TRUE(NavigateTo("a.test", "/empty_script.html"));

  EXPECT_FALSE(GetHatsService()->HasPendingTasksForTesting());
  EXPECT_FALSE(observer.IsAccepted());
  EXPECT_TRUE(observer.IsDismissed());
}

}  // namespace hats
