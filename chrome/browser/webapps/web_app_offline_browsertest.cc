// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_icon_waiter.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/webapps/browser/test/service_worker_registration_waiter.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_base.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/url_loader_interceptor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_switches.h"
#include "ui/native_theme/native_theme.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_version.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/public/cpp/style/dark_light_mode_controller.h"
#endif

using ::testing::ElementsAre;

namespace {

constexpr char kHistogramClosingReason[] =
    "WebApp.DefaultOffline.ClosingReason";
constexpr char kHistogramDurationShown[] =
    "WebApp.DefaultOffline.DurationShown";

}  // namespace

namespace web_app {

enum class PageFlagParam {
  kWithDefaultPageFlag = 0,
  kWithoutDefaultPageFlag = 1,
  kMaxValue = kWithoutDefaultPageFlag
};

class WebAppOfflineTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    base::ScopedAllowBlockingForTesting allow_blocking;
    override_registration_ =
        OsIntegrationTestOverrideImpl::OverrideForTesting();
  }
  void TearDownOnMainThread() override {
    test::UninstallAllWebApps(browser()->profile());
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      override_registration_.reset();
    }
  }

  // Start a web app without a service worker and disconnect.
  webapps::AppId StartWebAppAndDisconnect(content::WebContents* web_contents,
                                          std::string_view relative_url) {
    GURL target_url(embedded_test_server()->GetURL(relative_url));
    web_app::NavigateViaLinkClickToURLAndWait(browser(), target_url);
    webapps::AppId app_id = web_app::test::InstallPwaForCurrentUrl(browser());
    WebAppIconWaiter(browser()->profile(), app_id).Wait();
    std::unique_ptr<content::URLLoaderInterceptor> interceptor =
        content::URLLoaderInterceptor::SetupRequestFailForURL(
            target_url, net::ERR_INTERNET_DISCONNECTED);

    content::TestNavigationObserver observer(web_contents, 1);
    web_contents->GetController().Reload(content::ReloadType::NORMAL, false);
    observer.Wait();
    return app_id;
  }

  // Start a PWA with a service worker and disconnect.
  void StartPwaAndDisconnect(content::WebContents* web_contents,
                             std::string_view relative_url) {
    GURL target_url(embedded_test_server()->GetURL(relative_url));
    web_app::ServiceWorkerRegistrationWaiter registration_waiter(
        browser()->profile(), target_url);
    web_app::NavigateViaLinkClickToURLAndWait(browser(), target_url);
    registration_waiter.AwaitRegistration();
    webapps::AppId app_id = web_app::test::InstallPwaForCurrentUrl(browser());
    WebAppIconWaiter(browser()->profile(), app_id).Wait();
    std::unique_ptr<content::URLLoaderInterceptor> interceptor =
        content::URLLoaderInterceptor::SetupRequestFailForURL(
            target_url, net::ERR_INTERNET_DISCONNECTED);

    content::TestNavigationObserver observer(web_contents, 1);
    web_contents->GetController().Reload(content::ReloadType::NORMAL, false);
    observer.Wait();
  }

  void ReloadWebContents(content::WebContents* web_contents) {
    content::TestNavigationObserver observer(web_contents, 1);
    web_contents->GetController().Reload(content::ReloadType::NORMAL, false);
    observer.Wait();
  }

  void CloseBrowser(content::WebContents* web_contents) {
    Browser* app_browser = chrome::FindBrowserWithTab(web_contents);
    app_browser->window()->Close();
    ui_test_utils::WaitForBrowserToClose(app_browser);
  }

 private:
  std::unique_ptr<OsIntegrationTestOverrideImpl::BlockingRegistration>
      override_registration_;
};

class WebAppOfflinePageTest : public WebAppOfflineTest {
 public:
  void SyncHistograms() {
    content::FetchHistogramsFromChildProcesses();
    metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  }

  // Expect that the histogram has been updated.
  void ExpectUniqueSample(net::Error error, int samples) {
    SyncHistograms();
    histogram_tester_.ExpectUniqueSample(
        "Net.ErrorPageCounts.WebAppAlternativeErrorPage", -error, samples);
  }

