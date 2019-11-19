// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/json/string_escape.h"
#include "base/memory/ref_counted_memory.h"
#include "base/optional.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/search_provider_logos/logo_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/search/instant_test_utils.h"
#include "chrome/browser/ui/search/local_ntp_browsertest_base.h"
#include "chrome/browser/ui/search/local_ntp_test_utils.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/search_provider_logos/logo_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

using search_provider_logos::EncodedLogo;
using search_provider_logos::EncodedLogoCallback;
using search_provider_logos::LogoCallbacks;
using search_provider_logos::LogoCallbackReason;
using search_provider_logos::LogoObserver;
using search_provider_logos::LogoService;
using search_provider_logos::LogoType;
using testing::_;
using testing::DoAll;
using testing::Eq;
using testing::IsEmpty;

namespace {

const char kCachedB64[] = "\161\247\041\171\337\276";  // b64decode("cached++")
const char kFreshB64[] = "abc";                        // b64decode("YWJj")
const int kFakeboxTopPx = 56 + 200 + 38;  // top margin + height + bottom margin

#if defined(OS_WIN) || defined(OS_MACOSX)
const char kFreshDarkB64[] = "xyz";  // b64decode("eHl6");
#endif

// A base64 encoding of a tiny but valid gif file.
const char kTinyGifData[] =
    "R0lGODlhAQABAIABAP///wAAACH5BAEKAAEALAAAAAABAAEAAAICTAEAOw==";

scoped_refptr<base::RefCountedString> MakeRefPtr(std::string content) {
  return base::RefCountedString::TakeString(&content);
}

class MockLogoService : public LogoService {
 public:
  MOCK_METHOD1(GetLogoPtr, void(LogoCallbacks* callbacks));

  void GetLogo(LogoCallbacks callbacks) override { GetLogoPtr(&callbacks); }
  void GetLogo(LogoObserver* observer) override { NOTREACHED(); }
};

ACTION_P2(ReturnCachedLogo, reason, logo) {
  if (arg0->on_cached_encoded_logo_available) {
    std::move(arg0->on_cached_encoded_logo_available).Run(reason, logo);
  }
}

ACTION_P2(ReturnFreshLogo, reason, logo) {
  if (arg0->on_fresh_encoded_logo_available) {
    std::move(arg0->on_fresh_encoded_logo_available).Run(reason, logo);
  }
}

}  // namespace

class LocalNTPDoodleTest : public InProcessBrowserTest {
 protected:
  LocalNTPDoodleTest() {}

  MockLogoService* logo_service() {
    return static_cast<MockLogoService*>(
        LogoServiceFactory::GetForProfile(browser()->profile()));
  }

  base::Optional<int> GetDimension(content::WebContents* tab,
                                   const std::string& id,
                                   const std::string& dimension) {
    double value = 0.0;
    if (instant_test_utils::GetDoubleFromJS(
            tab,
            base::StringPrintf(
                "document.getElementById(%s).getBoundingClientRect()[%s]",
                base::GetQuotedJSONString(id).c_str(),
                base::GetQuotedJSONString(dimension).c_str()),
            &value)) {
      return value;
    }
    return base::nullopt;
  }

  void TeardownWindowOpenTest(content::WebContents* tab) {
    ASSERT_TRUE(content::ExecuteScript(tab, "window.open = windowOpenOld"));
  }

  void SetupWindowOpenTest(content::WebContents* tab) {
    ASSERT_TRUE(content::ExecuteScript(tab,
                                       "var windowOpenOld = window.open; "
                                       "window.open = (w) => { openedWindow = "
                                       "w };"));
  }

  base::Optional<std::string> GetWindowOpenURL(content::WebContents* tab) {
    std::string target_url;
    if (instant_test_utils::GetStringFromJS(tab, "openedWindow", &target_url)) {
      return target_url;
    }
    return base::nullopt;
  }

  void TeardownNavigatorTest(content::WebContents* tab) {
    ASSERT_TRUE(content::ExecuteScript(tab, "window.navigator = navigatorOld"));
  }

  void SetupBeaconTest(content::WebContents* tab) {
    ASSERT_TRUE(content::ExecuteScript(tab,
                                       "var navigatorOld = window.navigator; "
                                       "window.navigator = {};"
                                       "window.navigator.sendBeacon = "
                                       "(url) => { sentbeacon = url };"));
  }

  base::Optional<std::string> GetBeaconURL(content::WebContents* tab) {
    std::string target_url;
    if (instant_test_utils::GetStringFromJS(tab, "sentbeacon", &target_url)) {
      return target_url;
    }
    return base::nullopt;
  }

  bool ElementExists(content::WebContents* tab, const std::string& id) {
    return ExecuteBooleanJS(
        tab, base::StringPrintf("!!document.getElementById(%s)",
                                base::GetQuotedJSONString(id).c_str()));
  }

  bool DialogIsOpen(content::WebContents* tab, const std::string& id) {
    return ExecuteBooleanJS(
        tab,
        base::StringPrintf("!!document.getElementById(%s).hasAttribute('open')",
                           base::GetQuotedJSONString(id).c_str()));
  }

  bool ExecuteBooleanJS(content::WebContents* tab, const std::string& js) {
    bool value;
    if (instant_test_utils::GetBoolFromJS(tab, js, &value)) {
      return value;
    }
    return false;
  }

  base::Optional<std::string> GetComputedStyle(content::WebContents* tab,
                                               const std::string& id,
                                               const std::string& css_name) {
    std::string css_value;
    if (instant_test_utils::GetStringFromJS(
            tab,
            base::StringPrintf(
                "getComputedStyle(document.getElementById(%s))[%s]",
                base::GetQuotedJSONString(id).c_str(),
                base::GetQuotedJSONString(css_name).c_str()),
            &css_value)) {
      return css_value;
    }
    return base::nullopt;
  }

  base::Optional<double> GetComputedOpacity(content::WebContents* tab,
                                            const std::string& id) {
    auto css_value = GetComputedStyle(tab, id, "opacity");
    double double_value;
    if ((css_value != base::nullopt) &&
        base::StringToDouble(*css_value, &double_value)) {
      return double_value;
    }
    return base::nullopt;
  }

  base::Optional<std::string> GetComputedDisplay(content::WebContents* tab,
                                                 const std::string& id) {
    return GetComputedStyle(tab, id, "display");
  }

  // Gets $(id)[property]. Coerces to string.
  base::Optional<std::string> GetElementProperty(content::WebContents* tab,
                                                 const std::string& id,
                                                 const std::string& property) {
    std::string value;
    if (instant_test_utils::GetStringFromJS(
            tab,
            base::StringPrintf("document.getElementById(%s)[%s] + ''",
                               base::GetQuotedJSONString(id).c_str(),
                               base::GetQuotedJSONString(property).c_str()),
            &value)) {
      return value;
    }
    return base::nullopt;
  }

  void WaitForFadeIn(content::WebContents* tab, const std::string& id) {
    content::ConsoleObserverDelegate console_observer(tab, "WaitForFadeIn");
    tab->SetDelegate(&console_observer);

    bool result = false;
    if (!instant_test_utils::GetBoolFromJS(
            tab,
            base::StringPrintf(
                R"js(
                  (function(id, message) {
                    var element = document.getElementById(id);
                    var fn = function() {
                      if (element.classList.contains('show-logo') &&
                          (window.getComputedStyle(element).opacity == 1.0)) {
                        console.log(message);
                      } else {
                        element.addEventListener('transitionend', fn);
                      }
                    };
                    fn();
                    return true;
                  })(%s, 'WaitForFadeIn')
                )js",
                base::GetQuotedJSONString(id).c_str()),
            &result) &&
        result) {
      ADD_FAILURE() << "failed to wait for fade-in";
      return;
    }

    console_observer.Wait();
  }

  void WaitForLogoSwap(content::WebContents* tab, const std::string& id) {
    content::ConsoleObserverDelegate console_observer(tab, "WaitForFadeIn");
    tab->SetDelegate(&console_observer);

    bool result = false;
    if (!instant_test_utils::GetBoolFromJS(
            tab,
            base::StringPrintf(
                R"js(
                  (function(id, message) {
                    var element = document.getElementById(id);
                    var fn = function() {
                      if (element.classList.contains('show-logo') &&
                          (window.getComputedStyle(element).opacity == 1.0)) {
                        console.log(message);
                      }
                    };
                    element.addEventListener('transitionend', fn);
                    return true;
                  })(%s, 'WaitForFadeIn')
                )js",
                base::GetQuotedJSONString(id).c_str()),
            &result) &&
        result) {
      ADD_FAILURE() << "failed to wait for fade-in";
      return;
    }

    console_observer.Wait();
  }

  // See enum LogoImpressionType in ntp_user_data_logger.cc.
  static const int kLogoImpressionStatic = 0;
  static const int kLogoImpressionCta = 1;

  // See enum LogoClickType in ntp_user_data_logger.cc.
  static const int kLogoClickCta = 1;

 private:
  void SetUp() override {
    InProcessBrowserTest::SetUp();
  }

  void SetUpInProcessBrowserTestFixture() override {
    will_create_browser_context_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterWillCreateBrowserContextServicesCallbackForTesting(
                base::Bind(
                    &LocalNTPDoodleTest::OnWillCreateBrowserContextServices,
                    base::Unretained(this)));
  }

  static std::unique_ptr<KeyedService> CreateLogoService(
      content::BrowserContext* context) {
    return std::make_unique<MockLogoService>();
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    LogoServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&LocalNTPDoodleTest::CreateLogoService));
  }

  std::unique_ptr<
      base::CallbackList<void(content::BrowserContext*)>::Subscription>
      will_create_browser_context_services_subscription_;
};

