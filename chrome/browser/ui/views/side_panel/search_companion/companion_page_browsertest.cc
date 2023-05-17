// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base64.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/companion/core/companion_metrics_logger.h"
#include "chrome/browser/companion/core/constants.h"
#include "chrome/browser/companion/core/features.h"
#include "chrome/browser/companion/core/mojom/companion.mojom.h"
#include "chrome/browser/companion/core/proto/companion_url_params.pb.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/side_panel/companion/companion_tab_helper.h"
#include "chrome/browser/ui/side_panel/companion/companion_utils.h"
#include "chrome/browser/ui/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/search_companion/search_companion_side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/unified_consent/pref_names.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/base/url_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/tab_helper.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

using side_panel::mojom::MethodType;
using side_panel::mojom::PromoAction;
using side_panel::mojom::PromoType;
using side_panel::mojom::UiSurface;

namespace {

const char kRelativeUrl1[] = "/english_page.html";
const char kRelativeUrl2[] = "/german_page.html";
const char kRelativeUrl3[] = "/simple.html";
const char kRelativeUrl4[] = "/simple.html#part1";
const char kHost[] = "foo.com";
const char kSearchQueryUrl[] = "https://www.google.com/search?q=xyz";

}  // namespace

// Helper class to generate a script that sends a postmessage to the browser
// with given parameters.
struct CompanionScriptBuilder {
  // Only mandatory argument.
  MethodType method_type;

  // The rest of the arguments are optional. If the value is set, will be added
  // to the postmessage.
  absl::optional<PromoType> promo_type;
  absl::optional<PromoAction> promo_action;
  absl::optional<bool> is_exps_opted_in;
  absl::optional<UiSurface> ui_surface;
  absl::optional<int> ui_surface_position;
  absl::optional<int> child_element_available_count;
  absl::optional<int> child_element_shown_count;
  absl::optional<std::string> text_directive;
  absl::optional<std::vector<std::string>> cq_text_directives;
  absl::optional<int> click_position;

  // Useful in case chrome sends a postmessage in response. Companion waits for
  // the message in response and resolves the promise that was sent back to
  // EvalJs.
  bool wait_for_message = false;

  // Constructor.
  explicit CompanionScriptBuilder(MethodType type) : method_type(type) {}

  // Generates the JS script that can be injected to simulate a postmessage.
  std::string Build() {
    std::stringstream ss;
    ss << "const message = {};";
    ss << "message['type'] = "
       << base::NumberToString(static_cast<size_t>(method_type)) << ";";

    if (promo_type.has_value()) {
      ss << "message['promoType'] = "
         << base::NumberToString(static_cast<size_t>(promo_type.value()))
         << ";";
    }

    if (promo_action.has_value()) {
      ss << "message['promoAction'] = "
         << base::NumberToString(static_cast<size_t>(promo_action.value()))
         << ";";
    }

    if (is_exps_opted_in.has_value()) {
      ss << "message['isExpsOptedIn'] = "
         << base::NumberToString(is_exps_opted_in.value()) << ";";
    }

    if (ui_surface.has_value()) {
      ss << "message['uiSurface'] = "
         << base::NumberToString(static_cast<size_t>(ui_surface.value()))
         << ";";
    }

    if (ui_surface_position.has_value()) {
      ss << "message['uiSurfacePosition'] = "
         << base::NumberToString(ui_surface_position.value()) << ";";
    }

    if (child_element_available_count.has_value()) {
      ss << "message['childElementAvailableCount'] = "
         << base::NumberToString(child_element_available_count.value()) << ";";
    }

    if (child_element_shown_count.has_value()) {
      ss << "message['childElementShownCount'] = "
         << base::NumberToString(child_element_shown_count.value()) << ";";
    }

    if (text_directive.has_value()) {
      ss << "message['cqJumptagText'] = '" << text_directive.value() << "';";
    }

    if (cq_text_directives.has_value()) {
      std::string joined_text;
      for (const auto& text : cq_text_directives.value()) {
        joined_text.append("'" + text + "',");
      }
      ss << "message['cqTextDirectives'] = [" << joined_text << "];";
    }

    if (click_position.has_value()) {
      ss << "message['clickPosition'] = "
         << base::NumberToString(click_position.value()) << ";";
    }

    ss << "window.parent.postMessage(message, '*');";

    if (wait_for_message) {
      ss << "waitForMessage();";
    }

    return ss.str();
  }
};