  base::HistogramTester* histogram() { return &histogram_tester_; }

 private:
  base::HistogramTester histogram_tester_;
};

// When a web app with a manifest and no service worker is offline it should
// display the default offline page rather than the dino.
// When the exact same conditions are applied with the feature flag disabled
// expect that the default offline page is not shown.
IN_PROC_BROWSER_TEST_F(WebAppOfflinePageTest, WebAppOfflinePageIsDisplayed) {
  ASSERT_TRUE(embedded_test_server()->Start());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ExpectUniqueSample(net::ERR_INTERNET_DISCONNECTED, 0);
  StartWebAppAndDisconnect(web_contents, "/banners/no-sw-with-colors.html");

  ExpectUniqueSample(net::ERR_INTERNET_DISCONNECTED, 1);
  // Expect that the default offline page is showing.
  EXPECT_TRUE(EvalJs(web_contents,
                     "document.getElementById('default-web-app-msg') !== null")
                  .ExtractBool());
}

// When a web app with a manifest and service worker that doesn't handle being
// offline it should display the default offline page rather than the dino.
IN_PROC_BROWSER_TEST_F(WebAppOfflinePageTest,
                       WebAppOfflineWithEmptyServiceWorker) {
  ASSERT_TRUE(embedded_test_server()->Start());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ExpectUniqueSample(net::ERR_INTERNET_DISCONNECTED, 0);
  StartPwaAndDisconnect(web_contents, "/banners/background-color.html");

  ExpectUniqueSample(net::ERR_INTERNET_DISCONNECTED, 1);
  // Expect that the default offline page is showing.
  EXPECT_TRUE(EvalJs(web_contents,
                     "document.getElementById('default-web-app-msg') !== null")
                  .ExtractBool());
}

// When a web app with a manifest and service worker that handles being offline
// it should not display the default offline page.
IN_PROC_BROWSER_TEST_F(WebAppOfflinePageTest, WebAppOfflineWithServiceWorker) {
  ASSERT_TRUE(embedded_test_server()->Start());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ExpectUniqueSample(net::ERR_INTERNET_DISCONNECTED, 0);
  StartPwaAndDisconnect(web_contents, "/banners/theme-color.html");

  ExpectUniqueSample(net::ERR_INTERNET_DISCONNECTED, 0);
  // Expect that the default offline page is not showing.
  EXPECT_TRUE(EvalJs(web_contents,
                     "document.getElementById('default-web-app-msg') === null")
                  .ExtractBool());
}