IN_PROC_BROWSER_TEST_F(LocalNTPDoodleTest,
                       ShouldBeUnchangedOnLogoFetchCancelled) {
  EXPECT_CALL(*logo_service(), GetLogoPtr(_))
      .WillRepeatedly(
          DoAll(ReturnCachedLogo(LogoCallbackReason::CANCELED, base::nullopt),
                ReturnFreshLogo(LogoCallbackReason::CANCELED, base::nullopt)));

  // Open a new blank tab, then go to NTP and listen for console messages.
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  content::ConsoleObserverDelegate console_observer(active_tab, "*");
  active_tab->SetDelegate(&console_observer);
  base::HistogramTester histograms;
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));

  EXPECT_THAT(GetDimension(active_tab, "fakebox", "top"), Eq(kFakeboxTopPx));
  EXPECT_THAT(GetComputedOpacity(active_tab, "logo-default"), Eq(1.0));
  EXPECT_THAT(GetComputedOpacity(active_tab, "logo-doodle"), Eq(0.0));
  EXPECT_THAT(console_observer.message(), IsEmpty());

  histograms.ExpectTotalCount("NewTabPage.LogoShown", 0);
  histograms.ExpectTotalCount("NewTabPage.LogoShown.FromCache", 0);
  histograms.ExpectTotalCount("NewTabPage.LogoShown.Fresh", 0);
  histograms.ExpectTotalCount("NewTabPage.LogoShownTime2", 0);
}

IN_PROC_BROWSER_TEST_F(LocalNTPDoodleTest,
                       ShouldBeUnchangedWhenNoCachedOrFreshDoodle) {
  EXPECT_CALL(*logo_service(), GetLogoPtr(_))
      .WillRepeatedly(DoAll(
          ReturnCachedLogo(LogoCallbackReason::DETERMINED, base::nullopt),
          ReturnFreshLogo(LogoCallbackReason::REVALIDATED, base::nullopt)));

  // Open a new blank tab, then go to NTP and listen for console messages.
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  content::ConsoleObserverDelegate console_observer(active_tab, "*");
  active_tab->SetDelegate(&console_observer);
  base::HistogramTester histograms;
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));

  EXPECT_THAT(GetDimension(active_tab, "fakebox", "top"), Eq(kFakeboxTopPx));
  EXPECT_THAT(GetComputedOpacity(active_tab, "logo-default"), Eq(1.0));
  EXPECT_THAT(GetComputedOpacity(active_tab, "logo-doodle"), Eq(0.0));
  EXPECT_THAT(console_observer.message(), IsEmpty());

  histograms.ExpectTotalCount("NewTabPage.LogoShown", 0);
  histograms.ExpectTotalCount("NewTabPage.LogoShown.FromCache", 0);
  histograms.ExpectTotalCount("NewTabPage.LogoShown.Fresh", 0);
  histograms.ExpectTotalCount("NewTabPage.LogoShownTime2", 0);
}

IN_PROC_BROWSER_TEST_F(LocalNTPDoodleTest, ShouldShowDoodleWhenCached) {
  EncodedLogo cached_logo;
  cached_logo.encoded_image = MakeRefPtr(kCachedB64);
  cached_logo.metadata.mime_type = "image/png";
  cached_logo.metadata.on_click_url = GURL("https://www.chromium.org/");
  cached_logo.metadata.alt_text = "Chromium";

  EXPECT_CALL(*logo_service(), GetLogoPtr(_))
      .WillRepeatedly(DoAll(
          ReturnCachedLogo(LogoCallbackReason::DETERMINED, cached_logo),
          ReturnFreshLogo(LogoCallbackReason::REVALIDATED, base::nullopt)));

  // Open a new blank tab, then go to NTP and listen for console messages.
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  content::ConsoleObserverDelegate console_observer(active_tab, "*");
  active_tab->SetDelegate(&console_observer);
  base::HistogramTester histograms;
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));

  EXPECT_THAT(GetDimension(active_tab, "fakebox", "top"), Eq(kFakeboxTopPx));
  EXPECT_THAT(GetComputedOpacity(active_tab, "logo-default"), Eq(0.0));
  EXPECT_THAT(GetComputedOpacity(active_tab, "logo-doodle"), Eq(1.0));
  EXPECT_THAT(GetComputedDisplay(active_tab, "logo-doodle-container"),
              Eq<std::string>("inline-block"));
  EXPECT_THAT(GetComputedDisplay(active_tab, "logo-doodle-iframe"),
              Eq<std::string>("none"));
  EXPECT_THAT(GetElementProperty(active_tab, "logo-doodle-image", "title"),
              Eq<std::string>("Chromium"));
  // TODO(sfiera): check href by clicking on button.
  EXPECT_THAT(console_observer.message(), IsEmpty());

  histograms.ExpectTotalCount("NewTabPage.LogoShown", 1);
  histograms.ExpectBucketCount("NewTabPage.LogoShown", kLogoImpressionStatic,
                               1);
  histograms.ExpectTotalCount("NewTabPage.LogoShown.FromCache", 1);
  histograms.ExpectBucketCount("NewTabPage.LogoShown.FromCache",
                               kLogoImpressionStatic, 1);
  histograms.ExpectTotalCount("NewTabPage.LogoShown.Fresh", 0);
  histograms.ExpectTotalCount("NewTabPage.LogoShownTime2", 1);
}

IN_PROC_BROWSER_TEST_F(LocalNTPDoodleTest, ShouldShowInteractiveLogo) {
  EncodedLogo cached_logo;
  cached_logo.encoded_image = MakeRefPtr(std::string());
  cached_logo.metadata.mime_type = "image/png";
  cached_logo.metadata.type = LogoType::INTERACTIVE;
  cached_logo.metadata.full_page_url =
      GURL("https://www.chromium.org/interactive");
  cached_logo.metadata.alt_text = "alt text";
  cached_logo.metadata.iframe_width_px = 500;
  cached_logo.metadata.iframe_height_px = 200;

  EXPECT_CALL(*logo_service(), GetLogoPtr(_))
      .WillRepeatedly(DoAll(
          ReturnCachedLogo(LogoCallbackReason::DETERMINED, cached_logo),
          ReturnFreshLogo(LogoCallbackReason::REVALIDATED, base::nullopt)));

  // Open a new blank tab, then go to NTP.
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  base::HistogramTester histograms;
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));

  EXPECT_THAT(GetDimension(active_tab, "fakebox", "top"), Eq(kFakeboxTopPx));
  EXPECT_THAT(GetComputedOpacity(active_tab, "logo-default"), Eq(0.0));
  EXPECT_THAT(GetComputedOpacity(active_tab, "logo-doodle"), Eq(1.0));
  EXPECT_THAT(GetComputedDisplay(active_tab, "logo-doodle-container"),
              Eq<std::string>("none"));
  EXPECT_THAT(GetComputedDisplay(active_tab, "logo-doodle-iframe"),
              Eq<std::string>("block"));

  EXPECT_THAT(GetElementProperty(active_tab, "logo-doodle-iframe", "src"),
              Eq<std::string>("https://www.chromium.org/interactive"));
  EXPECT_THAT(GetElementProperty(active_tab, "logo-doodle-iframe", "title"),
              Eq<std::string>("alt text"));
}

IN_PROC_BROWSER_TEST_F(LocalNTPDoodleTest,
                       ShouldShowInteractiveLogoWithoutImage) {
  EncodedLogo cached_logo;
  cached_logo.encoded_image = nullptr;
  cached_logo.metadata.type = LogoType::INTERACTIVE;
  cached_logo.metadata.full_page_url =
      GURL("https://www.chromium.org/interactive");
  cached_logo.metadata.alt_text = "alt text";
  cached_logo.metadata.iframe_width_px = 500;
  cached_logo.metadata.iframe_height_px = 200;

  EXPECT_CALL(*logo_service(), GetLogoPtr(_))
      .WillRepeatedly(DoAll(
          ReturnCachedLogo(LogoCallbackReason::DETERMINED, cached_logo),
          ReturnFreshLogo(LogoCallbackReason::REVALIDATED, base::nullopt)));

  // Open a new blank tab, then go to NTP.
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  base::HistogramTester histograms;
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));

  EXPECT_THAT(GetDimension(active_tab, "fakebox", "top"), Eq(kFakeboxTopPx));
  EXPECT_THAT(GetComputedOpacity(active_tab, "logo-default"), Eq(0.0));
  EXPECT_THAT(GetComputedOpacity(active_tab, "logo-doodle"), Eq(1.0));
  EXPECT_THAT(GetComputedDisplay(active_tab, "logo-doodle-container"),
              Eq<std::string>("none"));
  EXPECT_THAT(GetComputedDisplay(active_tab, "logo-doodle-iframe"),
              Eq<std::string>("block"));

  EXPECT_THAT(GetElementProperty(active_tab, "logo-doodle-iframe", "src"),
              Eq<std::string>("https://www.chromium.org/interactive"));
  EXPECT_THAT(GetElementProperty(active_tab, "logo-doodle-iframe", "title"),
              Eq<std::string>("alt text"));
}