class CompanionPageBrowserTest : public InProcessBrowserTest {
 protected:
  void SetUp() override {
    page_url_server_.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    companion_server_.ServeFilesFromSourceDirectory(GetChromeTestDataDir());

    // Register a handler to inspect the URL and examine the proto.
    // Nevertheless, it returns null which causes the default handler to be
    // invoked right away.
    companion_server_.RegisterRequestHandler(base::BindRepeating(
        &CompanionPageBrowserTest::InspectRequest, base::Unretained(this)));

    ASSERT_TRUE(page_url_server_.Start());
    ASSERT_TRUE(companion_server_.Start());
    SetUpFeatureList();
    histogram_tester_ = std::make_unique<base::HistogramTester>();
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  GURL CreateUrl(const std::string& host, const std::string& relative_url) {
    return page_url_server_.GetURL(host, relative_url);
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  SidePanelCoordinator* side_panel_coordinator() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->side_panel_coordinator();
  }

  content::WebContents* GetCompanionWebContents(Browser* browser) {
    auto* companion_helper =
        companion::CompanionTabHelper::FromWebContents(web_contents());
    auto* web_contents = companion_helper->GetCompanionWebContentsForTesting();
    DCHECK(web_contents);
    return web_contents;
  }

  void WaitForCompanionToBeLoaded() {
    content::WebContents* companion_web_contents =
        GetCompanionWebContents(browser());
    EXPECT_TRUE(companion_web_contents);

    // Verify that extensions do not have access to the companion web contents.
#if BUILDFLAG(ENABLE_EXTENSIONS)
    CHECK_EQ(nullptr,
             extensions::TabHelper::FromWebContents(companion_web_contents));
#endif

    // Wait for the navigations in both the frames to complete.
    content::TestNavigationObserver nav_observer(companion_web_contents, 2);
    nav_observer.Wait();
  }

  void WaitForMainPageToBeLoaded(const std::string& relative_url) {
    // Wait for the navigations in the frame to complete.

    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                             CreateUrl(kHost, relative_url)));
  }

  void WaitForCompanionIframeReload() {
    content::WebContents* companion_web_contents =
        GetCompanionWebContents(browser());
    EXPECT_TRUE(companion_web_contents);

    // Wait for the navigations in both the frames to complete.
    content::TestNavigationObserver nav_observer(companion_web_contents, 1);
    nav_observer.Wait();
  }

  ::testing::AssertionResult ExecJs(const std::string& code) {
    // Execute test in iframe.
    content::RenderFrameHost* iframe =
        content::ChildFrameAt(GetCompanionWebContents(browser()), 0);

    return content::ExecJs(iframe, code);
  }

  content::EvalJsResult EvalJs(const std::string& code) {
    // Execute test in iframe.
    content::RenderFrameHost* iframe =
        content::ChildFrameAt(GetCompanionWebContents(browser()), 0);

    return content::EvalJs(iframe, code);
  }

  std::unique_ptr<net::test_server::HttpResponse> InspectRequest(
      const net::test_server::HttpRequest& request) {
    requests_received_on_server_++;
    const GURL& url = request.GetURL();

    std::string query_proto;
    EXPECT_TRUE(
        net::GetValueForKeyInQuery(url, "companion_query", &query_proto));
    last_proto_from_url_load_ = DeserializeCompanionRequest(query_proto);

    return nullptr;
  }

  absl::optional<companion::proto::CompanionUrlParams>
  GetLastCompanionProtoFromUrlLoad() {
    auto proto_copy = last_proto_from_url_load_;
    last_proto_from_url_load_ = absl::nullopt;
    return proto_copy;
  }

  companion::proto::CompanionUrlParams DeserializeCompanionRequest(
      const std::string& companion_url_param) {
    companion::proto::CompanionUrlParams proto;
    auto base64_decoded = base::Base64Decode(companion_url_param);
    auto serialized_proto = std::string(base64_decoded.value().begin(),
                                        base64_decoded.value().end());
    EXPECT_TRUE(proto.ParseFromString(serialized_proto));
    return proto;
  }

  absl::optional<companion::proto::CompanionUrlParams>
  GetLastCompanionProtoFromPostMessage() {
    companion::proto::CompanionUrlParams proto;
    content::EvalJsResult eval_js_result =
        EvalJs("getLastReceivedCompanionProto()");
    if (!eval_js_result.error.empty() || !eval_js_result.value.is_string()) {
      return absl::nullopt;
    }

    std::string companion_proto_encoded = eval_js_result.ExtractString();
    proto = DeserializeCompanionRequest(companion_proto_encoded);
    return proto;
  }

  void EnableMsbb(bool enable_msbb) {
    auto* pref_service = browser()->profile()->GetPrefs();
    pref_service->SetBoolean(
        unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
        enable_msbb);
  }

  void EnableSignInMsbbExps(bool signed_in, bool msbb, bool exps) {
    if (signed_in) {
      // Mock a signed-in user.
      signin::SetPrimaryAccount(
          IdentityManagerFactory::GetForProfile(browser()->profile()),
          "someemail@gmail.com", signin::ConsentLevel::kSignin);
    }

    // Set MSBB and exps status.
    EnableMsbb(msbb);
    browser()->profile()->GetPrefs()->SetBoolean(
        companion::kExpsOptInStatusGrantedPref, exps);
  }

  virtual void SetUpFeatureList() {
    base::FieldTrialParams params;
    params["companion-homepage-url"] =
        companion_server_.GetURL("/companion_iframe.html").spec();
    feature_list_.InitAndEnableFeatureWithParameters(
        companion::features::kSidePanelCompanion, params);
  }

  void WaitForHistogram(const std::string& histogram_name) {
    // Continue if histogram was already recorded.
    if (base::StatisticsRecorder::FindHistogram(histogram_name)) {
      return;
    }

    // Else, wait until the histogram is recorded.
    base::RunLoop run_loop;
    auto histogram_observer = std::make_unique<
        base::StatisticsRecorder::ScopedHistogramSampleObserver>(
        histogram_name,
        base::BindLambdaForTesting(
            [&](const char* histogram_name, uint64_t name_hash,
                base::HistogramBase::Sample sample) { run_loop.Quit(); }));
    run_loop.Run();
  }

  void ExpectUkmEntry(ukm::TestUkmRecorder* ukm_recorder,
                      const char* metric_name,
                      int expected_value) {
    // There should be only one UKM entry of Companion_PageView type.
    const char* entry_name = ukm::builders::Companion_PageView::kEntryName;
    EXPECT_EQ(ukm_recorder->GetEntriesByName(entry_name).size(), 1ul);
    auto* entry = ukm_recorder->GetEntriesByName(entry_name)[0];

    // Verify the metric.
    ukm_recorder->EntryHasMetric(entry, metric_name);
    ukm_recorder->ExpectEntryMetric(entry, metric_name, expected_value);
  }

  size_t requests_received_on_server() const {
    return requests_received_on_server_;
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  net::EmbeddedTestServer page_url_server_{net::EmbeddedTestServer::TYPE_HTTPS};
  net::EmbeddedTestServer companion_server_{
      net::EmbeddedTestServer::TYPE_HTTPS};
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  absl::optional<companion::proto::CompanionUrlParams>
      last_proto_from_url_load_;
  size_t requests_received_on_server_ = 0;
};