// Default offline page icon test.
IN_PROC_BROWSER_TEST_F(WebAppOfflinePageTest, WebAppOfflinePageIconShowing) {
  ASSERT_TRUE(embedded_test_server()->Start());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  StartWebAppAndDisconnect(web_contents,
                           "/banners/no_sw_fetch_handler_test_page.html");
  WaitForLoadStop(web_contents);

  constexpr char kExpectedIconUrl[] =
      "data:image/"
      "png;base64,"
      "iVBORw0KGgoAAAANSUhEUgAAAKAAAACgCAIAAAAErfB6AAAAAXNSR0IArs4c6QAADvBJREFU"
      "eJztnHuMXPV1x8/3/"
      "O68dndmd2dtHDAmBoxxzMsOjwA2GFo1UVJC0xa1pUqDoihRU0IKIY2B8AhPE6ukRG2hDZXaC"
      "oHSoqhq2jQhqQBhU2PzNGAgEMCQEBt7H7Oved37O6d//"
      "GaXXXvXeLGdvfvj99FobO3M3Lkzn/n97vmdc+7F/"
      "Lu2U8BfeLZ3IHBoCYI9Jwj2nCDYc4JgzwmCPScI9pwg2HOCYM8Jgj0nCPacINhzgmDPCYI9J"
      "wj2nCDYc4JgzwmCPScI9pwg2HOCYM8Jgj0nCPacINhzgmDPCYI9Jwj2nCDYc4JgzwmCPScI9"
      "pwg2HOCYM8Jgj0nCPacINhzgmDPCYI9Jwj2nCDYc4JgzwmCPScI9pwg2HOCYM8Jgj0nCPacI"
      "NhzgmDPCYI9Jwj2nCDYc4JgzwmCPScI9pxotnfg/aOqDERT/"
      "URFKREFMAu7lTLmqmAlBZCI7qoKqRKBSInI/ScToZw3VigonpOCVRVALLqwnf/"
      "ut3s6s0bJOSZRZeCpdxq3Pj5QLhirs72vs83cE6xEAJi0EcvXT+u+eFlx7+"
      "e0Z3BDIowgeA4KJlXDGGzoHx/ffvGyYiLqRq17MBGNGEMNIUA/"
      "8HbnpGADVGNZXDJXntaVNbCiEYMBN28TUcQwDCLC+"
      "HH5A8wcWyapqlVl4IqPlk6cn7OqZrLdwB7MJcHOYj3RTyzOX3JiKRE1gBLtHEkAaJiRp2LOC"
      "HaxlRWdX+Bbzi5nDTMpESp1+61N/"
      "S5+nu19TCNzRjCpGqKG1evO7F5SzoqoEgF051ODW3Y0iUg1TNFTMDcEq2rEqDTtp48u/"
      "MmyDlG1qob56Z21u7cOMRMRBb9TMgcEKykDtUSOKkbXnd1dyLBVBVBp2Js3V/"
      "oamjNwGawZbFMVRAwyE26M1kPun328eGwbCpq0hT03kgLmwDIJCoKK0hWndi7vycVWAYoY92"
      "4bfuitensGMz36qhKAptVqMumFDGrP7EdM3groAFDDam3yRgyorbWRVCRK0y5YVQ1oqCkXHF"
      "P4/AnFxCqRRoytu2p3Pj0YMWJRndnoJQMaiuXipe3XfKxLxpLYIOyqJn/2P7v6G5IxvC/"
      "HqgAYNNSQC47J33ZOj1ViIlGKGDtGkr98uO/"
      "VSlyIOA2jOO2CATRFFxWj6z5WzkWcWDGMeqy3bxncUZWuHNfrghl+"
      "kULUHuGHr41etLTtk8d0jP99SXfm2+eWv/DT3SBtxe3T7RVpPdbju6P15/"
      "Ys7sxOfOiuZyovDzQ7Mqmwm/ZjsKoSaWz18pWlkw7LWRHDYNB9L4/"
      "8xy+qxQwnVkGY4QBuTdEJ4SsP9W3bXSciK2JFrOgfHNdx6YrScFMMjx1rJ87/"
      "2hIvSrkIt59TXtyZTUSsSGxFVf/hmcrdzw23Z4xQWhKl6RXsYqtqrB9fnP/iKSVVBYgIrw/"
      "Et2+ptGdY3Cw68+McQKSaYeyuydoN/"
      "YN1S4TxguOVp3WtWZgfrGvE0D0OpICqGsZIUy5fWfrE0e2iagAQMoaf2tlY98RgIWKogjQNB"
      "+D0ClYlEBLR+QW+eVXZwM14APTax/p3jloDOqDcJKCq+QiP/LKx/"
      "okBwyBSBpS0pxCtX9NzVJFHY+HJCTK3Whuo2c8c23blaV2iSqruaD1Qs1c80tffkIghwAyD+"
      "kNISgUDykSjsaw9vXN5T1ZUE1EG/"
      "vWF4QffrOUMH3iECoAIhQzufm7k314aMsyJKECxlZPm5649qzvHsJPtGqZqrCfOy9y8upwda"
      "yVRIkCv3tD39K5mW8SSsoRLGgWrqgGGmvb3l7R9dnlRlEQ1a/"
      "gXA43vPjOYCEV8cBaa7o1U6VuPV57b1cgYVoUBWZHPLi99bnlHPVE3iN2qKBHKR3Tj2V1Lur"
      "OxFVK1Sob5H7cO/fur1baMWxqlym8qBTNQi3Vhh/"
      "nG6V0dWU6sqzHIHU8OvtAbt40lOg78jQBY1VyE7UP22o19Iw3rtgoiVb1pVfeqw3MjTWvgas"
      "taj+XylaULji0mIhFDiDKGt+yorX+iItKa4Q/C5z+opE6wqooqQb+yorRyQT4RjQwiph+/"
      "Mfr9n4+W82wPajedK2B0ZvnBt+q3bh5gEAGuNtWeNd85v+eIDtMUcb+"
      "5Tx1duPzULivKgAuvdo0max/t2zEqhfGgL2WkS7ALWBpWz1mY/"
      "+LJnbG4hCLeGUlu2lQRghIOenjqxnFnznzv+"
      "eEHfj7i5DnxJ8zL3XRWtyg1RI8uRevX9BQyDFK4J6jesnlg8864K8+"
      "pbeJMkWB9dzzhttXltgwbct+arttSeWUgKRgcohDGuRHFDZsq23obhtmqgjSx+"
      "kfLil86qVhvynfO617cmRVRAEJg4P6XRv5520gxy4mkIy05FSkSTKpMVLdy9RldJ83PudCGg"
      "R+/"
      "Vr33pdG2DA7WoXeaN9ecwfbBZO2jfaOxNQARMdQwvvrRzh9cuOC8RW2i6kIuw3j2ndq1G/"
      "szgLrc5SHarQMmLYJVKWIMNu3HP1y45ISia80hQm8tufHxgdhNgIf0awSsUinLD/"
      "2yccumgUTIEtyOHVnMfOrY9sgwA6JEQG81ufzhvkpTIwNNs97UCFaGNqwe3hZdf2Z3e9aIKg"
      "iGsX5LZVtfnI+gh3h9CSJARbUjg+89P/"
      "LD10YzjLGAmsRlTVWVIEo3P155cldciOZAL1gqBKuCCE0rl60onnJYLhYVJcP42fbRe18cyb"
      "ZC29/AjkABAFbpmg39L/c22GUrSRkgVasaMe5/cehfXhwuRFBCyu2mQrASMWi4Kb+1KP/"
      "nK0uqBNWM4d5qcseTg9WE2rIwIAPsUVqfeNuDvevw2L8iPIhEtRDhreHk+"
      "k0DLpxWVbd4M8DmHbXrN1UiwLgsZkpKCtMz++"
      "VCEDWtHNFhrjurXIiMKwg2Elm3eeDBV0fzbWbH6LSFOxAlTemvy1jpqVWKb1i1VdsboTlW7y"
      "1EaIv4PaNdHXOcj3DR0ra9i/"
      "+GyGBCOiP1IzglgnV+wZz+oXxsxTAADNSTwaZ+bkXne36BjUQ/XIpc/"
      "mv8flEx+"
      "sxJpXKBrZIoMejVgfiF3jgfvVftuHXahL1sRemi44qunOCy1gyyqqcdXrhtdfnLP+"
      "tNQK1BnG7HmH/X9tneB6rGcmJPZtOfLkyUMnxIIpe7nh289Ke9R3RGsUz7nFZrX8OuOjz3/"
      "QsWlAtGWyfFgKg1V1ulCHTVht6/"
      "3zpSMGh17uyrOWCWScUIJmolCDFWfVPVfZiYiBIxUaa1XGndi2osrS03rWYNhhpCvK8ivHNZ"
      "jWVhu1l3bnleW2RFGCBA1dUN3d6qVVx1Rnnr7uSxt+"
      "v5zD6be1JAKoIsIiLVRDQRtUrunrFfNzPN8thMftp+KICoGsY3Tu9yBwsQJaIgumfr0E3/"
      "1+8UMwikXXlz5/"
      "k9C9o5tpLykypSMYJdbihiRPz+"
      "h4IbRhg7Eo9vyW2zLeJWwmkqF274jjT14mVtXzqlZMVFUsgYPL+"
      "78d1nht4cilfMz154XNFVGqzI8eXsHWt6LvnJbiIF0jtHz75gJYoYQ0350evViSeC7g8gSlR"
      "LWT5vUcG91t3vHE02/bqej9hdyyFiPNfbzEaQKe0SuXLkiT2Zdef0EAEkLkCuxva6x/"
      "q3D9n2jPn6o/"
      "1Ly9llPTkRYSJRvfDY9q+d2rh9y2ApZ8bS0akTPftBlvtKEtGBmp1wJYb9JpYlh+Ve/"
      "cJRsZWMYXf/g1dGL7r/V6YUWWnNEYUMl3K810UdXFsdCVEG9MCnF6w+sqBjnXUMuuGx/"
      "tu3DHbnjRDVEzl3Yf6+3z2sI8Nu2iHCcNN+/"
      "ie7H3yz1pkz6Swozf4Ixlj4ekRxxjsDomosH2o3e0zRhQjZUnR48d2Y2SrtXUhWAikZpuG6v"
      "XV19+ojC4kokyqRYf7v14bv2jrcmWOrRKT5CA//qr5+y8Ct58xz8ZdV7cpHf72m57X/"
      "3PnGkG1LZUl49oOscSuxvJ9bUyjZK95Woubkp8mUzTSqEVOlbv/"
      "wuLbLVnaKvmv3jUrz2o0D7veBVtUSbdGkBi4GJVaO7c7esrrckYVr6Rp7/"
      "7SQCsGzQmtdlOgJPdnrzyq7sxmIiIBGItds6H+lYvPRu/"
      "F3q4GL6IZNlWfHG7gYiciFSzouPaXUtMpjqbRZ/"
      "mwT+"
      "OAKdi0ZWaYbzupaWs66BY8lQOlvn6n81xu1Ypat0HiuqtXAZfDWsP3mxr6hhgVIVZlIVa86o"
      "/uTi/ODDRulbNX0ARXs6gTVWP/ilOKFSzpcmch1w298u/"
      "Y3Tw3lDBEpoADGKwqu4aSU5f99q37zpv6JDVyRwR3nzftIORpNxCBFZxd+"
      "EAW7ybmW6PmLcmvP6LaqLoPGwK7R5K8e7RtsaoZZdCzPNuHg7cZxV8780wsj920bdqGWE7+"
      "olPn2ufPaM0iU0pP9mNuCx0/UdVmw8ZsVne4UX1VioGGlnOd1q8r5CFbUKimBQN/"
      "c2Pd8X1LM7qszF4ASEXDj5srT79Qj5qZVq1pL5LyjCl9dURpuCqUmnJ7bgidmwfIRj9+"
      "XcjxdoxRAopo3uOd3ek5ekGcgazhj2DDu2Tr0wCvVjghjK9pph6Br4Hp72K7d0DfStLmIs4Y"
      "LEWcYV59Z/vLJxYbVlMTSs78OPhD2zoKNX8owG/"
      "GUeSsmGkl01RH5huBHr1d17OTgoaZd98QgTer8mnYIArBCxRw/"
      "9uvm1x7p+70lHVbVgFwjykd6suU8DzZ1ygul/oaZ/"
      "UzWgTBVFuw9LkaqSgyqJTrcsETjFpWAngLP5JJb6mb74VhqTZnwa1AwynmeUc710DG3R/"
      "B0WbB9XE7YTdGFCMXsnp89FppJ7Q9uU51Z7s5NGqpKlMxsU4eQuS14YhZsyr9P9xKd6iX7ft"
      "V0m7JKU17yNA1253yQFXhPgmDPCYI9Jwj2nCDYc4JgzwmCPScI9pwg2HOCYM8Jgj0nCPacIN"
      "hzgmDPCYI9Jwj2nCDYc4JgzwmCPScI9pwg2HOCYM8Jgj0nCPacINhzgmDPCYI9Jwj2nCDYc4"
      "JgzwmCPScI9pwg2HOCYM8Jgj0nCPacINhzgmDPCYI9Jwj2nCDYc4JgzwmCPScI9pwg2HOCYM"
      "8Jgj0nCPacINhzgmDPCYI9Jwj2nCDYc4JgzwmCPef/Afw/wBt50raAAAAAAElFTkSuQmCC";

  // Ensure that we don't proceed until the icon loading is finished.
  ASSERT_EQ(
      true,
      EvalJs(web_contents,
             "var promiseResolve;"
             "var imageLoadedPromise = new Promise(resolve => {"
             "  promiseResolve = resolve;"
             "});"
             "function mutatedCallback(mutations) {"
             "  let mutation = mutations[0];"
             "  if (mutation.attributeName == 'src' &&"
             "      mutation.target.src.startsWith('data:image/png')) {"
             "    console.log('Change in src observed, resolving promise');"
             "    promiseResolve();"
             "  }"
             "}"
             "let observer = new MutationObserver(mutatedCallback);"
             "observer.observe(document.getElementById('icon'),"
             "                 {attributes: true});"
             "if (document.getElementById('icon').src.startsWith("
             "    'data:image/png')) {"
             "  console.log('Inline src already set, resolving promise');"
             "  promiseResolve();"
             "}"
             "imageLoadedPromise.then(function(e) {"
             "  return true;"
             "});")
          .ExtractBool());

  // Expect that the icon on the default offline page is showing.
  EXPECT_EQ("You're offline",
            EvalJs(web_contents,
                   "document.getElementById('default-web-app-msg').textContent")
                .ExtractString());
  EXPECT_EQ("Manifest test app",
            EvalJs(web_contents, "document.title").ExtractString());
  EXPECT_EQ(kExpectedIconUrl,
            EvalJs(web_contents, "document.getElementById('icon').src")
                .ExtractString());
  EXPECT_EQ("inline",
            EvalJs(web_contents,
                   "document.getElementById('offlineIcon').style.display")
                .ExtractString());
}