IN_PROC_BROWSER_TEST_F(LocalNTPDoodleTest,
                       ShouldFadeSimpleDoodleToDefaultWhenFetched) {
  EncodedLogo cached_logo;
  cached_logo.encoded_image = MakeRefPtr(kCachedB64);
  cached_logo.metadata.mime_type = "image/png";
  cached_logo.metadata.on_click_url = GURL("https://www.chromium.org/");
  cached_logo.metadata.alt_text = "Chromium";

  EXPECT_CALL(*logo_service(), GetLogoPtr(_))
      .WillOnce(
          DoAll(ReturnCachedLogo(LogoCallbackReason::DETERMINED, cached_logo),
                ReturnFreshLogo(LogoCallbackReason::DETERMINED, base::nullopt)))
      .WillRepeatedly(DoAll(
          ReturnCachedLogo(LogoCallbackReason::DETERMINED, base::nullopt),
          ReturnFreshLogo(LogoCallbackReason::REVALIDATED, base::nullopt)));

  // Open a new blank tab, then go to NTP.
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  base::HistogramTester histograms;
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));

  WaitForFadeIn(active_tab, "logo-default");
  EXPECT_THAT(GetDimension(active_tab, "fakebox", "top"), Eq(kFakeboxTopPx));
  EXPECT_THAT(GetComputedOpacity(active_tab, "logo-default"), Eq(1.0));
  EXPECT_THAT(GetComputedOpacity(active_tab, "logo-doodle"), Eq(0.0));

  histograms.ExpectTotalCount("NewTabPage.LogoShown", 1);
  histograms.ExpectBucketCount("NewTabPage.LogoShown", kLogoImpressionStatic,
                               1);
  histograms.ExpectTotalCount("NewTabPage.LogoShown.FromCache", 1);
  histograms.ExpectBucketCount("NewTabPage.LogoShown.FromCache",
                               kLogoImpressionStatic, 1);
  histograms.ExpectTotalCount("NewTabPage.LogoShown.Fresh", 0);
  histograms.ExpectTotalCount("NewTabPage.LogoShownTime2", 1);
}

IN_PROC_BROWSER_TEST_F(LocalNTPDoodleTest,
                       ShouldFadeDefaultToSimpleDoodleWhenFetched) {
  EncodedLogo fresh_logo;
  fresh_logo.encoded_image = MakeRefPtr(kFreshB64);
  fresh_logo.metadata.mime_type = "image/png";
  fresh_logo.metadata.on_click_url = GURL("https://www.chromium.org/");
  fresh_logo.metadata.alt_text = "Chromium";

  EXPECT_CALL(*logo_service(), GetLogoPtr(_))
      .WillOnce(
          DoAll(ReturnCachedLogo(LogoCallbackReason::DETERMINED, base::nullopt),
                ReturnFreshLogo(LogoCallbackReason::DETERMINED, fresh_logo)))
      .WillRepeatedly(DoAll(
          ReturnCachedLogo(LogoCallbackReason::DETERMINED, fresh_logo),
          ReturnFreshLogo(LogoCallbackReason::REVALIDATED, base::nullopt)));

  // Open a new blank tab, then go to NTP.
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  base::HistogramTester histograms;
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));

  WaitForFadeIn(active_tab, "logo-doodle");
  EXPECT_THAT(GetDimension(active_tab, "fakebox", "top"), Eq(kFakeboxTopPx));
  EXPECT_THAT(GetComputedOpacity(active_tab, "logo-default"), Eq(0.0));
  EXPECT_THAT(GetComputedOpacity(active_tab, "logo-doodle"), Eq(1.0));
  EXPECT_THAT(GetComputedDisplay(active_tab, "logo-doodle-container"),
              Eq<std::string>("inline-block"));
  EXPECT_THAT(GetComputedDisplay(active_tab, "logo-doodle-iframe"),
              Eq<std::string>("none"));
  EXPECT_THAT(GetElementProperty(active_tab, "logo-doodle-image", "title"),
              Eq<std::string>("Chromium"));
  // TODO(sfiera): check href by clicking on button.

  histograms.ExpectTotalCount("NewTabPage.LogoShown", 1);
  histograms.ExpectBucketCount("NewTabPage.LogoShown", kLogoImpressionStatic,
                               1);
  histograms.ExpectTotalCount("NewTabPage.LogoShown.FromCache", 0);
  histograms.ExpectTotalCount("NewTabPage.LogoShown.Fresh", 1);
  histograms.ExpectBucketCount("NewTabPage.LogoShown.Fresh",
                               kLogoImpressionStatic, 1);
  histograms.ExpectTotalCount("NewTabPage.LogoShownTime2", 1);
}

IN_PROC_BROWSER_TEST_F(LocalNTPDoodleTest,
                       ShouldFadeDefaultToInteractiveDoodleWhenFetched) {
  EncodedLogo fresh_logo;
  fresh_logo.encoded_image = MakeRefPtr(std::string());
  fresh_logo.metadata.mime_type = "image/png";
  fresh_logo.metadata.type = LogoType::INTERACTIVE;
  fresh_logo.metadata.full_page_url =
      GURL("https://www.chromium.org/interactive");
  fresh_logo.metadata.alt_text = "alt text";
  fresh_logo.metadata.iframe_width_px = 500;
  fresh_logo.metadata.iframe_height_px = 200;

  EXPECT_CALL(*logo_service(), GetLogoPtr(_))
      .WillOnce(
          DoAll(ReturnCachedLogo(LogoCallbackReason::DETERMINED, base::nullopt),
                ReturnFreshLogo(LogoCallbackReason::DETERMINED, fresh_logo)))
      .WillRepeatedly(DoAll(
          ReturnCachedLogo(LogoCallbackReason::DETERMINED, fresh_logo),
          ReturnFreshLogo(LogoCallbackReason::REVALIDATED, base::nullopt)));

  // Open a new blank tab, then go to NTP.
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));

  WaitForFadeIn(active_tab, "logo-doodle");
  EXPECT_THAT(GetDimension(active_tab, "fakebox", "top"), Eq(kFakeboxTopPx));
  EXPECT_THAT(GetComputedOpacity(active_tab, "logo-default"), Eq(0.0));
  EXPECT_THAT(GetComputedOpacity(active_tab, "logo-doodle"), Eq(1.0));
  EXPECT_THAT(GetComputedDisplay(active_tab, "logo-doodle-container"),
              Eq<std::string>("none"));
  EXPECT_THAT(GetComputedDisplay(active_tab, "logo-doodle-iframe"),
              Eq<std::string>("block"));
  EXPECT_THAT(GetElementProperty(active_tab, "logo-doodle-iframe", "src"),
              Eq<std::string>("https://www.chromium.org/interactive"));
}

IN_PROC_BROWSER_TEST_F(LocalNTPDoodleTest, ShouldNotFadeFromInteractiveDoodle) {
  EncodedLogo cached_logo;
  cached_logo.encoded_image = MakeRefPtr(std::string());
  cached_logo.metadata.mime_type = "image/png";
  cached_logo.metadata.type = LogoType::INTERACTIVE;
  cached_logo.metadata.full_page_url =
      GURL("https://www.chromium.org/interactive");
  cached_logo.metadata.alt_text = "alt text";
  cached_logo.metadata.iframe_width_px = 500;
  cached_logo.metadata.iframe_height_px = 200;

  EXPECT_CALL(*logo_service(), GetLogoPtr(_))
      .WillOnce(
          DoAll(ReturnCachedLogo(LogoCallbackReason::DETERMINED, cached_logo),
                ReturnFreshLogo(LogoCallbackReason::DETERMINED, base::nullopt)))
      .WillRepeatedly(DoAll(
          ReturnCachedLogo(LogoCallbackReason::DETERMINED, base::nullopt),
          ReturnFreshLogo(LogoCallbackReason::REVALIDATED, base::nullopt)));

  // Open a new blank tab, then go to NTP.
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));

  EXPECT_THAT(GetDimension(active_tab, "fakebox", "top"), Eq(kFakeboxTopPx));
  EXPECT_THAT(GetComputedOpacity(active_tab, "logo-default"), Eq(0.0));
  EXPECT_THAT(GetComputedOpacity(active_tab, "logo-doodle"), Eq(1.0));
  EXPECT_THAT(GetComputedDisplay(active_tab, "logo-doodle-container"),
              Eq<std::string>("none"));
  EXPECT_THAT(GetComputedDisplay(active_tab, "logo-doodle-iframe"),
              Eq<std::string>("block"));
  EXPECT_THAT(GetElementProperty(active_tab, "logo-doodle-iframe", "src"),
              Eq<std::string>("https://www.chromium.org/interactive"));
}

IN_PROC_BROWSER_TEST_F(LocalNTPDoodleTest,
                       ShouldFadeSimpleDoodleToSimpleDoodleWhenFetched) {
  EncodedLogo cached_logo;
  cached_logo.encoded_image = MakeRefPtr(kCachedB64);
  cached_logo.metadata.mime_type = "image/png";
  cached_logo.metadata.on_click_url = GURL("https://www.chromium.org/cached");
  cached_logo.metadata.alt_text = "cached alt text";

  EncodedLogo fresh_logo;
  fresh_logo.encoded_image = MakeRefPtr(kFreshB64);
  fresh_logo.metadata.mime_type = "image/png";
  fresh_logo.metadata.on_click_url = GURL("https://www.chromium.org/fresh");
  fresh_logo.metadata.alt_text = "fresh alt text";

  EXPECT_CALL(*logo_service(), GetLogoPtr(_))
      .WillOnce(
          DoAll(ReturnCachedLogo(LogoCallbackReason::DETERMINED, cached_logo),
                ReturnFreshLogo(LogoCallbackReason::DETERMINED, fresh_logo)))
      .WillRepeatedly(DoAll(
          ReturnCachedLogo(LogoCallbackReason::DETERMINED, fresh_logo),
          ReturnFreshLogo(LogoCallbackReason::REVALIDATED, base::nullopt)));

  // Open a new blank tab, then go to NTP.
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  base::HistogramTester histograms;
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));

  WaitForFadeIn(active_tab, "logo-doodle");
  EXPECT_THAT(GetDimension(active_tab, "fakebox", "top"), Eq(kFakeboxTopPx));
  EXPECT_THAT(GetComputedOpacity(active_tab, "logo-default"), Eq(0.0));
  EXPECT_THAT(GetComputedOpacity(active_tab, "logo-doodle"), Eq(1.0));
  EXPECT_THAT(GetComputedDisplay(active_tab, "logo-doodle-container"),
              Eq<std::string>("inline-block"));
  EXPECT_THAT(GetComputedDisplay(active_tab, "logo-doodle-iframe"),
              Eq<std::string>("none"));
  EXPECT_THAT(GetElementProperty(active_tab, "logo-doodle-image", "src"),
              Eq<std::string>("data:image/png;base64,YWJj"));
  EXPECT_THAT(GetElementProperty(active_tab, "logo-doodle-image", "title"),
              Eq<std::string>("fresh alt text"));
  // TODO(sfiera): check href by clicking on button.

  // LogoShown is recorded for both cached and fresh Doodle, but LogoShownTime2
  // is only recorded once per NTP.
  histograms.ExpectTotalCount("NewTabPage.LogoShown", 2);
  histograms.ExpectBucketCount("NewTabPage.LogoShown", kLogoImpressionStatic,
                               2);
  histograms.ExpectTotalCount("NewTabPage.LogoShown.FromCache", 1);
  histograms.ExpectBucketCount("NewTabPage.LogoShown.FromCache",
                               kLogoImpressionStatic, 1);
  histograms.ExpectTotalCount("NewTabPage.LogoShown.Fresh", 1);
  histograms.ExpectBucketCount("NewTabPage.LogoShown.Fresh",
                               kLogoImpressionStatic, 1);
  histograms.ExpectTotalCount("NewTabPage.LogoShownTime2", 1);
}