IN_PROC_BROWSER_TEST_F(CompanionPageBrowserTest, InitialNavigationWithoutMsbb) {
  // Turn off Msbb. Load a page on the active tab and open companion side panel.
  EnableMsbb(false);
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), CreateUrl(kHost, kRelativeUrl1)));
  side_panel_coordinator()->Show(SidePanelEntry::Id::kSearchCompanion);

  WaitForCompanionToBeLoaded();
  EXPECT_EQ(side_panel_coordinator()->GetCurrentEntryId(),
            SidePanelEntry::Id::kSearchCompanion);
  EXPECT_EQ(1u, requests_received_on_server());

  // Inspect the URL from the proto.
  auto proto = GetLastCompanionProtoFromUrlLoad();
  EXPECT_TRUE(proto.has_value());
  EXPECT_TRUE(proto->page_url().empty());
}

IN_PROC_BROWSER_TEST_F(CompanionPageBrowserTest,
                       SubsequentNavigationWithAndWithoutMsbb) {
  // Load a page on the active tab and open companion side panel
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), CreateUrl(kHost, kRelativeUrl1)));
  side_panel_coordinator()->Show(SidePanelEntry::Id::kSearchCompanion);

  WaitForCompanionToBeLoaded();
  EXPECT_EQ(1u, requests_received_on_server());
  EXPECT_EQ(side_panel_coordinator()->GetCurrentEntryId(),
            SidePanelEntry::Id::kSearchCompanion);

  // Turn off Msbb, and navigate to a URL. Verify that URL isn't sent.
  EnableMsbb(false);
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), CreateUrl(kHost, kRelativeUrl2)));
  auto proto = GetLastCompanionProtoFromPostMessage();
  EXPECT_FALSE(proto.has_value());

  // Turn on Msbb, and navigate to a URL. Verify that URL is sent.
  EnableMsbb(true);
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), CreateUrl(kHost, kRelativeUrl3)));
  proto = GetLastCompanionProtoFromPostMessage();
  EXPECT_TRUE(proto.has_value());
  EXPECT_EQ(proto->page_url(), CreateUrl(kHost, kRelativeUrl3));
}

