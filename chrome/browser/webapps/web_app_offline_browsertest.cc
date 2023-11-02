// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/webapps/browser/test/service_worker_registration_waiter.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_base.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/url_loader_interceptor.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
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
                                          base::StringPiece relative_url) {
    GURL target_url(embedded_test_server()->GetURL(relative_url));
    web_app::NavigateToURLAndWait(browser(), target_url);
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
                             base::StringPiece relative_url) {
    GURL target_url(embedded_test_server()->GetURL(relative_url));
    web_app::ServiceWorkerRegistrationWaiter registration_waiter(
        browser()->profile(), target_url);
    web_app::NavigateToURLAndWait(browser(), target_url);
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
      "iVBORw0KGgoAAAANSUhEUgAAAKAAAACgCAIAAAAErfB6AAAO8ElEQVR4nO2ce4xc9XXHz/"
      "f87rx2d2Z3Z20cMCYGjHHMyw6PADYYWjVRUkLTFrWlSoOiKFFTQgohjYHwCE8Tq6REbaENld"
      "oKgdKiqGraNCGpAGFTY/M0YCAQwJAQG3sfs6953fs7p3/"
      "8Zpdde9d4sZ29++P30Whs7czcuTOf+f3u+"
      "Z1z7sX8u7ZTwF94tncgcGgJgj0nCPacINhzgmDPCYI9Jwj2nCDYc4JgzwmCPScI9pwg2HOCY"
      "M8Jgj0nCPacINhzgmDPCYI9Jwj2nCDYc4JgzwmCPScI9pwg2HOCYM8Jgj0nCPacINhzgmDPC"
      "YI9Jwj2nCDYc4JgzwmCPScI9pwg2HOCYM8Jgj0nCPacINhzgmDPCYI9Jwj2nCDYc4JgzwmCP"
      "ScI9pwg2HOCYM8Jgj0nCPacINhzgmDPCYI9Jwj2nGi2d+"
      "D9o6oMRFP9REUpEQUwC7uVMuaqYCUFkIjuqgqpEoFIicj9JxOhnDdWKCiek4JVFUAsurCd/"
      "+63ezqzRsk5JlFl4Kl3Grc+PlAuGKuzva+"
      "zzdwTrEQAmLQRy9dP6754WXHv57RncEMijCB4DgomVcMYbOgfH99+"
      "8bJiIupGrXswEY0YQw0hQD/"
      "wduekYANUY1lcMlee1pU1sKIRgwE3bxNRxDAMIsL4cfkDzBxbJqmqVWXgio+"
      "WTpyfs6pmst3AHswlwc5iPdFPLM5fcmIpETWAEu0cSQBomJGnYs4IdrGVFZ1f4FvOLmcNMyk"
      "RKnX7rU39Ln6e7X1MI3NGMKkaoobV687sXlLOiqgSAXTnU4NbdjSJSDVM0VMwNwSrasSoNO2"
      "njy78ybIOUbWqhvnpnbW7tw4xExEFv1MyBwQrKQO1RI4qRted3V3IsFUFUGnYmzdX+"
      "hqaM3AZrBlsUxVEDDITbozWQ+6ffbx4bBsKmrSFPTeSAubAMgkKgorSFad2Lu/"
      "JxVYBihj3bht+6K16ewYzPfqqEoCm1Woy6YUMas/"
      "sR0zeCugAUMNqbfJGDKittZFUJErTLlhVDWioKRccU/"
      "j8CcXEKpFGjK27anc+PRgxYlGd2eglAxqK5eKl7dd8rEvGktgg7Komf/Y/u/"
      "obkjG8L8eqABg01JALjsnfdk6PVWIiUYoYO0aSv3y479VKXIg4DaM47YIBNEUXFaPrPlbORZ"
      "xYMYx6rLdvGdxRla4c1+uCGX6RQtQe4YevjV60tO2Tx3SM/31Jd+bb55a/"
      "8NPdIG3F7dPtFWk91uO7o/"
      "Xn9izuzE586K5nKi8PNDsyqbCb9mOwqhJpbPXylaWTDstZEcNg0H0vj/"
      "zHL6rFDCdWQZjhAG5N0QnhKw/1bdtdJyIrYkWs6B8c13HpitJwUwyPHWsnzv/"
      "aEi9KuQi3n1Ne3JlNRKxIbEVV/+GZyt3PDbdnjFBaEqXpFexiq2qsH1+c/"
      "+IpJVUFiAivD8S3b6m0Z1jcLDrz4xxApJph7K7J2g39g3VLhPGC45Wnda1ZmB+"
      "sa8TQPQ6kgKoaxkhTLl9Z+sTR7aJqABAyhp/"
      "a2Vj3xGAhYqiCNA0H4PQKViUQEtH5Bb55VdnAzXgA9NrH+neOWgM6oNwkoKr5CI/"
      "8srH+iQHDIFIGlLSnEK1f03NUkUdj4ckJMrdaG6jZzxzbduVpXaJKqu5oPVCzVzzS19+"
      "QiCHADIP6Q0hKBQPKRKOxrD29c3lPVlQTUQb+9YXhB9+s5QwfeIQKgAiFDO5+"
      "buTfXhoyzIkoQLGVk+bnrj2rO8ewk+0apmqsJ87L3Ly6nB1rJVEiQK/"
      "e0Pf0rmZbxJKyhEsaBauqAYaa9veXtH12eVGURDVr+BcDje8+"
      "M5gIRXxwFprujVTpW49XntvVyBhWhQFZkc8uL31ueUc9UTeI3aooEcpHdOPZXUu6s7EVUrVK"
      "hvkftw79+6vVtoxbGqXKbyoFM1CLdWGH+cbpXR1ZTqyrMcgdTw6+"
      "0Bu3jSU6DvyNAFjVXITtQ/"
      "bajX0jDeu2CiJVvWlV96rDcyNNa+"
      "Bqy1qP5fKVpQuOLSYiEUOIMoa37Kitf6Ii0prhD8LnP6ikTrCqiipBv7KitHJBPhGNDCKmH7"
      "8x+v2fj5bzbA9qN50rYHRm+cG36rduHmAQAa421Z413zm/54gO0xRxv7lPHV24/"
      "NQuK8qAC692jSZrH+3bMSqF8aAvZaRLsAtYGlbPWZj/"
      "4smdsbiEIt4ZSW7aVBGCEg56eOrGcWfOfO/"
      "54Qd+"
      "PuLkOfEnzMvddFa3KDVEjy5F69f0FDIMUrgnqN6yeWDzzrgrz6lt4kyRYH13POG21eW2DBty"
      "35qu21J5ZSApGByiEMa5EcUNmyrbehuG2aqCNLH6R8uKXzqpWG/Kd87rXtyZFVEAQmDg/"
      "pdG/"
      "nnbSDHLiaQjLTkVKRJMqkxUt3L1GV0nzc+"
      "50IaBH79Wvfel0bYMDtahd5o315zB9sFk7aN9o7E1ABEx1DC++"
      "tHOH1y44LxFbaLqQi7DePad2rUb+zOAutzlIdqtAyYtglUpYgw27cc/"
      "XLjkhKJrzSFCby258fGB2E2Ah/RrBKxSKcsP/"
      "bJxy6aBRMgS3I4dWcx86tj2yDADokRAbzW5/OG+SlMjA02z3tQIVoY2rB7eFl1/"
      "Znd71ogqCIaxfktlW1+cj6CHeH0JIkBFtSOD7z0/8sPXRjOMsYCaxGVNVZUgSjc/"
      "XnlyV1yI5kAvWCoEq4IITSuXrSieclguFhUlw/"
      "jZ9tF7XxzJtkLb38COQAEAVumaDf0v9zbYZStJGSBVqxox7n9x6F9eHC5EUELK7aZCsBIxaL"
      "gpv7Uo/+crS6oE1Yzh3mpyx5OD1YTasjAgA+xRWp9424O96/"
      "DYvyI8iES1EOGt4eT6TQMunFZVt3gzwOYdtes3VSLAuCxmSkoK0zP75UIQNa0c0WGuO6tciI"
      "wrCDYSWbd54MFXR/NtZsfotIU7ECVN6a/LWOmpVYpvWLVV2xuhOVbvLURoi/"
      "g9o10dc5yPcNHStr2L/4bIYEI6I/"
      "UjOCWCdX7BnP6hfGzFMAAM1JPBpn5uRed7foGNRD9cilz+a/"
      "x+UTH6zEmlcoGtkigx6NWB+IXeOB+9V+24ddqEvWxF6aLjiq6c4LLWDLKqpx1euG11+cs/"
      "601ArUGcbseYf9f22d4HqsZyYk9m058uTJQyfEgil7ueHbz0p71HdEaxTPucVmtfw646PPf9"
      "CxaUC0ZbJ8WAqDVXW6UIdNWG3r/"
      "fOlIwaHXu7Ks5YJZJxQgmaiUIMVZ9U9V9mJiIEjFRprVcad2LaiytLTetZg2GGkK8ryK8c1m"
      "NZWG7WXdueV5bZEUYIEDV1Q3d3qpVXHVGeevu5LG36/"
      "nMPpt7UkAqgiwiItVENBG1Su6esV83M83y2Ex+2n4ogKgaxjdO73IHCxAloiC6Z+vQTf/"
      "X7xQzCKRdeXPn+T0L2jm2kvKTKlIxgl1uKGJE/P6HghtGGDsSj2/"
      "JbbMt4lbCaSoXbviONPXiZW1fOqVkxUVSyBg8v7vx3WeG3hyKV8zPXnhc0VUarMjx5ewda3o"
      "u+"
      "cluIgXSO0fPvmAlihhDTfnR69WJJ4LuDyBKVEtZPm9Rwb3W3e8cTTb9up6P2F3LIWI819vMR"
      "pAp7RK5cuSJPZl15/QQASQuQK7G9rrH+rcP2faM+fqj/"
      "UvL2WU9ORFhIlG98Nj2r53auH3LYClnxtLRqRM9+0GW+"
      "0oS0YGanXAlhv0mliWH5V79wlGxlYxhd/"
      "+DV0Yvuv9XphRZac0RhQyXcrzXRR1cWx0JUQb0wKcXrD6yoGOddQy64bH+"
      "27cMdueNENUTOXdh/r7fPawjw27aIcJw037+J7sffLPWmTPpLCjN/"
      "gjGWPh6RHHGOwOiaiwfajd7TNGFCNlSdHjx3ZjZKu1dSFYCKRmm4bq9dXX36iMLiSiTKpFh/"
      "u/Xhu/aOtyZY6tEpPkID/+qvn7LwK3nzHPxl1Xtykd/vabntf/"
      "c+caQbUtlSXj2g6xxK7G8n1tTKNkr3lai5uSnyZTNNKoRU6Vu//C4tstWdoq+a/"
      "eNSvPajQPu94FW1RJt0aQGLgYlVo7tzt6yutyRhWvpGnv/"
      "tJAKwbNCa12U6Ak92evPKruzGYiIgEYi12zof6Vi89G78XergYvohk2VZ8cbuBiJyIVLOi49"
      "pdS0ymOptFn+bBP44Ap2LRlZphvO6lpazroFjyVA6W+fqfzXG7Vilq3QeK6q1cBl8Naw/"
      "ebGvqGGBUhVmUhVrzqj+5OL84MNG6Vs1fQBFezqBNVY/"
      "+KU4oVLOlyZyHXDb3y79jdPDeUMESmgAMYrCq7hpJTl/32rfvOm/"
      "okNXJHBHefN+0g5Gk3EIEVnF34QBbvJuZbo+Ytya8/"
      "otqoug8bArtHkrx7tG2xqhll0LM824eDtxnFXzvzTCyP3bRt2oZYTv6iU+fa589ozSJTSk/"
      "2Y24LHT9R1WbDxmxWd7hRfVWKgYaWc53WryvkIVtQqKYFA39zY93xfUszuqzMXgBIRcOPmyt"
      "Pv1CPmplWrWkvkvKMKX11RGm4KpSacntuCJ2bB8hGP35dyPF2jFECimje453d6Tl6QZyBrOG"
      "PYMO7ZOvTAK9WOCGMr2mmHoGvgenvYrt3QN9K0uYizhgsRZxhXn1n+8snFhtWUxNKzvw4+"
      "EPbOgo1fyjAb8ZR5KyYaSXTVEfmG4EevV3Xs5OChpl33xCBN6vyadggCsELFHD/"
      "26+bXHun7vSUdVtWAXCPKR3qy5TwPNnXKC6X+"
      "hpn9TNaBMFUW7D0uRqpKDKolOtywROMWlYCeAs/"
      "kklvqZvvhWGpNmfBrUDDKeZ5RzvXQMbdH8HRZsH1cTthN0YUIxeyenz0WmkntD25TnVnuzk0"
      "aqkqUzGxTh5C5LXhiFmzKv0/"
      "3Ep3qJft+1XSbskpTXvI0DXbnfJAVeE+"
      "CYM8Jgj0nCPacINhzgmDPCYI9Jwj2nCDYc4JgzwmCPScI9pwg2HOCYM8Jgj0nCPacINhzgmD"
      "PCYI9Jwj2nCDYc4JgzwmCPScI9pwg2HOCYM8Jgj0nCPacINhzgmDPCYI9Jwj2nCDYc4Jgzwm"
      "CPScI9pwg2HOCYM8Jgj0nCPacINhzgmDPCYI9Jwj2nCDYc4JgzwmCPScI9pwg2HOCYM8Jgj0"
      "nCPacINhzgmDPCYI95/8B/D/AG3nStoAAAAAASUVORK5CYII=";

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
    EXPECT_EQ(
        "You're offline",
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
  WebAppOfflineDarkModeTest() {
    std::vector<base::test::FeatureRef> disabled_features;
    feature_list_.InitWithFeatures({blink::features::kWebAppEnableDarkMode},
                                   {disabled_features});
  }

  void SetUp() override {
#if BUILDFLAG(IS_WIN)
    InProcessBrowserTest::SetUp();
#elif BUILDFLAG(IS_MAC)
    // TODO(crbug.com/1298658): Get this test suite working.
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

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Testing offline page in dark mode for a web app with a manifest and no
// service worker.
// TODO(crbug.com/1373750): tests are flaky on Lacros and Linux.
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

  // ui::NativeTheme::GetInstanceForNativeUi()->set_use_dark_colors(true);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  StartWebAppAndDisconnect(
      web_contents, "/web_apps/get_manifest.html?color_scheme_dark.json");

  // Expect that the default offline page is showing with dark mode colors.
  if (GetParam() == blink::mojom::PreferredColorScheme::kDark) {
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
// TODO(crbug.com/1373750): tests are flaky on Lacros and Linux.
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
  StartPwaAndDisconnect(
      web_contents,
      "/banners/manifest_test_page_empty_fetch_handler.html?manifest=../"
      "web_apps/color_scheme_dark.json");
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

// Testing offline page in dark mode for a web app with a manifest that has not
// provided dark mode colors.
// TODO(crbug.com/1373750): tests are flaky on Lacros and Linux.
#if BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_LINUX)
#define MAYBE_WebAppOfflineNoDarkModeColorsProvided \
  DISABLED_WebAppOfflineNoDarkModeColorsProvided
#else
#define MAYBE_WebAppOfflineNoDarkModeColorsProvided \
  WebAppOfflineNoDarkModeColorsProvided
#endif
IN_PROC_BROWSER_TEST_P(WebAppOfflineDarkModeTest,
                       MAYBE_WebAppOfflineNoDarkModeColorsProvided) {
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

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    WebAppOfflineDarkModeTest,
    ::testing::Values(blink::mojom::PreferredColorScheme::kDark,
                      blink::mojom::PreferredColorScheme::kLight));
}  // namespace web_app