IN_PROC_BROWSER_TEST_F(LocalNTPDoodleTest, ShouldUpdateMetadataWhenChanged) {
  EncodedLogo cached_logo;
  cached_logo.encoded_image = MakeRefPtr(kCachedB64);
  cached_logo.metadata.mime_type = "image/png";
  cached_logo.metadata.on_click_url = GURL("https://www.chromium.org/cached");
  cached_logo.metadata.alt_text = "cached alt text";

  EncodedLogo fresh_logo;
  fresh_logo.encoded_image = cached_logo.encoded_image;
  fresh_logo.metadata.mime_type = cached_logo.metadata.mime_type;
  fresh_logo.metadata.on_click_url = GURL("https://www.chromium.org/fresh");
  fresh_logo.metadata.alt_text = "fresh alt text";

  EXPECT_CALL(*logo_service(), GetLogoPtr(_))
      .WillOnce(
          DoAll(ReturnCachedLogo(LogoCallbackReason::DETERMINED, cached_logo),
                ReturnFreshLogo(LogoCallbackReason::DETERMINED, fresh_logo)))
      .WillRepeatedly(DoAll(
          ReturnCachedLogo(LogoCallbackReason::DETERMINED, fresh_logo),
          ReturnFreshLogo(LogoCallbackReason::REVALIDATED, base::nullopt)));

  // Open a new blank tab, then go to NTP.
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  base::HistogramTester histograms;
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));

  EXPECT_THAT(GetDimension(active_tab, "fakebox", "top"), Eq(kFakeboxTopPx));
  EXPECT_THAT(GetComputedOpacity(active_tab, "logo-default"), Eq(0.0));
  EXPECT_THAT(GetComputedOpacity(active_tab, "logo-doodle"), Eq(1.0));
  EXPECT_THAT(GetComputedDisplay(active_tab, "logo-doodle-container"),
              Eq<std::string>("inline-block"));
  EXPECT_THAT(GetComputedDisplay(active_tab, "logo-doodle-iframe"),
              Eq<std::string>("none"));

  EXPECT_THAT(GetElementProperty(active_tab, "logo-doodle-image", "title"),
              Eq<std::string>("fresh alt text"));
  // TODO(sfiera): check href by clicking on button.

  // Metadata update does not count as a new impression.
  histograms.ExpectTotalCount("NewTabPage.LogoShown", 1);
  histograms.ExpectBucketCount("NewTabPage.LogoShown", kLogoImpressionStatic,
                               1);
  histograms.ExpectTotalCount("NewTabPage.LogoShown.FromCache", 1);
  histograms.ExpectBucketCount("NewTabPage.LogoShown.FromCache",
                               kLogoImpressionStatic, 1);
  histograms.ExpectTotalCount("NewTabPage.LogoShown.Fresh", 0);
  histograms.ExpectTotalCount("NewTabPage.LogoShownTime2", 1);
}

IN_PROC_BROWSER_TEST_F(LocalNTPDoodleTest, ShouldAppendShareButtonWhenCached) {
  EncodedLogo cached_logo;
  cached_logo.encoded_image = MakeRefPtr(kCachedB64);
  cached_logo.metadata.mime_type = "image/png";
  cached_logo.metadata.on_click_url = GURL("https://www.chromium.org/");
  cached_logo.metadata.alt_text = "Chromium";
  cached_logo.metadata.short_link = GURL("https://g.co");
  cached_logo.metadata.share_button_x = 12;
  cached_logo.metadata.share_button_y = 39;
  cached_logo.metadata.share_button_opacity = 0.8;
  cached_logo.metadata.share_button_icon = "sbimg";
  cached_logo.metadata.share_button_bg = "#ffff00";

  EXPECT_CALL(*logo_service(), GetLogoPtr(_))
      .WillRepeatedly(DoAll(
          ReturnCachedLogo(LogoCallbackReason::DETERMINED, cached_logo),
          ReturnFreshLogo(LogoCallbackReason::REVALIDATED, base::nullopt)));

  // Open a new blank tab, then go to NTP and listen for console messages.
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  content::ConsoleObserverDelegate console_observer(active_tab, "*");
  active_tab->SetDelegate(&console_observer);
  base::HistogramTester histograms;
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));

  EXPECT_TRUE(ElementExists(active_tab, "ddlsb"));
  EXPECT_TRUE(ElementExists(active_tab, "ddlsb-img"));
  EXPECT_THAT(GetComputedStyle(active_tab, "ddlsb", "left"),
              Eq<std::string>("12px"));
  EXPECT_THAT(GetComputedStyle(active_tab, "ddlsb", "top"),
              Eq<std::string>("39px"));
  EXPECT_THAT(GetComputedStyle(active_tab, "ddlsb", "background-color"),
              Eq<std::string>("rgba(255, 255, 0, 0.8)"));
  EXPECT_THAT(GetElementProperty(active_tab, "ddlsb-img", "src"),
              Eq<std::string>("data:image/png;base64,sbimg"));
}

IN_PROC_BROWSER_TEST_F(LocalNTPDoodleTest,
                       ShouldNotAppendShareButtonWhenCacheEmpty) {
  EncodedLogo cached_logo;
  cached_logo.encoded_image = MakeRefPtr(kCachedB64);
  cached_logo.metadata.mime_type = "image/png";
  cached_logo.metadata.on_click_url = GURL("https://www.chromium.org/");
  cached_logo.metadata.alt_text = "Chromium";
  cached_logo.metadata.short_link = GURL("https://g.co");
  cached_logo.metadata.share_button_x = -1;
  cached_logo.metadata.share_button_y = -1;
  cached_logo.metadata.share_button_opacity = 0;
  cached_logo.metadata.share_button_icon = "";
  cached_logo.metadata.share_button_bg = "";

  EXPECT_CALL(*logo_service(), GetLogoPtr(_))
      .WillRepeatedly(DoAll(
          ReturnCachedLogo(LogoCallbackReason::DETERMINED, cached_logo),
          ReturnFreshLogo(LogoCallbackReason::REVALIDATED, base::nullopt)));

  // Open a new blank tab, then go to NTP and listen for console messages.
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  content::ConsoleObserverDelegate console_observer(active_tab, "*");
  active_tab->SetDelegate(&console_observer);
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));

  EXPECT_FALSE(ElementExists(active_tab, "ddlsb"));
  EXPECT_FALSE(ElementExists(active_tab, "ddlsb-img"));
}

IN_PROC_BROWSER_TEST_F(LocalNTPDoodleTest,
                       ShouldNotAppendShareButtonWhenCacheIncomplete) {
  EncodedLogo cached_logo;
  cached_logo.encoded_image = MakeRefPtr(kCachedB64);
  cached_logo.metadata.mime_type = "image/png";
  cached_logo.metadata.on_click_url = GURL("https://www.chromium.org/");
  cached_logo.metadata.alt_text = "Chromium";
  cached_logo.metadata.short_link = GURL("https://g.co");
  cached_logo.metadata.share_button_x = 12;
  cached_logo.metadata.share_button_y = 36;
  cached_logo.metadata.share_button_opacity = 0.8;
  cached_logo.metadata.share_button_icon = "";
  cached_logo.metadata.share_button_bg = "#ffff00";

  EXPECT_CALL(*logo_service(), GetLogoPtr(_))
      .WillRepeatedly(DoAll(
          ReturnCachedLogo(LogoCallbackReason::DETERMINED, cached_logo),
          ReturnFreshLogo(LogoCallbackReason::REVALIDATED, base::nullopt)));

  // Open a new blank tab, then go to NTP and listen for console messages.
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  content::ConsoleObserverDelegate console_observer(active_tab, "*");
  active_tab->SetDelegate(&console_observer);
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));

  EXPECT_FALSE(ElementExists(active_tab, "ddlsb"));
  EXPECT_FALSE(ElementExists(active_tab, "ddlsb-img"));
}