IN_PROC_BROWSER_TEST_F(CompanionPageBrowserTest, AutoRefreshOnMsbb) {
  EnableSignInMsbbExps(/*signed_in=*/true, /*msbb=*/false, /*exps=*/false);

  // Load a page on the active tab and open companion side panel
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), CreateUrl(kHost, kRelativeUrl1)));
  side_panel_coordinator()->Show(SidePanelEntry::Id::kSearchCompanion);

  WaitForCompanionToBeLoaded();
  EXPECT_EQ(side_panel_coordinator()->GetCurrentEntryId(),
            SidePanelEntry::Id::kSearchCompanion);

  // Inspect the URL from the proto. This will reset the proto.
  auto proto = GetLastCompanionProtoFromUrlLoad();
  EXPECT_TRUE(proto.has_value());
  EXPECT_TRUE(proto->page_url().empty());

  // Turn on Msbb via promo. This should auto refresh the companion page.
  CompanionScriptBuilder builder(MethodType::kOnPromoAction);
  builder.promo_type = PromoType::kMsbb;
  builder.promo_action = PromoAction::kAccepted;
  EXPECT_TRUE(ExecJs(builder.Build()));
  WaitForHistogram("Companion.PromoEvent");

  WaitForCompanionIframeReload();
  proto = GetLastCompanionProtoFromUrlLoad();
  EXPECT_TRUE(proto.has_value());
  EXPECT_EQ(proto->page_url(), CreateUrl(kHost, kRelativeUrl1));
}

IN_PROC_BROWSER_TEST_F(CompanionPageBrowserTest, AutoRefreshOnTabForegrounded) {
  EnableSignInMsbbExps(/*signed_in=*/false, /*msbb=*/false, /*exps=*/false);

  // Load a page on the active tab and open companion side panel
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), CreateUrl(kHost, kRelativeUrl1)));
  side_panel_coordinator()->Show(SidePanelEntry::Id::kSearchCompanion);

  WaitForCompanionToBeLoaded();
  EXPECT_EQ(side_panel_coordinator()->GetCurrentEntryId(),
            SidePanelEntry::Id::kSearchCompanion);

  // Inspect the URL from the proto. This will reset the proto.
  auto proto = GetLastCompanionProtoFromUrlLoad();
  EXPECT_TRUE(proto.has_value());
  EXPECT_TRUE(proto->page_url().empty());

  // Navigate to a new tab.
  chrome::NewTab(browser());

  // Go back to the original tab. This should refresh the companion.
  browser()->tab_strip_model()->ActivateTabAt(0);
  WaitForCompanionIframeReload();

  proto = GetLastCompanionProtoFromUrlLoad();
  EXPECT_TRUE(proto.has_value());
  EXPECT_TRUE(proto->page_url().empty());
}

IN_PROC_BROWSER_TEST_F(CompanionPageBrowserTest,
                       DontAutoRefreshIfHasAllPermissions) {
  EnableSignInMsbbExps(/*signed_in=*/true, /*msbb=*/true, /*exps=*/true);

  // Load a page on the active tab and open companion side panel
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), CreateUrl(kHost, kRelativeUrl1)));
  side_panel_coordinator()->Show(SidePanelEntry::Id::kSearchCompanion);

  WaitForCompanionToBeLoaded();
  EXPECT_EQ(side_panel_coordinator()->GetCurrentEntryId(),
            SidePanelEntry::Id::kSearchCompanion);

  // Inspect the URL from the proto. This will reset the proto.
  auto proto = GetLastCompanionProtoFromUrlLoad();
  EXPECT_TRUE(proto.has_value());
  EXPECT_FALSE(proto->page_url().empty());
  EXPECT_EQ(proto->page_url(), CreateUrl(kHost, kRelativeUrl1));

  // Navigate to a new tab.
  chrome::NewTab(browser());

  // Go back to the original tab. This should not refresh the companion.
  browser()->tab_strip_model()->ActivateTabAt(0);
  proto = GetLastCompanionProtoFromUrlLoad();
  EXPECT_FALSE(proto.has_value());
}

IN_PROC_BROWSER_TEST_F(CompanionPageBrowserTest, SamePageNavigation) {
  // Load a page on the active tab and open companion side panel
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), CreateUrl(kHost, kRelativeUrl3)));
  side_panel_coordinator()->Show(SidePanelEntry::Id::kSearchCompanion);
  WaitForCompanionToBeLoaded();

  // Navigation to a same document URL. Verify that companion is not refreshed.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), CreateUrl(kHost, kRelativeUrl4)));
  auto proto = GetLastCompanionProtoFromPostMessage();
  EXPECT_FALSE(proto.has_value());
}