IN_PROC_BROWSER_TEST_F(WebAppOfflinePageTest, WebAppOfflineMetricsNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ExpectUniqueSample(net::ERR_INTERNET_DISCONNECTED, 0);
  StartWebAppAndDisconnect(web_contents, "/banners/no-sw-with-colors.html");

  SyncHistograms();
  histogram()->ExpectTotalCount(kHistogramDurationShown, 0);
  histogram()->ExpectTotalCount(kHistogramClosingReason, 0);

  ExpectUniqueSample(net::ERR_INTERNET_DISCONNECTED, 1);
  // Expect that the default offline page is showing.
  EXPECT_TRUE(EvalJs(web_contents,
                     "document.getElementById('default-web-app-msg') !== null")
                  .ExtractBool());

  // Navigate somewhere else (anywhere else but the current page will do).
  EXPECT_TRUE(NavigateToURL(web_contents, GURL("about:blank")));

  SyncHistograms();
  histogram()->ExpectTotalCount(kHistogramDurationShown, 1);
  histogram()->ExpectTotalCount(kHistogramClosingReason, 1);
  EXPECT_THAT(histogram()->GetAllSamples(kHistogramClosingReason),
              ElementsAre(base::Bucket(/* min= */ 1, /* count= */ 1)));
}

IN_PROC_BROWSER_TEST_F(WebAppOfflinePageTest, WebAppOfflineMetricsBackOnline) {
  ASSERT_TRUE(embedded_test_server()->Start());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ExpectUniqueSample(net::ERR_INTERNET_DISCONNECTED, 0);
  StartWebAppAndDisconnect(web_contents, "/banners/no-sw-with-colors.html");

  SyncHistograms();
  histogram()->ExpectTotalCount(kHistogramDurationShown, 0);
  histogram()->ExpectTotalCount(kHistogramClosingReason, 0);

  ExpectUniqueSample(net::ERR_INTERNET_DISCONNECTED, 1);
  // Expect that the default offline page is showing.
  EXPECT_TRUE(EvalJs(web_contents,
                     "document.getElementById('default-web-app-msg') !== null")
                  .ExtractBool());

  // The URL interceptor only blocks the first navigation. This one should
  // go through.
  ReloadWebContents(web_contents);

  // Expect that the default offline page is not showing.
  EXPECT_TRUE(EvalJs(web_contents,
                     "document.getElementById('default-web-app-msg') === null")
                  .ExtractBool());

  SyncHistograms();
  histogram()->ExpectTotalCount(kHistogramDurationShown, 1);
  histogram()->ExpectTotalCount(kHistogramClosingReason, 1);
  EXPECT_THAT(histogram()->GetAllSamples(kHistogramClosingReason),
              ElementsAre(base::Bucket(/* min= */ 0, /* count= */ 1)));
}