IN_PROC_BROWSER_TEST_F(LocalNTPDoodleTest,
                       ShouldShowShareDialogWhenShareButtonClicked) {
  EncodedLogo cached_logo;
  cached_logo.encoded_image = MakeRefPtr(kCachedB64);
  cached_logo.metadata.mime_type = "image/png";
  cached_logo.metadata.on_click_url = GURL("https://www.chromium.org/");
  cached_logo.metadata.alt_text = "Chromium";
  cached_logo.metadata.short_link = GURL("https://g.co");
  cached_logo.metadata.share_button_x = 12;
  cached_logo.metadata.share_button_y = 36;
  cached_logo.metadata.share_button_opacity = 0.8;
  cached_logo.metadata.share_button_icon = "sbimg";
  cached_logo.metadata.share_button_bg = "#ffff00";

  EXPECT_CALL(*logo_service(), GetLogoPtr(_))
      .WillRepeatedly(DoAll(
          ReturnCachedLogo(LogoCallbackReason::DETERMINED, cached_logo),
          ReturnFreshLogo(LogoCallbackReason::REVALIDATED, base::nullopt)));

  // Open a new blank tab, then go to NTP and listen for console messages.
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  content::ConsoleObserverDelegate console_observer(active_tab, "*");
  active_tab->SetDelegate(&console_observer);
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));

  EXPECT_FALSE(DialogIsOpen(active_tab, "ddlsd"));
  ASSERT_TRUE(content::ExecuteScript(
      active_tab, "document.getElementById('ddlsb').click();"));
  EXPECT_TRUE(DialogIsOpen(active_tab, "ddlsd"));

  // Check title
  std::string title;
  ASSERT_TRUE(instant_test_utils::GetStringFromJS(
      active_tab, "document.getElementById('ddlsd-title').innerHTML", &title));
  EXPECT_THAT(title, Eq<std::string>(cached_logo.metadata.alt_text));

  // Check share link inside textbox
  std::string link;
  ASSERT_TRUE(instant_test_utils::GetStringFromJS(
      active_tab, "document.getElementById('ddlsd-text').value", &link));
  EXPECT_THAT(link, Eq<std::string>("https://g.co/"));

  ASSERT_TRUE(content::ExecuteScript(
      active_tab, "document.getElementById('ddlsd-close').click();"));
  EXPECT_FALSE(DialogIsOpen(active_tab, "ddlsd"));
}

IN_PROC_BROWSER_TEST_F(LocalNTPDoodleTest, ShouldOpenFacebookInShareDialog) {
  EncodedLogo cached_logo;
  cached_logo.encoded_image = MakeRefPtr(kCachedB64);
  cached_logo.metadata.mime_type = "image/png";
  cached_logo.metadata.on_click_url = GURL("https://www.chromium.org/");
  cached_logo.metadata.alt_text = "Chromium";
  cached_logo.metadata.short_link = GURL("https://g.co");
  cached_logo.metadata.share_button_x = 12;
  cached_logo.metadata.share_button_y = 36;
  cached_logo.metadata.share_button_opacity = 0.8;
  cached_logo.metadata.share_button_icon = "sbimg";
  cached_logo.metadata.share_button_bg = "#ffff00";

  EXPECT_CALL(*logo_service(), GetLogoPtr(_))
      .WillRepeatedly(DoAll(
          ReturnCachedLogo(LogoCallbackReason::DETERMINED, cached_logo),
          ReturnFreshLogo(LogoCallbackReason::REVALIDATED, base::nullopt)));

  // Open a new blank tab, then go to NTP and listen for console messages.
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));

  ASSERT_TRUE(content::ExecuteScript(
      active_tab, "document.getElementById('ddlsb').click();"));
  EXPECT_TRUE(DialogIsOpen(active_tab, "ddlsd"));
  SetupWindowOpenTest(active_tab);
  ASSERT_TRUE(content::ExecuteScript(
      active_tab, "document.getElementById('ddlsd-fbb').click();"));
  EXPECT_THAT(GetWindowOpenURL(active_tab),
              Eq<std::string>(
                  "https://www.facebook.com/dialog/share?app_id=738026486351791"
                  "&href=https%3A%2F%2Fg.co%2F&hashtag=%23GoogleDoodle"));
}

IN_PROC_BROWSER_TEST_F(LocalNTPDoodleTest, ShouldOpenTwitterInShareDialog) {
  EncodedLogo cached_logo;
  cached_logo.encoded_image = MakeRefPtr(kCachedB64);
  cached_logo.metadata.mime_type = "image/png";
  cached_logo.metadata.on_click_url = GURL("https://www.chromium.org/");
  cached_logo.metadata.alt_text = "Chromium";
  cached_logo.metadata.short_link = GURL("https://g.co");
  cached_logo.metadata.share_button_x = 12;
  cached_logo.metadata.share_button_y = 36;
  cached_logo.metadata.share_button_opacity = 0.8;
  cached_logo.metadata.share_button_icon = "sbimg";
  cached_logo.metadata.share_button_bg = "#ffff00";

  EXPECT_CALL(*logo_service(), GetLogoPtr(_))
      .WillRepeatedly(DoAll(
          ReturnCachedLogo(LogoCallbackReason::DETERMINED, cached_logo),
          ReturnFreshLogo(LogoCallbackReason::REVALIDATED, base::nullopt)));

  // Open a new blank tab, then go to NTP and listen for console messages.
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));

  ASSERT_TRUE(content::ExecuteScript(
      active_tab, "document.getElementById('ddlsb').click();"));
  EXPECT_TRUE(DialogIsOpen(active_tab, "ddlsd"));
  SetupWindowOpenTest(active_tab);
  ASSERT_TRUE(content::ExecuteScript(
      active_tab, "document.getElementById('ddlsd-twb').click();"));
  EXPECT_THAT(GetWindowOpenURL(active_tab),
              Eq<std::string>("https://twitter.com/intent/tweet"
                              "?text=Chromium%0Ahttps%3A%2F%2Fg.co%2F"));
}

IN_PROC_BROWSER_TEST_F(LocalNTPDoodleTest, ShouldCopyLinkInShareDialog) {
  EncodedLogo cached_logo;
  cached_logo.encoded_image = MakeRefPtr(kCachedB64);
  cached_logo.metadata.mime_type = "image/png";
  cached_logo.metadata.on_click_url = GURL("https://www.chromium.org/");
  cached_logo.metadata.alt_text = "Chromium";
  cached_logo.metadata.short_link = GURL("https://g.co");
  cached_logo.metadata.share_button_x = 12;
  cached_logo.metadata.share_button_y = 36;
  cached_logo.metadata.share_button_opacity = 0.8;
  cached_logo.metadata.share_button_icon = "sbimg";
  cached_logo.metadata.share_button_bg = "#ffff00";

  EXPECT_CALL(*logo_service(), GetLogoPtr(_))
      .WillRepeatedly(DoAll(
          ReturnCachedLogo(LogoCallbackReason::DETERMINED, cached_logo),
          ReturnFreshLogo(LogoCallbackReason::REVALIDATED, base::nullopt)));

  // Open a new blank tab, then go to NTP and listen for console messages.
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));

  ASSERT_TRUE(content::ExecuteScript(
      active_tab, "document.getElementById('ddlsb').click();"));
  EXPECT_TRUE(DialogIsOpen(active_tab, "ddlsd"));

  ASSERT_TRUE(content::ExecuteScript(
      active_tab, "document.getElementById('ddlsd-copy').click();"));
  std::string short_link;
  ASSERT_TRUE(instant_test_utils::GetStringFromJS(
      active_tab, "window.getSelection().toString()", &short_link));
  EXPECT_EQ("https://g.co/", short_link);
}

IN_PROC_BROWSER_TEST_F(LocalNTPDoodleTest, ShouldLogShareClicksNoEventId) {
  EncodedLogo cached_logo;
  cached_logo.encoded_image = MakeRefPtr(kCachedB64);
  cached_logo.metadata.mime_type = "image/png";
  cached_logo.metadata.on_click_url =
      GURL("https://www.chrotmium.org/?ct=test");
  cached_logo.metadata.alt_text = "Chromium";
  cached_logo.metadata.short_link = GURL("https://g.co");
  cached_logo.metadata.share_button_x = 12;
  cached_logo.metadata.share_button_y = 36;
  cached_logo.metadata.share_button_opacity = 0.8;
  cached_logo.metadata.share_button_icon = "sbimg";
  cached_logo.metadata.share_button_bg = "#ffff00";

  EXPECT_CALL(*logo_service(), GetLogoPtr(_))
      .WillRepeatedly(DoAll(
          ReturnCachedLogo(LogoCallbackReason::DETERMINED, cached_logo),
          ReturnFreshLogo(LogoCallbackReason::REVALIDATED, base::nullopt)));

  // Open a new blank tab, then go to NTP and listen for console messages.
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));

  // Replace window.open so we stay in the same tab.
  SetupWindowOpenTest(active_tab);
  SetupBeaconTest(active_tab);

  ASSERT_TRUE(content::ExecuteScript(
      active_tab, "document.getElementById('ddlsb').click();"));

  ASSERT_TRUE(content::ExecuteScript(
      active_tab, "document.getElementById('ddlsd-fbb').click();"));
  EXPECT_THAT(
      GetBeaconURL(active_tab),
      Eq<std::string>("https://www.google.com/gen_204"
                      "?atyp=i&ct=doodle&cad=sh%2C2%2Cct%3Atest&ntp=1"));

  ASSERT_TRUE(content::ExecuteScript(
      active_tab, "document.getElementById('ddlsd-twb').click();"));
  EXPECT_THAT(
      GetBeaconURL(active_tab),
      Eq<std::string>("https://www.google.com/gen_204"
                      "?atyp=i&ct=doodle&cad=sh%2C3%2Cct%3Atest&ntp=1"));

  ASSERT_TRUE(content::ExecuteScript(
      active_tab, "document.getElementById('ddlsd-emb').click();"));
  EXPECT_THAT(
      GetBeaconURL(active_tab),
      Eq<std::string>("https://www.google.com/gen_204"
                      "?atyp=i&ct=doodle&cad=sh%2C5%2Cct%3Atest&ntp=1"));

  ASSERT_TRUE(content::ExecuteScript(
      active_tab, "document.getElementById('ddlsd-copy').click();"));
  std::string short_link;
  EXPECT_THAT(
      GetBeaconURL(active_tab),
      Eq<std::string>("https://www.google.com/gen_204"
                      "?atyp=i&ct=doodle&cad=sh%2C6%2Cct%3Atest&ntp=1"));
}