IN_PROC_BROWSER_TEST_F(CompanionPageBrowserTest,
                       UiSurfaceShownAndClickedForListSurfaces) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  // Load a page on the active tab.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), CreateUrl(kHost, kRelativeUrl1)));
  ASSERT_EQ(side_panel_coordinator()->GetCurrentEntryId(), absl::nullopt);

  // Open companion companion via toolbar entry point.
  side_panel_coordinator()->Show(SidePanelEntry::Id::kSearchCompanion);
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());

  WaitForCompanionToBeLoaded();
  EXPECT_EQ(side_panel_coordinator()->GetCurrentEntryId(),
            SidePanelEntry::Id::kSearchCompanion);

  // Post message for showing CQ surface. Verify histograms.
  CompanionScriptBuilder builder(MethodType::kRecordUiSurfaceShown);
  builder.ui_surface = UiSurface::kCQ;
  builder.ui_surface_position = 3;
  builder.child_element_available_count = 8;
  builder.child_element_shown_count = 5;
  EXPECT_TRUE(ExecJs(builder.Build()));

  WaitForHistogram("Companion.CQ.Shown");
  histogram_tester_->ExpectBucketCount("Companion.CQ.Shown",
                                       /*sample=*/true, /*expected_count=*/1);

  // Post message for click metrics. Verify histograms.
  CompanionScriptBuilder builder2(MethodType::kRecordUiSurfaceClicked);
  builder2.ui_surface = UiSurface::kCQ;
  builder2.click_position = 3;
  EXPECT_TRUE(ExecJs(builder2.Build()));
  WaitForHistogram("Companion.CQ.Clicked");
  histogram_tester_->ExpectBucketCount("Companion.CQ.Clicked",
                                       /*sample=*/true,
                                       /*expected_count=*/1);
  histogram_tester_->ExpectBucketCount("Companion.CQ.ClickPosition",
                                       /*sample=*/3,
                                       /*expected_count=*/1);

  // Close side panel and verify UKM.
  side_panel_coordinator()->Close();
  ExpectUkmEntry(&ukm_recorder,
                 ukm::builders::Companion_PageView::kCQ_LastEventName,
                 static_cast<int>(companion::UiEvent::kClicked));
  ExpectUkmEntry(&ukm_recorder,
                 ukm::builders::Companion_PageView::kCQ_ClickPositionName, 3);

  ExpectUkmEntry(&ukm_recorder,
                 ukm::builders::Companion_PageView::kCQ_ComponentPositionName,
                 3);
  ExpectUkmEntry(&ukm_recorder,
                 ukm::builders::Companion_PageView::kCQ_NumEntriesAvailableName,
                 8);
  ExpectUkmEntry(&ukm_recorder,
                 ukm::builders::Companion_PageView::kCQ_NumEntriesShownName, 5);
  ExpectUkmEntry(&ukm_recorder,
                 ukm::builders::Companion_PageView::kCQ_ClickPositionName, 3);
}

IN_PROC_BROWSER_TEST_F(CompanionPageBrowserTest,
                       UiSurfaceShownAndClickedForNonListSurfaces) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  // Load a page on the active tab.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), CreateUrl(kHost, kRelativeUrl1)));
  ASSERT_EQ(side_panel_coordinator()->GetCurrentEntryId(), absl::nullopt);

  // Open companion companion via toolbar entry point.
  side_panel_coordinator()->Show(SidePanelEntry::Id::kSearchCompanion);
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());

  WaitForCompanionToBeLoaded();
  EXPECT_EQ(side_panel_coordinator()->GetCurrentEntryId(),
            SidePanelEntry::Id::kSearchCompanion);

  // Post message for showing PH surface. Verify histograms.
  CompanionScriptBuilder builder(MethodType::kRecordUiSurfaceShown);
  builder.ui_surface = UiSurface::kPH;
  builder.ui_surface_position = 3;
  EXPECT_TRUE(ExecJs(builder.Build()));

  WaitForHistogram("Companion.PH.Shown");
  histogram_tester_->ExpectBucketCount("Companion.PH.Shown",
                                       /*sample=*/true, /*expected_count=*/1);

  // Post message for click metrics. Verify histograms.
  CompanionScriptBuilder builder2(MethodType::kRecordUiSurfaceClicked);
  builder2.ui_surface = UiSurface::kPH;
  EXPECT_TRUE(ExecJs(builder2.Build()));
  WaitForHistogram("Companion.PH.Clicked");
  histogram_tester_->ExpectBucketCount("Companion.PH.Clicked",
                                       /*sample=*/true,
                                       /*expected_count=*/1);
  histogram_tester_->ExpectTotalCount("Companion.PH.ClickPosition", 0);

  // Close side panel and verify UKM.
  side_panel_coordinator()->Close();
  ExpectUkmEntry(&ukm_recorder,
                 ukm::builders::Companion_PageView::kPH_LastEventName,
                 static_cast<int>(companion::UiEvent::kClicked));

  ExpectUkmEntry(&ukm_recorder,
                 ukm::builders::Companion_PageView::kPH_ComponentPositionName,
                 3);
}