IN_PROC_BROWSER_TEST_F(WebAppOfflinePageTest, WebAppOfflineMetricsPwaClosing) {
  ASSERT_TRUE(embedded_test_server()->Start());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ExpectUniqueSample(net::ERR_INTERNET_DISCONNECTED, 0);
  webapps::AppId app_id =
      StartWebAppAndDisconnect(web_contents, "/banners/no-sw-with-colors.html");

  SyncHistograms();
  histogram()->ExpectTotalCount(kHistogramDurationShown, 0);
  histogram()->ExpectTotalCount(kHistogramClosingReason, 0);

  ExpectUniqueSample(net::ERR_INTERNET_DISCONNECTED, 1);
  // Expect that the default offline page is showing.
  EXPECT_TRUE(EvalJs(web_contents,
                     "document.getElementById('default-web-app-msg') !== null")
                  .ExtractBool());

  CloseBrowser(web_contents);

  SyncHistograms();
  histogram()->ExpectTotalCount(kHistogramDurationShown, 1);
  histogram()->ExpectTotalCount(kHistogramClosingReason, 1);
  EXPECT_THAT(histogram()->GetAllSamples(kHistogramClosingReason),
              ElementsAre(base::Bucket(/* min= */ 2, /* count= */ 1)));
}

class WebAppOfflineDarkModeTest
    : public WebAppOfflineTest,
      public testing::WithParamInterface<blink::mojom::PreferredColorScheme> {
 public:
  void SetUp() override {
#if BUILDFLAG(IS_WIN)
    InProcessBrowserTest::SetUp();
#elif BUILDFLAG(IS_MAC)
    // TODO(crbug.com/40215627): Get this test suite working.
    GTEST_SKIP();
#else
    InProcessBrowserTest::SetUp();
#endif  // BUILDFLAG(IS_MAC)
  }

  void SetUpOnMainThread() override {
    WebAppOfflineTest::SetUpOnMainThread();
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // Explicitly set dark mode in ChromeOS or we can't get light mode after
    // sunset (due to dark mode auto-scheduling).
    ash::DarkLightModeController::Get()->SetDarkModeEnabledForTest(
        GetParam() == blink::mojom::PreferredColorScheme::kDark);
#endif
  }

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // ShellContentBrowserClient::OverrideWebkitPrefs() overrides the
    // prefers-color-scheme according to switches::kForceDarkMode
    // command line.
    if (GetParam() == blink::mojom::PreferredColorScheme::kDark)
      command_line->AppendSwitch(switches::kForceDarkMode);
  }
};