IN_PROC_BROWSER_TEST_F(LocalNTPDoodleTest, ShouldLogShareClicksWithEventId) {
  EncodedLogo cached_logo;
  cached_logo.encoded_image = MakeRefPtr(kCachedB64);
  cached_logo.metadata.mime_type = "image/png";
  cached_logo.metadata.on_click_url =
      GURL("https://www.chrotmium.org/?ct=test");
  cached_logo.metadata.alt_text = "Chromium";
  cached_logo.metadata.short_link = GURL("https://g.co");
  cached_logo.metadata.share_button_x = 12;
  cached_logo.metadata.share_button_y = 36;
  cached_logo.metadata.share_button_opacity = 0.8;
  cached_logo.metadata.share_button_icon = "sbimg";
  cached_logo.metadata.share_button_bg = "#ffff00";

  EXPECT_CALL(*logo_service(), GetLogoPtr(_))
      .WillRepeatedly(DoAll(
          ReturnCachedLogo(LogoCallbackReason::DETERMINED, cached_logo),
          ReturnFreshLogo(LogoCallbackReason::REVALIDATED, base::nullopt)));

  // Open a new blank tab, then go to NTP and listen for console messages.
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));

  // Replace window.open so we stay in the same tab.
  SetupWindowOpenTest(active_tab);
  SetupBeaconTest(active_tab);

  ASSERT_TRUE(content::ExecuteScript(active_tab, "doodles.ei = 'test_ei';"));

  ASSERT_TRUE(content::ExecuteScript(
      active_tab, "document.getElementById('ddlsb').click();"));

  ASSERT_TRUE(content::ExecuteScript(
      active_tab, "document.getElementById('ddlsd-fbb').click();"));
  EXPECT_THAT(
      GetBeaconURL(active_tab),
      Eq<std::string>("https://www.google.com/gen_204?atyp=i"
                      "&ct=doodle&cad=sh%2C2%2Cct%3Atest&ntp=1&ei=test_ei"));

  ASSERT_TRUE(content::ExecuteScript(
      active_tab, "document.getElementById('ddlsd-twb').click();"));
  EXPECT_THAT(
      GetBeaconURL(active_tab),
      Eq<std::string>("https://www.google.com/gen_204?atyp=i"
                      "&ct=doodle&cad=sh%2C3%2Cct%3Atest&ntp=1&ei=test_ei"));

  ASSERT_TRUE(content::ExecuteScript(
      active_tab, "document.getElementById('ddlsd-emb').click();"));
  EXPECT_THAT(
      GetBeaconURL(active_tab),
      Eq<std::string>("https://www.google.com/gen_204?atyp=i"
                      "&ct=doodle&cad=sh%2C5%2Cct%3Atest&ntp=1&ei=test_ei"));

  ASSERT_TRUE(content::ExecuteScript(
      active_tab, "document.getElementById('ddlsd-copy').click();"));
  std::string short_link;
  EXPECT_THAT(
      GetBeaconURL(active_tab),
      Eq<std::string>("https://www.google.com/gen_204?atyp=i"
                      "&ct=doodle&cad=sh%2C6%2Cct%3Atest&ntp=1&ei=test_ei"));
}

IN_PROC_BROWSER_TEST_F(LocalNTPDoodleTest, ShouldAnimateLogoWhenClicked) {
  EncodedLogo cached_logo;
  cached_logo.encoded_image = MakeRefPtr(kCachedB64);
  cached_logo.metadata.mime_type = "image/png";
  cached_logo.metadata.type = LogoType::ANIMATED;
  cached_logo.metadata.animated_url = GURL("data:image/png;base64,eHl6");
  cached_logo.metadata.on_click_url = GURL("https://www.chromium.org/");
  cached_logo.metadata.alt_text = "alt text";

  EXPECT_CALL(*logo_service(), GetLogoPtr(_))
      .WillRepeatedly(DoAll(
          ReturnCachedLogo(LogoCallbackReason::DETERMINED, cached_logo),
          ReturnFreshLogo(LogoCallbackReason::REVALIDATED, base::nullopt)));

  // Open a new blank tab, then go to NTP.
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  base::HistogramTester histograms;
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));

  EXPECT_THAT(GetDimension(active_tab, "fakebox", "top"), Eq(kFakeboxTopPx));
  EXPECT_THAT(GetComputedOpacity(active_tab, "logo-default"), Eq(0.0));
  EXPECT_THAT(GetComputedOpacity(active_tab, "logo-doodle"), Eq(1.0));
  EXPECT_THAT(GetComputedDisplay(active_tab, "logo-doodle-container"),
              Eq<std::string>("inline-block"));
  EXPECT_THAT(GetComputedDisplay(active_tab, "logo-doodle-iframe"),
              Eq<std::string>("none"));

  EXPECT_THAT(GetElementProperty(active_tab, "logo-doodle-image", "src"),
              Eq<std::string>("data:image/png;base64,cached++"));
  EXPECT_THAT(GetElementProperty(active_tab, "logo-doodle-image", "title"),
              Eq<std::string>("alt text"));

  // Click image, swapping out for animated URL.
  ASSERT_TRUE(content::ExecuteScript(
      active_tab, "document.getElementById('logo-doodle-button').click();"));

  EXPECT_THAT(GetDimension(active_tab, "fakebox", "top"), Eq(kFakeboxTopPx));
  EXPECT_THAT(GetElementProperty(active_tab, "logo-doodle-image", "src"),
              Eq(cached_logo.metadata.animated_url.spec()));
  // TODO(sfiera): check href by clicking on button.

  histograms.ExpectTotalCount("NewTabPage.LogoShown", 1);
  histograms.ExpectBucketCount("NewTabPage.LogoShown", kLogoImpressionCta, 1);
  histograms.ExpectTotalCount("NewTabPage.LogoShown.FromCache", 1);
  histograms.ExpectBucketCount("NewTabPage.LogoShown.FromCache",
                               kLogoImpressionCta, 1);
  histograms.ExpectTotalCount("NewTabPage.LogoShown.Fresh", 0);
  histograms.ExpectTotalCount("NewTabPage.LogoShownTime2", 1);
  histograms.ExpectTotalCount("NewTabPage.LogoClick", 1);
  histograms.ExpectBucketCount("NewTabPage.LogoClick", kLogoClickCta, 1);
}

IN_PROC_BROWSER_TEST_F(LocalNTPDoodleTest,
                       ShouldAddShareButtonInAnimatedDoodle) {
  EncodedLogo cached_logo;
  cached_logo.encoded_image = MakeRefPtr(kCachedB64);
  cached_logo.metadata.mime_type = "image/png";
  cached_logo.metadata.type = LogoType::ANIMATED;
  cached_logo.metadata.animated_url = GURL("data:image/png;base64,eHl6");
  cached_logo.metadata.on_click_url = GURL("https://www.chromium.org/");
  cached_logo.metadata.alt_text = "alt text";
  cached_logo.metadata.short_link = GURL("https://g.co");
  cached_logo.metadata.share_button_x = 12;
  cached_logo.metadata.share_button_y = 36;
  cached_logo.metadata.share_button_opacity = 0.8;
  cached_logo.metadata.share_button_icon = "sbimg";
  cached_logo.metadata.share_button_bg = "#ffff00";

  EXPECT_CALL(*logo_service(), GetLogoPtr(_))
      .WillRepeatedly(DoAll(
          ReturnCachedLogo(LogoCallbackReason::DETERMINED, cached_logo),
          ReturnFreshLogo(LogoCallbackReason::REVALIDATED, base::nullopt)));

  // Open a new blank tab, then go to NTP.
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  base::HistogramTester histograms;
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));

  // Share button should not be present pre-CTA click
  EXPECT_FALSE(ElementExists(active_tab, "ddlsb"));
  ASSERT_TRUE(content::ExecuteScript(
      active_tab, "document.getElementById('logo-doodle-button').click();"));

  // Share button is only added post-CTA click when the animation is playing
  EXPECT_TRUE(ElementExists(active_tab, "ddlsb"));
}

std::string WaitForDdllogResponse(content::WebContents* tab,
                                  int expected_ddllog_count) {
  std::string response;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      tab,
      base::StringPrintf(R"js(
        if (doodles.numDdllogResponsesReceived == %i) {
          window.domAutomationController.send(doodles.lastDdllogResponse);
        } else {
          doodles.onDdllogResponse = function() {
            if (doodles.numDdllogResponsesReceived == %i) {
              window.domAutomationController.send(doodles.lastDdllogResponse);
              doodles.onDdllogResponse = null;
            }
          }
        }                )js",
                         expected_ddllog_count, expected_ddllog_count),
      &response));
  return response;
}