IN_PROC_BROWSER_TEST_F(CompanionPageBrowserTest, PostMessageForPromoEvents) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  // Load a page on the active tab.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), CreateUrl(kHost, kRelativeUrl1)));
  ASSERT_EQ(side_panel_coordinator()->GetCurrentEntryId(), absl::nullopt);

  // Open companion companion via toolbar entry point.
  side_panel_coordinator()->Show(SidePanelEntry::Id::kSearchCompanion);
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());

  WaitForCompanionToBeLoaded();
  EXPECT_EQ(side_panel_coordinator()->GetCurrentEntryId(),
            SidePanelEntry::Id::kSearchCompanion);

  // Show a promo, user rejects it. Verify histogram.
  CompanionScriptBuilder builder(MethodType::kOnPromoAction);
  builder.promo_type = PromoType::kMsbb;
  builder.promo_action = PromoAction::kRejected;
  EXPECT_TRUE(ExecJs(builder.Build()));

  WaitForHistogram("Companion.PromoEvent");
  histogram_tester_->ExpectBucketCount("Companion.PromoEvent",
                                       companion::PromoEvent::kMsbbRejected,
                                       /*expected_count=*/1);
  // Close side panel and verify UKM.
  side_panel_coordinator()->Close();
  ExpectUkmEntry(&ukm_recorder,
                 ukm::builders::Companion_PageView::kPromoEventName,
                 static_cast<int>(companion::PromoEvent::kMsbbRejected));
}

IN_PROC_BROWSER_TEST_F(CompanionPageBrowserTest, RegionSearchClick) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  // Load a page on the active tab.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), CreateUrl(kHost, kRelativeUrl1)));
  ASSERT_EQ(side_panel_coordinator()->GetCurrentEntryId(), absl::nullopt);

  // Open companion companion via toolbar entry point.
  side_panel_coordinator()->Show(SidePanelEntry::Id::kSearchCompanion);
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());

  WaitForCompanionToBeLoaded();
  EXPECT_EQ(side_panel_coordinator()->GetCurrentEntryId(),
            SidePanelEntry::Id::kSearchCompanion);

  // Post message for click metrics. Verify histograms.
  CompanionScriptBuilder builder(MethodType::kRecordUiSurfaceClicked);
  builder.ui_surface = UiSurface::kRegionSearch;
  EXPECT_TRUE(ExecJs(builder.Build()));
  WaitForHistogram("Companion.RegionSearch.Clicked");

  histogram_tester_->ExpectBucketCount("Companion.RegionSearch.Clicked",
                                       /*sample=*/true,
                                       /*expected_count=*/1);
  histogram_tester_->ExpectTotalCount("Companion.RegionSearch.ClickPosition",
                                      0);

  side_panel_coordinator()->Close();
  ExpectUkmEntry(
      &ukm_recorder,
      ukm::builders::Companion_PageView::kRegionSearch_ClickCountName, 1);
}

IN_PROC_BROWSER_TEST_F(CompanionPageBrowserTest,
                       PostMessageForCqCandidatesAvailable) {
  // Load a page on the active tab.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), CreateUrl(kHost, kRelativeUrl1)));
  ASSERT_EQ(side_panel_coordinator()->GetCurrentEntryId(), absl::nullopt);

  // Open companion companion via toolbar entry point.
  side_panel_coordinator()->Show(SidePanelEntry::Id::kSearchCompanion);
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());

  WaitForCompanionToBeLoaded();
  EXPECT_EQ(side_panel_coordinator()->GetCurrentEntryId(),
            SidePanelEntry::Id::kSearchCompanion);

  CompanionScriptBuilder builder(MethodType::kOnCqCandidatesAvailable);
  builder.ui_surface = UiSurface::kCQ;
  builder.cq_text_directives = std::vector<std::string>{"abc", "def"};
  builder.wait_for_message = true;
  content::EvalJsResult eval_js_result = EvalJs(builder.Build());
  const base::Value promise_values = eval_js_result.ExtractList();
  EXPECT_EQ(2u, promise_values.GetList().size());
  EXPECT_EQ(content::ListValueOf(false, false), promise_values);
}

IN_PROC_BROWSER_TEST_F(CompanionPageBrowserTest,
                       PostMessageForCqJumptagClicked) {
  // Load a page on the active tab.
  GURL url = page_url_server_.GetURL(kHost, kRelativeUrl1);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  ASSERT_EQ(side_panel_coordinator()->GetCurrentEntryId(), absl::nullopt);

  // Open companion companion via toolbar entry point.
  side_panel_coordinator()->Show(SidePanelEntry::Id::kSearchCompanion);
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());

  WaitForCompanionToBeLoaded();
  EXPECT_EQ(side_panel_coordinator()->GetCurrentEntryId(),
            SidePanelEntry::Id::kSearchCompanion);

  // Click a cq jumptag.
  CompanionScriptBuilder builder(MethodType::kOnCqJumptagClicked);
  builder.ui_surface = UiSurface::kCQ;
  builder.text_directive = "English";
  EXPECT_TRUE(ExecJs(builder.Build()));
  WaitForHistogram("Companion.CQ.TextHighlight.Success");
  // TODO(b/280453152): Fix the metrics expectation.
  histogram_tester_->ExpectBucketCount("Companion.CQ.TextHighlight.Success",
                                       /*sample=*/false, /*expected_count=*/1);
}