// Testing offline page in dark mode for a web app with a manifest and no
// service worker.
// TODO(crbug.com/40871921): tests are flaky on Lacros and Linux.
#if BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_LINUX)
#define MAYBE_WebAppOfflineDarkModeNoServiceWorker \
  DISABLED_WebAppOfflineDarkModeNoServiceWorker
#else
#define MAYBE_WebAppOfflineDarkModeNoServiceWorker \
  WebAppOfflineDarkModeNoServiceWorker
#endif
IN_PROC_BROWSER_TEST_P(WebAppOfflineDarkModeTest,
                       MAYBE_WebAppOfflineDarkModeNoServiceWorker) {
#if BUILDFLAG(IS_WIN)
  if (GetParam() == blink::mojom::PreferredColorScheme::kLight &&
      ui::NativeTheme::GetInstanceForNativeUi()->ShouldUseDarkColors()) {
    GTEST_SKIP() << "Host is in dark mode; skipping test";
  }
#endif  // BUILDFLAG(IS_WIN)

  ASSERT_TRUE(embedded_test_server()->Start());

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  StartWebAppAndDisconnect(web_contents, "/banners/no-sw-with-colors.html");

  if (GetParam() == blink::mojom::PreferredColorScheme::kDark) {
    // Expect that the default offline page is showing with dark mode colors.
    EXPECT_TRUE(
        EvalJs(web_contents,
               "window.matchMedia('(prefers-color-scheme: dark)').matches")
            .ExtractBool());
    EXPECT_EQ(
        EvalJs(web_contents,
               "window.getComputedStyle(document.querySelector('div')).color")
            .ExtractString(),
        "rgb(227, 227, 227)");
    EXPECT_EQ(EvalJs(web_contents,
                     "window.getComputedStyle(document.querySelector('body'))."
                     "backgroundColor")
                  .ExtractString(),
              "rgb(31, 31, 31)");
  } else {
    // Expect that the default offline page is showing with light mode colors.
    EXPECT_TRUE(
        EvalJs(web_contents,
               "window.matchMedia('(prefers-color-scheme: light)').matches")
            .ExtractBool());
    EXPECT_EQ(
        EvalJs(web_contents,
               "window.getComputedStyle(document.querySelector('div')).color")
            .ExtractString(),
        "rgb(31, 31, 31)");
    EXPECT_EQ(EvalJs(web_contents,
                     "window.getComputedStyle(document.querySelector('body'))."
                     "backgroundColor")
                  .ExtractString(),
              "rgb(255, 255, 255)");
  }
}