IN_PROC_BROWSER_TEST_F(LocalNTPDoodleTest, ShouldLogForSimpleDoodle) {
  // Start a test server to provide the ddllog response.
  net::EmbeddedTestServer test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  test_server.ServeFilesFromSourceDirectory("chrome/test/data/local_ntp");
  ASSERT_TRUE(test_server.Start());
  const GURL on_click_url = test_server.GetURL("/simple.html");
  const GURL log_url = test_server.GetURL("/ddllog-target_url_params");

  EncodedLogo cached_logo;
  cached_logo.encoded_image = MakeRefPtr(kCachedB64);
  cached_logo.metadata.mime_type = "image/png";
  cached_logo.metadata.on_click_url = on_click_url;
  cached_logo.metadata.alt_text = "Chromium";
  cached_logo.metadata.log_url = log_url;

  EXPECT_CALL(*logo_service(), GetLogoPtr(_))
      .WillRepeatedly(DoAll(
          ReturnCachedLogo(LogoCallbackReason::DETERMINED, cached_logo),
          ReturnFreshLogo(LogoCallbackReason::REVALIDATED, base::nullopt)));

  // Open a new blank tab, then go to NTP and listen for console messages.
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  content::WebContentsDelegate* original_delegate = active_tab->GetDelegate();
  content::ConsoleObserverDelegate console_observer(active_tab, "*");
  active_tab->SetDelegate(&console_observer);
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));

  ASSERT_THAT(GetComputedOpacity(active_tab, "logo-doodle"), Eq(1.0));

  // Wait for the ddllog request to get resolved.
  std::string response = WaitForDdllogResponse(active_tab, 1);
  EXPECT_EQ("target_url_params a=b&c=d", response);

  // Before clicking on the Doodle, re-attach the original WebContentsDelegate,
  // otherwise setting 'window.location' doesn't have any effect.
  active_tab->SetDelegate(original_delegate);
  content::TestNavigationObserver nav_observer(active_tab);
  ASSERT_TRUE(content::ExecuteScript(
      active_tab, "document.getElementById('logo-doodle-button').click();"));
  nav_observer.Wait();
  ASSERT_TRUE(nav_observer.last_navigation_succeeded());

  std::string target_url;
  ASSERT_TRUE(instant_test_utils::GetStringFromJS(
      active_tab, "document.location.href", &target_url));
  EXPECT_EQ(on_click_url.spec() + "?a=b&c=d", target_url);

  EXPECT_THAT(console_observer.message(), IsEmpty());
}

IN_PROC_BROWSER_TEST_F(LocalNTPDoodleTest, ShouldLogForAnimatedDoodle) {
  // Start a test server to provide the ddllog responses.
  net::EmbeddedTestServer test_server(net::EmbeddedTestServer::TYPE_HTTPS);
  test_server.ServeFilesFromSourceDirectory("chrome/test/data/local_ntp");
  ASSERT_TRUE(test_server.Start());
  const GURL on_click_url = test_server.GetURL("/simple.html");
  const GURL cta_log_url = test_server.GetURL("/ddllog-interaction_log_url");
  const GURL log_url = test_server.GetURL("/ddllog-target_url_params");

  EncodedLogo cached_logo;
  cached_logo.encoded_image = MakeRefPtr(kCachedB64);
  cached_logo.metadata.mime_type = "image/png";
  cached_logo.metadata.type = LogoType::ANIMATED;
  cached_logo.metadata.animated_url =
      GURL(std::string("data:image/gif;base64,") + kTinyGifData);
  cached_logo.metadata.on_click_url = on_click_url;
  cached_logo.metadata.alt_text = "alt text";
  cached_logo.metadata.cta_log_url = cta_log_url;
  cached_logo.metadata.log_url = log_url;

  EXPECT_CALL(*logo_service(), GetLogoPtr(_))
      .WillRepeatedly(DoAll(
          ReturnCachedLogo(LogoCallbackReason::DETERMINED, cached_logo),
          ReturnFreshLogo(LogoCallbackReason::REVALIDATED, base::nullopt)));

  // Open a new blank tab, then go to NTP and listen for console messages.
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  content::WebContentsDelegate* original_delegate = active_tab->GetDelegate();
  content::ConsoleObserverDelegate console_observer(active_tab, "*");
  active_tab->SetDelegate(&console_observer);
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));

  ASSERT_THAT(GetComputedOpacity(active_tab, "logo-doodle"), Eq(1.0));

  // Wait for the first (CTA) ddllog request to get resolved.
  std::string cta_response = WaitForDdllogResponse(active_tab, 1);
  EXPECT_EQ(
      "interaction_log_url https://www.chromium.org/doodle_interaction_log",
      cta_response);

  // Click image, swapping out for animated URL.
  ASSERT_TRUE(content::ExecuteScript(
      active_tab, "document.getElementById('logo-doodle-button').click();"));
  ASSERT_THAT(GetElementProperty(active_tab, "logo-doodle-image", "src"),
              Eq(cached_logo.metadata.animated_url.spec()));

  // Wait for the second (non-CTA) ddllog request to get resolved.
  std::string anim_response = WaitForDdllogResponse(active_tab, 2);
  EXPECT_EQ("target_url_params a=b&c=d", anim_response);

  // Before clicking on the Doodle, re-attach the original WebContentsDelegate,
  // otherwise setting 'window.location' doesn't seem to have any effect for
  // some reason.
  active_tab->SetDelegate(original_delegate);
  content::TestNavigationObserver nav_observer(active_tab);
  ASSERT_TRUE(content::ExecuteScript(
      active_tab, "document.getElementById('logo-doodle-button').click();"));
  nav_observer.Wait();
  ASSERT_TRUE(nav_observer.last_navigation_succeeded());

  std::string target_url;
  ASSERT_TRUE(instant_test_utils::GetStringFromJS(
      active_tab, "document.location.href", &target_url));
  EXPECT_EQ(on_click_url.spec() + "?a=b&c=d", target_url);

  EXPECT_THAT(console_observer.message(), IsEmpty());
}

IN_PROC_BROWSER_TEST_F(LocalNTPDoodleTest, ShouldNotMoveFakeboxForIframeSizes) {
  for (int height : {0, 150, 229, 500}) {
    EncodedLogo cached_logo;
    cached_logo.encoded_image = MakeRefPtr(std::string());
    cached_logo.metadata.mime_type = "image/png";
    cached_logo.metadata.type = LogoType::INTERACTIVE;
    cached_logo.metadata.full_page_url =
        GURL("https://www.chromium.org/interactive");
    cached_logo.metadata.alt_text = "alt text";
    cached_logo.metadata.iframe_width_px = 500;
    cached_logo.metadata.iframe_height_px = height;

    EXPECT_CALL(*logo_service(), GetLogoPtr(_))
        .WillRepeatedly(DoAll(
            ReturnCachedLogo(LogoCallbackReason::DETERMINED, cached_logo),
            ReturnFreshLogo(LogoCallbackReason::REVALIDATED, base::nullopt)));

    // Open a new blank tab, then go to NTP.
    content::WebContents* active_tab =
        local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
    base::HistogramTester histograms;
    ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));

    EXPECT_THAT(GetDimension(active_tab, "fakebox", "top"), Eq(kFakeboxTopPx))
        << "iframe_height_px = " << height;
  }
}