IN_PROC_BROWSER_TEST_F(CompanionPageBrowserTest,
                       OpenedFromContextMenuTextSearch) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  // Load a page on the active tab.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), CreateUrl(kHost, kRelativeUrl1)));

  // Start a text query via context menu. It should open companion side panel.
  auto* companion_helper =
      companion::CompanionTabHelper::FromWebContents(web_contents());
  companion_helper->ShowCompanionSidePanelForSearchURL(GURL(kSearchQueryUrl));
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());

  WaitForCompanionToBeLoaded();
  EXPECT_EQ(side_panel_coordinator()->GetCurrentEntryId(),
            SidePanelEntry::Id::kSearchCompanion);

  // Close side panel and verify UKM.
  side_panel_coordinator()->Close();
  ExpectUkmEntry(
      &ukm_recorder, ukm::builders::Companion_PageView::kOpenTriggerName,
      static_cast<int>(SidePanelOpenTrigger::kContextMenuSearchOption));
}

IN_PROC_BROWSER_TEST_F(CompanionPageBrowserTest,
                       OpenedFromContextMenuImageSearch) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  // Load a page on the active tab.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), CreateUrl(kHost, kRelativeUrl1)));

  // Start a image query via context menu. It should open companion side panel.
  GURL src_url = CreateUrl(kHost, kRelativeUrl2);
  gfx::Size original_size(8, 8);
  gfx::Size downscaled_size(8, 8);
  std::vector<uint8_t> thumbnail_data(64, 0);
  std::string content_type("image/jpeg");

  auto* companion_helper =
      companion::CompanionTabHelper::FromWebContents(web_contents());
  companion_helper->ShowCompanionSidePanelForImage(
      src_url,
      /*is_image_translate=*/false,
      /*additional_query_params_modified=*/"", thumbnail_data, original_size,
      downscaled_size,
      /*image_extension=*/"", content_type);
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());

  WaitForCompanionToBeLoaded();
  EXPECT_EQ(side_panel_coordinator()->GetCurrentEntryId(),
            SidePanelEntry::Id::kSearchCompanion);

  // Close side panel and verify UKM.
  side_panel_coordinator()->Close();
  ExpectUkmEntry(&ukm_recorder,
                 ukm::builders::Companion_PageView::kOpenTriggerName,
                 static_cast<int>(SidePanelOpenTrigger::kLensContextMenu));
}

IN_PROC_BROWSER_TEST_F(CompanionPageBrowserTest, OpenedFromEntryPoint) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  // Load a page on the active tab.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), CreateUrl(kHost, kRelativeUrl1)));
  ASSERT_EQ(side_panel_coordinator()->GetCurrentEntryId(), absl::nullopt);

  // Open companion from entry point via dropdown.
  side_panel_coordinator()->Show(SidePanelEntry::Id::kSearchCompanion,
                                 SidePanelOpenTrigger::kComboboxSelected);
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());

  WaitForCompanionToBeLoaded();
  EXPECT_EQ(side_panel_coordinator()->GetCurrentEntryId(),
            SidePanelEntry::Id::kSearchCompanion);

  // Close side panel and verify UKM.
  side_panel_coordinator()->Close();
  ExpectUkmEntry(&ukm_recorder,
                 ukm::builders::Companion_PageView::kOpenTriggerName,
                 static_cast<int>(SidePanelOpenTrigger::kComboboxSelected));
}

IN_PROC_BROWSER_TEST_F(CompanionPageBrowserTest,
                       SubsequentContextMenuTextSearch) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  // Load a page on the active tab.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), CreateUrl(kHost, kRelativeUrl1)));
  ASSERT_EQ(side_panel_coordinator()->GetCurrentEntryId(), absl::nullopt);

  // Open companion from pinned entry point.
  side_panel_coordinator()->Show(
      SidePanelEntry::Id::kSearchCompanion,
      SidePanelOpenTrigger::kPinnedEntryToolbarButton);
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());

  WaitForCompanionToBeLoaded();
  EXPECT_EQ(side_panel_coordinator()->GetCurrentEntryId(),
            SidePanelEntry::Id::kSearchCompanion);

  // Start a text query via context menu.
  auto* companion_helper =
      companion::CompanionTabHelper::FromWebContents(web_contents());
  companion_helper->ShowCompanionSidePanelForSearchURL(GURL(kSearchQueryUrl));
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());

  EXPECT_EQ(side_panel_coordinator()->GetCurrentEntryId(),
            SidePanelEntry::Id::kSearchCompanion);

  // Close side panel and verify UKM.
  side_panel_coordinator()->Close();
  ExpectUkmEntry(
      &ukm_recorder, ukm::builders::Companion_PageView::kOpenTriggerName,
      static_cast<int>(SidePanelOpenTrigger::kPinnedEntryToolbarButton));
}