// Testing offline page in dark mode for a web app with a manifest and service
// worker that does not handle offline error.
// TODO(crbug.com/40871921): tests are flaky on Lacros and Linux.
#if BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_LINUX)
#define MAYBE_WebAppOfflineDarkModeEmptyServiceWorker \
  DISABLED_WebAppOfflineDarkModeEmptyServiceWorker
#else
#define MAYBE_WebAppOfflineDarkModeEmptyServiceWorker \
  WebAppOfflineDarkModeEmptyServiceWorker
#endif
IN_PROC_BROWSER_TEST_P(WebAppOfflineDarkModeTest,
                       MAYBE_WebAppOfflineDarkModeEmptyServiceWorker) {
#if BUILDFLAG(IS_WIN)
  if (GetParam() == blink::mojom::PreferredColorScheme::kLight &&
      ui::NativeTheme::GetInstanceForNativeUi()->ShouldUseDarkColors()) {
    GTEST_SKIP() << "Host is in dark mode; skipping test";
  }
#endif  // BUILDFLAG(IS_WIN)

  ASSERT_TRUE(embedded_test_server()->Start());

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  StartPwaAndDisconnect(web_contents,
                        "/banners/manifest_test_page_empty_fetch_handler.html");
  if (GetParam() == blink::mojom::PreferredColorScheme::kDark) {
    // Expect that the default offline page is showing with dark mode colors.
    EXPECT_TRUE(
        EvalJs(web_contents,
               "window.matchMedia('(prefers-color-scheme: dark)').matches")
            .ExtractBool());
    EXPECT_EQ(
        EvalJs(web_contents,
               "window.getComputedStyle(document.querySelector('div')).color")
            .ExtractString(),
        "rgb(227, 227, 227)");
    EXPECT_EQ(EvalJs(web_contents,
                     "window.getComputedStyle(document.querySelector('body'))."
                     "backgroundColor")
                  .ExtractString(),
              "rgb(31, 31, 31)");
  } else {
    // Expect that the default offline page is showing with light mode colors.
    EXPECT_TRUE(
        EvalJs(web_contents,
               "window.matchMedia('(prefers-color-scheme: light)').matches")
            .ExtractBool());
    EXPECT_EQ(
        EvalJs(web_contents,
               "window.getComputedStyle(document.querySelector('div')).color")
            .ExtractString(),
        "rgb(31, 31, 31)");
    EXPECT_EQ(EvalJs(web_contents,
                     "window.getComputedStyle(document.querySelector('body'))."
                     "backgroundColor")
                  .ExtractString(),
              "rgb(255, 255, 255)");
  }
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    WebAppOfflineDarkModeTest,
    ::testing::Values(blink::mojom::PreferredColorScheme::kDark,
                      blink::mojom::PreferredColorScheme::kLight));
}  // namespace web_app