IN_PROC_BROWSER_TEST_F(LocalNTPDoodleTest, ShouldMoveFakeboxWhenIframeResized) {
  EncodedLogo cached_logo;
  cached_logo.encoded_image = MakeRefPtr(std::string());
  cached_logo.metadata.mime_type = "image/png";
  cached_logo.metadata.type = LogoType::INTERACTIVE;
  cached_logo.metadata.full_page_url =
      GURL("https://www.chromium.org/interactive");
  cached_logo.metadata.alt_text = "alt text";
  cached_logo.metadata.iframe_width_px = 400;
  cached_logo.metadata.iframe_height_px = 220;  // > 200, so smaller margin

  EXPECT_CALL(*logo_service(), GetLogoPtr(_))
      .WillRepeatedly(DoAll(
          ReturnCachedLogo(LogoCallbackReason::DETERMINED, cached_logo),
          ReturnFreshLogo(LogoCallbackReason::REVALIDATED, base::nullopt)));

  // Open a new blank tab, then go to NTP.
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  base::HistogramTester histograms;
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));

  // Initial dimensions are correct:
  EXPECT_THAT(GetDimension(active_tab, "fakebox", "top"), Eq(kFakeboxTopPx));
  EXPECT_THAT(GetDimension(active_tab, "logo-doodle-iframe", "width"), Eq(400));
  EXPECT_THAT(GetDimension(active_tab, "logo-doodle-iframe", "height"),
              Eq(220));

  // Trigger resize. The duration parameter is left untested, since that would
  // require introducing a potentially-flaky sleep in the test. Additionally,
  // this would really be a cross-origin postMessage, from the Doodle iframe,
  // origin google.com.
  ASSERT_TRUE(content::ExecuteScript(active_tab,
                                     R"js(
                                         window.postMessage({
                                             cmd: "resizeDoodle",
                                             width: "100%",
                                             height: "400px",  // 180px more
                                             duration: "0s",
                                         }, "chrome-search://local-ntp");
                                     )js"));

  // Fakebox is now 180px lower, with the iframe larger, as requested.
  EXPECT_THAT(GetDimension(active_tab, "fakebox", "top"),
              Eq(kFakeboxTopPx + 180));
  EXPECT_THAT(GetDimension(active_tab, "logo-doodle-iframe", "width"),
              Eq(GetDimension(active_tab, "logo", "width")));
  EXPECT_THAT(GetDimension(active_tab, "logo-doodle-iframe", "height"),
              Eq(400));

  ASSERT_TRUE(content::ExecuteScript(active_tab,
                                     R"js(
                                         window.postMessage({
                                             cmd: "resizeDoodle",
                                             width: "400px",
                                             height: "220px",
                                             duration: "0s",
                                         }, "chrome-search://local-ntp");
                                     )js"));

  // Back to the original dimensions now.
  EXPECT_THAT(GetDimension(active_tab, "fakebox", "top"), Eq(kFakeboxTopPx));
  EXPECT_THAT(GetDimension(active_tab, "logo-doodle-iframe", "width"), Eq(400));
  EXPECT_THAT(GetDimension(active_tab, "logo-doodle-iframe", "height"),
              Eq(220));
}

// TODO(crbug/980638): Update/Remove when Linux and/or ChromeOS support dark
// mode.
#if defined(OS_WIN) || defined(OS_MACOSX)

// Tests that dark mode styling is properly applied to the local NTP Doodle.
class LocalNTPDarkModeDoodleTest : public LocalNTPDoodleTest,
                                   public DarkModeTestBase {
 public:
  LocalNTPDarkModeDoodleTest() {}

 private:
  void SetUpOnMainThread() override {
    LocalNTPDoodleTest::SetUpOnMainThread();

    theme()->AddColorSchemeNativeThemeObserver(
        ui::NativeTheme::GetInstanceForWeb());
  }
};

IN_PROC_BROWSER_TEST_F(LocalNTPDarkModeDoodleTest,
                       ShouldFadeDefaultToSimpleDoodleWhenFetched) {
  InstantService* instant_service =
      InstantServiceFactory::GetForProfile(browser()->profile());
  theme()->SetDarkMode(true);
  instant_service->SetNativeThemeForTesting(theme());
  theme()->NotifyObservers();

  EncodedLogo fresh_logo;
  fresh_logo.encoded_image = MakeRefPtr(kFreshB64);
  fresh_logo.dark_encoded_image = MakeRefPtr(kFreshDarkB64);
  fresh_logo.metadata.mime_type = "image/png";
  fresh_logo.metadata.dark_mime_type = "image/png";
  fresh_logo.metadata.on_click_url = GURL("https://www.chromium.org/");
  fresh_logo.metadata.alt_text = "Chromium";
  fresh_logo.metadata.dark_background_color = "#ABCDEF";

  EXPECT_CALL(*logo_service(), GetLogoPtr(_))
      .WillOnce(
          DoAll(ReturnCachedLogo(LogoCallbackReason::DETERMINED, base::nullopt),
                ReturnFreshLogo(LogoCallbackReason::DETERMINED, fresh_logo)))
      .WillRepeatedly(DoAll(
          ReturnCachedLogo(LogoCallbackReason::DETERMINED, fresh_logo),
          ReturnFreshLogo(LogoCallbackReason::REVALIDATED, base::nullopt)));

  // Open a new blank tab, then go to NTP.
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  base::HistogramTester histograms;
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));

  WaitForFadeIn(active_tab, "logo-doodle");
  EXPECT_THAT(GetDimension(active_tab, "fakebox", "top"), Eq(kFakeboxTopPx));
  EXPECT_THAT(GetComputedOpacity(active_tab, "logo-default"), Eq(0.0));
  EXPECT_THAT(GetComputedOpacity(active_tab, "logo-doodle"), Eq(1.0));
  EXPECT_THAT(GetComputedDisplay(active_tab, "logo-doodle-container"),
              Eq<std::string>("inline-block"));
  EXPECT_THAT(GetComputedDisplay(active_tab, "logo-doodle-wrapper"),
              Eq<std::string>("inline-block"));
  EXPECT_THAT(
      GetComputedStyle(active_tab, "logo-doodle-wrapper", "background-color"),
      Eq<std::string>("rgb(171, 205, 239)"));
  EXPECT_THAT(GetComputedDisplay(active_tab, "logo-doodle-iframe"),
              Eq<std::string>("none"));
  EXPECT_THAT(GetElementProperty(active_tab, "logo-doodle-image", "title"),
              Eq<std::string>("Chromium"));
  // TODO(sfiera): check href by clicking on button.

  histograms.ExpectTotalCount("NewTabPage.LogoShown", 1);
  histograms.ExpectBucketCount("NewTabPage.LogoShown", kLogoImpressionStatic,
                               1);
  histograms.ExpectTotalCount("NewTabPage.LogoShown.FromCache", 0);
  histograms.ExpectTotalCount("NewTabPage.LogoShown.Fresh", 1);
  histograms.ExpectBucketCount("NewTabPage.LogoShown.Fresh",
                               kLogoImpressionStatic, 1);
  histograms.ExpectTotalCount("NewTabPage.LogoShownTime2", 1);
}

IN_PROC_BROWSER_TEST_F(LocalNTPDarkModeDoodleTest,
                       DoodleUpdatesOnDarkModeChanged) {
  InstantService* instant_service =
      InstantServiceFactory::GetForProfile(browser()->profile());
  theme()->SetDarkMode(false);
  instant_service->SetNativeThemeForTesting(theme());
  theme()->NotifyObservers();

  EncodedLogo fresh_logo;
  fresh_logo.encoded_image = MakeRefPtr(kFreshB64);
  fresh_logo.dark_encoded_image = MakeRefPtr(kFreshDarkB64);
  fresh_logo.metadata.mime_type = "image/png";
  fresh_logo.metadata.dark_mime_type = "image/png";
  fresh_logo.metadata.on_click_url = GURL("https://www.chromium.org/");
  fresh_logo.metadata.alt_text = "Chromium";
  fresh_logo.metadata.dark_background_color = "#ABCDEF";

  EXPECT_CALL(*logo_service(), GetLogoPtr(_))
      .WillOnce(
          DoAll(ReturnCachedLogo(LogoCallbackReason::DETERMINED, base::nullopt),
                ReturnFreshLogo(LogoCallbackReason::DETERMINED, fresh_logo)))
      .WillRepeatedly(DoAll(
          ReturnCachedLogo(LogoCallbackReason::DETERMINED, fresh_logo),
          ReturnFreshLogo(LogoCallbackReason::REVALIDATED, base::nullopt)));

  // Open a new blank tab, then go to NTP.
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  base::HistogramTester histograms;
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));

  WaitForFadeIn(active_tab, "logo-doodle");
  EXPECT_THAT(GetDimension(active_tab, "fakebox", "top"), Eq(kFakeboxTopPx));
  EXPECT_THAT(GetComputedOpacity(active_tab, "logo-default"), Eq(0.0));
  EXPECT_THAT(GetComputedOpacity(active_tab, "logo-doodle"), Eq(1.0));
  EXPECT_THAT(GetComputedDisplay(active_tab, "logo-doodle-container"),
              Eq<std::string>("inline-block"));
  EXPECT_THAT(GetComputedDisplay(active_tab, "logo-doodle-wrapper"),
              Eq<std::string>("inline-block"));
  EXPECT_THAT(GetComputedDisplay(active_tab, "logo-doodle-iframe"),
              Eq<std::string>("none"));
  EXPECT_THAT(GetElementProperty(active_tab, "logo-doodle-image", "title"),
              Eq<std::string>("Chromium"));
  EXPECT_THAT(GetElementProperty(active_tab, "logo-doodle-image", "src"),
              Eq<std::string>("data:image/png;base64,YWJj"));
  // TODO(sfiera): check href by clicking on button.

  histograms.ExpectTotalCount("NewTabPage.LogoShown", 1);
  histograms.ExpectBucketCount("NewTabPage.LogoShown", kLogoImpressionStatic,
                               1);
  histograms.ExpectTotalCount("NewTabPage.LogoShown.FromCache", 0);
  histograms.ExpectTotalCount("NewTabPage.LogoShown.Fresh", 1);
  histograms.ExpectBucketCount("NewTabPage.LogoShown.Fresh",
                               kLogoImpressionStatic, 1);
  histograms.ExpectTotalCount("NewTabPage.LogoShownTime2", 1);

  content::DOMMessageQueue msg_queue(active_tab);
  theme()->SetDarkMode(true);
  theme()->NotifyObservers();

  local_ntp_test_utils::WaitUntilTilesLoaded(active_tab, &msg_queue,
                                             /*delay=*/1000);

  WaitForLogoSwap(active_tab, "logo-doodle");
  EXPECT_TRUE(GetIsDarkModeApplied(active_tab));
  EXPECT_THAT(GetDimension(active_tab, "fakebox", "top"), Eq(kFakeboxTopPx));
  EXPECT_THAT(GetComputedOpacity(active_tab, "logo-default"), Eq(0.0));
  EXPECT_THAT(GetComputedOpacity(active_tab, "logo-doodle"), Eq(1.0));
  EXPECT_THAT(GetComputedDisplay(active_tab, "logo-doodle-container"),
              Eq<std::string>("inline-block"));
  EXPECT_THAT(GetComputedDisplay(active_tab, "logo-doodle-wrapper"),
              Eq<std::string>("inline-block"));
  EXPECT_THAT(
      GetComputedStyle(active_tab, "logo-doodle-wrapper", "background-color"),
      Eq<std::string>("rgb(171, 205, 239)"));
  EXPECT_THAT(GetComputedDisplay(active_tab, "logo-doodle-iframe"),
              Eq<std::string>("none"));
  EXPECT_THAT(GetElementProperty(active_tab, "logo-doodle-image", "title"),
              Eq<std::string>("Chromium"));
  EXPECT_THAT(GetElementProperty(active_tab, "logo-doodle-image", "src"),
              Eq<std::string>("data:image/png;base64,eHl6"));
  // TODO(sfiera): check href by clicking on button.

  histograms.ExpectTotalCount("NewTabPage.LogoShown", 2);
  histograms.ExpectBucketCount("NewTabPage.LogoShown", kLogoImpressionStatic,
                               2);
  histograms.ExpectTotalCount("NewTabPage.LogoShown.FromCache", 0);
  histograms.ExpectTotalCount("NewTabPage.LogoShown.Fresh", 2);
  histograms.ExpectBucketCount("NewTabPage.LogoShown.Fresh",
                               kLogoImpressionStatic, 2);
  histograms.ExpectTotalCount("NewTabPage.LogoShownTime2", 1);
}

#endif