class CompanionPagePolicyBrowserTest : public CompanionPageBrowserTest {
 public:
  void EnableCompanionByPolicy(bool enable_companion_by_policy) {
    browser()->profile()->GetPrefs()->SetBoolean(
        prefs::kGoogleSearchSidePanelEnabled, enable_companion_by_policy);
  }
};

IN_PROC_BROWSER_TEST_F(CompanionPagePolicyBrowserTest,
                       SubsequentNavigationWithPolicyDefault) {
  EXPECT_TRUE(companion::IsCompanionFeatureEnabled());

  // Load a page on the active tab and open companion side panel
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), CreateUrl(kHost, kRelativeUrl1)));
  side_panel_coordinator()->Show(SidePanelEntry::Id::kSearchCompanion);

  WaitForCompanionToBeLoaded();
  EXPECT_EQ(side_panel_coordinator()->GetCurrentEntryId(),
            SidePanelEntry::Id::kSearchCompanion);
  EXPECT_EQ(1u, requests_received_on_server());
}

IN_PROC_BROWSER_TEST_F(
    CompanionPagePolicyBrowserTest,
    SubsequentNavigationWithPolicyEnabledFollowedbyDisabled) {
  EXPECT_TRUE(companion::IsCompanionFeatureEnabled());

  // Load a page on the active tab and open companion side panel
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), CreateUrl(kHost, kRelativeUrl1)));
  side_panel_coordinator()->Show(SidePanelEntry::Id::kSearchCompanion);

  WaitForCompanionToBeLoaded();
  EXPECT_EQ(side_panel_coordinator()->GetCurrentEntryId(),
            SidePanelEntry::Id::kSearchCompanion);
  EXPECT_EQ(1u, requests_received_on_server());

  // Disable companion by policy. CSC should not be shown anymore.
  EnableCompanionByPolicy(false);
  EXPECT_TRUE(companion::IsCompanionFeatureEnabled());
  WaitForMainPageToBeLoaded(kRelativeUrl2);
  side_panel_coordinator()->Show(SidePanelEntry::Id::kSearchCompanion);
  EXPECT_FALSE(side_panel_coordinator()->GetCurrentEntryId().has_value());
}

IN_PROC_BROWSER_TEST_F(CompanionPagePolicyBrowserTest,
                       PRE_SubsequentNavigationWithPolicyDisabled) {
  EnableCompanionByPolicy(false);
}

IN_PROC_BROWSER_TEST_F(CompanionPagePolicyBrowserTest,
                       SubsequentNavigationWithPolicyDisabled) {
  EXPECT_TRUE(companion::IsCompanionFeatureEnabled());
  // Load a page on the active tab and open companion side panel
  WaitForMainPageToBeLoaded(kRelativeUrl1);
  side_panel_coordinator()->Show(SidePanelEntry::Id::kSearchCompanion);

  EXPECT_FALSE(side_panel_coordinator()->GetCurrentEntryId().has_value());
  EXPECT_EQ(0u, requests_received_on_server());
}

IN_PROC_BROWSER_TEST_F(
    CompanionPagePolicyBrowserTest,
    PRE_SubsequentNavigationWithPolicyDisabledFollowedbyEnabled) {
  EnableCompanionByPolicy(false);
}
IN_PROC_BROWSER_TEST_F(
    CompanionPagePolicyBrowserTest,
    SubsequentNavigationWithPolicyDisabledFollowedbyEnabled) {
  // Load a page on the active tab and open companion side panel
  EXPECT_TRUE(companion::IsCompanionFeatureEnabled());
  WaitForMainPageToBeLoaded(kRelativeUrl1);
  side_panel_coordinator()->Show(SidePanelEntry::Id::kSearchCompanion);
  EXPECT_FALSE(side_panel_coordinator()->GetCurrentEntryId().has_value());
  EXPECT_EQ(0u, requests_received_on_server());

  // Enable companion by policy and that should enable the feature.
  EnableCompanionByPolicy(true);
  EXPECT_TRUE(companion::IsCompanionFeatureEnabled());
  // Load a page on the active tab and open companion side panel
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), CreateUrl(kHost, kRelativeUrl1)));
  side_panel_coordinator()->Show(SidePanelEntry::Id::kSearchCompanion);

  WaitForCompanionToBeLoaded();
  EXPECT_EQ(side_panel_coordinator()->GetCurrentEntryId(),
            SidePanelEntry::Id::kSearchCompanion);
  EXPECT_EQ(1u, requests_received_on_server());
}
