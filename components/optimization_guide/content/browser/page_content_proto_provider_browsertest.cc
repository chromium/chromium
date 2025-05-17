// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/page_content_proto_provider.h"

#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/test_future.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/common/features_generated.h"
#include "ui/display/display_switches.h"

namespace optimization_guide {

namespace {

// Allow 1px differences from rounding.
#define EXPECT_ALMOST_EQ(a, b) EXPECT_LE(abs(a - b), 1);

base::FilePath GetTestDataDir() {
  return base::FilePath(
      FILE_PATH_LITERAL("components/test/data/optimization_guide"));
}

void AssertHasText(const optimization_guide::proto::ContentNode& node,
                   std::string text) {
  EXPECT_EQ(node.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_ROOT);
  EXPECT_EQ(node.children_nodes().size(), 1);
  const auto& text_node = node.children_nodes().at(0);
  EXPECT_EQ(text_node.content_attributes().text_data().text_content(), text);
}

void AssertRectsEqual(const optimization_guide::proto::BoundingRect& proto_rect,
                      gfx::Rect rect) {
  EXPECT_EQ(proto_rect.width(), rect.width());
  EXPECT_EQ(proto_rect.height(), rect.height());
  EXPECT_EQ(proto_rect.x(), rect.x());
  EXPECT_EQ(proto_rect.y(), rect.y());
}

void AssertSizesEqual(const optimization_guide::proto::BoundingSize& proto_size,
                      gfx::Size size) {
  EXPECT_EQ(proto_size.width(), size.width());
  EXPECT_EQ(proto_size.height(), size.height());
}

void AssertRectsEqual(const optimization_guide::proto::BoundingRect& a,
                      const optimization_guide::proto::BoundingRect& b) {
  EXPECT_EQ(a.width(), b.width());
  EXPECT_EQ(a.height(), b.height());
  EXPECT_EQ(a.x(), b.x());
  EXPECT_EQ(a.y(), b.y());
}

void AssertValidOrigin(
    const optimization_guide::proto::SecurityOrigin& proto_origin,
    const url::Origin& expected) {
  EXPECT_EQ(proto_origin.opaque(), expected.opaque());

  if (expected.opaque()) {
    EXPECT_EQ(proto_origin.value(), expected.GetNonceForTesting()->ToString());
  } else {
    url::Origin actual = url::Origin::Create(GURL(proto_origin.value()));
    EXPECT_TRUE(actual.IsSameOriginWith(expected))
        << "actual: " << actual << ", expected: " << expected;
  }
}

blink::mojom::AIPageContentOptionsPtr GetAIPageContentOptions() {
  auto request = blink::mojom::AIPageContentOptions::New();
  request->include_geometry = true;
  request->on_critical_path = true;
  request->include_hidden_searchable_content = true;

  return request;
}

class PageContentProtoProviderBrowserTest : public content::ContentBrowserTest {
 public:
  PageContentProtoProviderBrowserTest() = default;

  PageContentProtoProviderBrowserTest(
      const PageContentProtoProviderBrowserTest&) = delete;
  PageContentProtoProviderBrowserTest& operator=(
      const PageContentProtoProviderBrowserTest&) = delete;

  ~PageContentProtoProviderBrowserTest() override = default;

  content::WebContents* web_contents() { return shell()->web_contents(); }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");

    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->AddDefaultHandlers(GetTestDataDir());
    content::SetupCrossSiteRedirector(https_server_.get());

    ASSERT_TRUE(https_server_->Start());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    content::ContentBrowserTest::SetUpCommandLine(command_line);

    // HTTPS server only serves a valid cert for localhost, so this is needed
    // to load pages from other hosts without an error.
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);

    command_line->AppendSwitchASCII(switches::kForceDeviceScaleFactor, "1.0");
  }

  void SetPageContent(base::OnceClosure quit_closure,
                      std::optional<AIPageContentResult> page_content) {
    page_content_ = std::move(page_content->proto);
    metadata_ = std::move(page_content->metadata);
    document_identifiers_ = std::move(page_content->document_identifiers);
    std::move(quit_closure).Run();
  }

  const proto::AnnotatedPageContent& page_content() { return *page_content_; }
  const optimization_guide::mojom::PageMetadata& metadata() {
    return *metadata_;
  }
  const base::flat_map<std::string, content::WeakDocumentPtr>&
  document_identifiers() {
    return document_identifiers_;
  }

  void LoadData(blink::mojom::AIPageContentOptionsPtr request =
                    GetAIPageContentOptions()) {
    base::RunLoop run_loop;
    GetAIPageContent(
        web_contents(), std::move(request),
        base::BindOnce(&PageContentProtoProviderBrowserTest::SetPageContent,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    CHECK(page_content_);
  }

  void LoadPage(GURL url, bool with_page_content = true) {
    content::NavigateToURLBlockUntilNavigationsComplete(web_contents(), url, 1);

    {
      base::test::TestFuture<bool> future;
      web_contents()
          ->GetPrimaryMainFrame()
          ->GetRenderWidgetHost()
          ->InsertVisualStateCallback(future.GetCallback());
      ASSERT_TRUE(future.Wait()) << "Timeout waiting for syncing with renderer";
    }

    if (with_page_content) {
      LoadData();
    }
  }

  void SelectTextInBody(content::RenderFrameHost* rfh) {
    const std::string kSelectText =
        "window.getSelection().selectAllChildren(document.body);";
    ASSERT_TRUE(content::ExecJs(rfh, kSelectText));
  }

  net::EmbeddedTestServer* https_server() { return https_server_.get(); }

 private:
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  std::optional<proto::AnnotatedPageContent> page_content_;
  optimization_guide::mojom::PageMetadataPtr metadata_;
  base::flat_map<std::string, content::WeakDocumentPtr> document_identifiers_;
};

IN_PROC_BROWSER_TEST_F(PageContentProtoProviderBrowserTest, AIPageContent) {
  const gfx::Size window_bounds(web_contents()->GetSize());
  LoadPage(https_server()->GetURL("/simple.html"));

  EXPECT_EQ(page_content().version(),
            optimization_guide::proto::ANNOTATED_PAGE_CONTENT_VERSION_1_0);
  EXPECT_EQ(page_content().root_node().children_nodes().size(), 1);
  AssertHasText(page_content().root_node(), "Non empty simple page\n\n");
  EXPECT_FALSE(
      page_content().root_node().content_attributes().has_interaction_info());

  const auto& root_geometry =
      page_content().root_node().content_attributes().geometry();
  EXPECT_EQ(root_geometry.outer_bounding_box().x(), 0);
  EXPECT_EQ(root_geometry.outer_bounding_box().y(), 0);
  EXPECT_EQ(root_geometry.outer_bounding_box().width(), window_bounds.width());
  EXPECT_EQ(root_geometry.outer_bounding_box().height(),
            window_bounds.height());

  EXPECT_EQ(root_geometry.visible_bounding_box().x(), 0);
  EXPECT_EQ(root_geometry.visible_bounding_box().y(), 0);
  EXPECT_EQ(root_geometry.visible_bounding_box().width(),
            window_bounds.width());
  EXPECT_EQ(root_geometry.visible_bounding_box().height(),
            window_bounds.height());

  EXPECT_EQ(page_content().viewport_geometry().x(), 0);
  EXPECT_EQ(page_content().viewport_geometry().y(), 0);
  EXPECT_EQ(page_content().viewport_geometry().width(), window_bounds.width());
  EXPECT_EQ(page_content().viewport_geometry().height(),
            window_bounds.height());
}

IN_PROC_BROWSER_TEST_F(PageContentProtoProviderBrowserTest, Selection) {
  LoadPage(https_server()->GetURL("/simple.html"), false);
  SelectTextInBody(web_contents()->GetPrimaryMainFrame());
  LoadData();

  const auto& selection =
      page_content().main_frame_data().frame_interaction_info().selection();
  EXPECT_NE(selection.start_node_id(), 0);
  EXPECT_NE(selection.end_node_id(), 0);
  EXPECT_EQ(selection.selected_text(), "Non empty simple page");
}

IN_PROC_BROWSER_TEST_F(PageContentProtoProviderBrowserTest,
                       RelativePathOnIframe) {
  LoadPage(https_server()->GetURL("a.com", "/relative_path.html"));

  const auto& main_frame_origin =
      page_content().main_frame_data().security_origin();
  const auto& iframe_origin = page_content()
                                  .root_node()
                                  .children_nodes()[0]
                                  .content_attributes()
                                  .iframe_data()
                                  .frame_data()
                                  .security_origin();
  AssertValidOrigin(
      main_frame_origin,
      web_contents()->GetPrimaryMainFrame()->GetLastCommittedOrigin());
  EXPECT_EQ(main_frame_origin.opaque(), iframe_origin.opaque());
  EXPECT_EQ(main_frame_origin.value(), iframe_origin.value());
}

IN_PROC_BROWSER_TEST_F(PageContentProtoProviderBrowserTest, ScrollerInfo) {
  const gfx::Size window_bounds(web_contents()->GetSize());

  LoadPage(https_server()->GetURL("a.com", "/scroller.html"));

  const auto& root = page_content().root_node();
  EXPECT_TRUE(root.content_attributes().has_interaction_info());
  EXPECT_TRUE(root.content_attributes().interaction_info().has_scroller_info());

  const auto& root_scroller =
      root.content_attributes().interaction_info().scroller_info();
  AssertSizesEqual(
      root_scroller.scrolling_bounds(),
      gfx::Size(window_bounds.width() + 50, window_bounds.height() + 30));
  AssertRectsEqual(root_scroller.visible_area(), gfx::Rect(window_bounds));

  ASSERT_EQ(root.children_nodes().size(), 1);
  const auto& child = root.children_nodes().at(0);
  EXPECT_TRUE(child.content_attributes().has_interaction_info());
  EXPECT_TRUE(
      child.content_attributes().interaction_info().has_scroller_info());

  const auto& sub_scroller =
      child.content_attributes().interaction_info().scroller_info();
  AssertSizesEqual(
      sub_scroller.scrolling_bounds(),
      gfx::Size(2 * window_bounds.width(), 3 * window_bounds.height()));
  AssertRectsEqual(
      sub_scroller.visible_area(),
      gfx::Rect(200, 100, window_bounds.width(), window_bounds.height()));

  EXPECT_TRUE(sub_scroller.user_scrollable_horizontal());
  EXPECT_TRUE(sub_scroller.user_scrollable_vertical());
}

class PageContentProtoProviderBrowserTestActionableElements
    : public PageContentProtoProviderBrowserTest {
 public:
  PageContentProtoProviderBrowserTestActionableElements()
      : features_(features::kAnnotatedPageContentWithActionableElements) {}

  const optimization_guide::proto::ContentNode& ContentRootNode() {
    EXPECT_EQ(page_content().root_node().children_nodes().size(), 1);
    const auto& html = page_content().root_node().children_nodes().at(0);

    EXPECT_EQ(html.children_nodes().size(), 1);
    const auto& body = html.children_nodes().at(0);

    return body;
  }

 private:
  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_F(PageContentProtoProviderBrowserTestActionableElements,
                       AIPageContent) {
  LoadPage(https_server()->GetURL("/actionable_elements.html"));
  EXPECT_EQ(page_content().version(),
            optimization_guide::proto::
                ANNOTATED_PAGE_CONTENT_VERSION_ONLY_ACTIONABLE_ELEMENTS_1_0);
  EXPECT_EQ(page_content().root_node().children_nodes().size(), 1);
  const auto& child = page_content().root_node().children_nodes().at(0);
  EXPECT_TRUE(child.content_attributes().has_interaction_info());
}

IN_PROC_BROWSER_TEST_F(PageContentProtoProviderBrowserTestActionableElements,
                       ForLabel) {
  LoadPage(https_server()->GetURL("/for_label.html"));
  EXPECT_EQ(page_content().version(),
            optimization_guide::proto::
                ANNOTATED_PAGE_CONTENT_VERSION_ONLY_ACTIONABLE_ELEMENTS_1_0);

  EXPECT_EQ(ContentRootNode().children_nodes().size(), 2);

  const auto& input = ContentRootNode().children_nodes()[0];
  ASSERT_TRUE(input.content_attributes().has_interaction_info());
  EXPECT_TRUE(input.content_attributes().interaction_info().is_clickable());

  const auto& label = ContentRootNode().children_nodes()[1];
  ASSERT_TRUE(label.content_attributes().has_interaction_info());
  EXPECT_TRUE(label.content_attributes().interaction_info().is_clickable());
  EXPECT_EQ(label.content_attributes().label_for_dom_node_id(),
            input.content_attributes().common_ancestor_dom_node_id());
}

IN_PROC_BROWSER_TEST_F(PageContentProtoProviderBrowserTestActionableElements,
                       LabelNotActionable) {
  LoadPage(https_server()->GetURL("/label_not_actionable.html"));
  EXPECT_EQ(page_content().version(),
            optimization_guide::proto::
                ANNOTATED_PAGE_CONTENT_VERSION_ONLY_ACTIONABLE_ELEMENTS_1_0);

  EXPECT_EQ(ContentRootNode().children_nodes().size(), 2);

  const auto& input = ContentRootNode().children_nodes()[0];
  ASSERT_TRUE(input.content_attributes().has_interaction_info());
  EXPECT_TRUE(input.content_attributes().interaction_info().is_clickable());

  const auto& label = ContentRootNode().children_nodes()[1];
  EXPECT_FALSE(label.content_attributes().has_interaction_info());
  EXPECT_EQ(label.content_attributes().label_for_dom_node_id(),
            input.content_attributes().common_ancestor_dom_node_id());
}

IN_PROC_BROWSER_TEST_F(PageContentProtoProviderBrowserTestActionableElements,
                       AriaRole) {
  LoadPage(https_server()->GetURL("/aria_role.html"));
  EXPECT_EQ(page_content().version(),
            optimization_guide::proto::
                ANNOTATED_PAGE_CONTENT_VERSION_ONLY_ACTIONABLE_ELEMENTS_1_0);

  EXPECT_EQ(ContentRootNode().children_nodes().size(), 1);
  const auto& button = ContentRootNode().children_nodes()[0];
  ASSERT_TRUE(button.content_attributes().has_interaction_info());
  EXPECT_TRUE(button.content_attributes().interaction_info().is_clickable());
  EXPECT_EQ(button.content_attributes().aria_role(),
            optimization_guide::proto::AXRole::AX_ROLE_BUTTON);
}

IN_PROC_BROWSER_TEST_F(PageContentProtoProviderBrowserTestActionableElements,
                       ZOrder) {
  LoadPage(https_server()->GetURL("/simple.html"));

  EXPECT_EQ(page_content()
                .root_node()
                .content_attributes()
                .interaction_info()
                .document_scoped_z_order(),
            1);
}

IN_PROC_BROWSER_TEST_F(PageContentProtoProviderBrowserTest,
                       AIPageContentNoGeometry) {
  LoadPage(https_server()->GetURL("/simple.html"),
           /* with_page_content = */ false);

  auto request = blink::mojom::AIPageContentOptions::New();
  request->include_geometry = false;
  LoadData(std::move(request));

  EXPECT_EQ(page_content().root_node().children_nodes().size(), 1);
  AssertHasText(page_content().root_node(), "Non empty simple page\n\n");
  EXPECT_FALSE(page_content().root_node().content_attributes().has_geometry());
}

IN_PROC_BROWSER_TEST_F(PageContentProtoProviderBrowserTest,
                       AIPageContentNoCriticalPath) {
  LoadPage(https_server()->GetURL("/simple.html"),
           /* with_page_content = */ false);

  auto request = blink::mojom::AIPageContentOptions::New();
  request->on_critical_path = false;
  request->include_geometry = true;
  LoadData(std::move(request));

  EXPECT_EQ(page_content().root_node().children_nodes().size(), 1);
  AssertHasText(page_content().root_node(), "Non empty simple page\n\n");
  EXPECT_TRUE(page_content().root_node().content_attributes().has_geometry());
}

IN_PROC_BROWSER_TEST_F(PageContentProtoProviderBrowserTest,
                       AIPageContentImageDataURL) {
  LoadPage(https_server()->GetURL("a.com", "/data_image.html"));

  EXPECT_EQ(page_content().root_node().children_nodes().size(), 1);
  const auto& image_node = page_content().root_node().children_nodes().at(0);

  ASSERT_TRUE(image_node.content_attributes().has_image_data());
  const auto& image_data = image_node.content_attributes().image_data();
  // TODO(crbug.com/382558422): Propagate image source URLs, this should be
  // a.com.
  EXPECT_TRUE(image_data.security_origin().value().empty());
}

IN_PROC_BROWSER_TEST_F(PageContentProtoProviderBrowserTest, SVG) {
  LoadPage(https_server()->GetURL("/svg.html"));

  EXPECT_EQ(page_content().root_node().children_nodes().size(), 1);

  const auto& svg = page_content().root_node().children_nodes().at(0);
  ASSERT_TRUE(svg.content_attributes().has_svg_data());
  EXPECT_EQ(svg.content_attributes().svg_data().inner_text(),
            "Hello SVG Text!");
}

IN_PROC_BROWSER_TEST_F(PageContentProtoProviderBrowserTest, Canvas) {
  LoadPage(https_server()->GetURL("/canvas.html"));

  EXPECT_EQ(page_content().root_node().children_nodes().size(), 1);

  const auto& canvas = page_content().root_node().children_nodes().at(0);
  ASSERT_TRUE(canvas.content_attributes().has_canvas_data());
  EXPECT_EQ(canvas.content_attributes().canvas_data().layout_width(), 200);
  EXPECT_EQ(canvas.content_attributes().canvas_data().layout_height(), 300);
}

namespace {

std::string GetFilePathWithHostAndPortReplacement(
    const std::string& original_file_path,
    const net::HostPortPair& host_port_pair) {
  base::StringPairs replacement_text;
  replacement_text.push_back(
      make_pair("REPLACE_WITH_HOST_AND_PORT", host_port_pair.ToString()));
  return net::test_server::GetFilePathWithReplacements(original_file_path,
                                                       replacement_text);
}

}  // namespace

IN_PROC_BROWSER_TEST_F(PageContentProtoProviderBrowserTest,
                       AIPageContentCrossOriginImage) {
  // Add a "replace_text=" query param that the test server will use to replace
  // the string "REPLACE_WITH_HOST_AND_PORT" in the destination page.
  net::HostPortPair host_port_pair =
      net::HostPortPair::FromURL(https_server()->GetURL("b.com", "/"));
  std::string replacement_path = GetFilePathWithHostAndPortReplacement(
      "/cross_origin_image.html", host_port_pair);

  LoadPage(https_server()->GetURL("a.com", replacement_path));

  EXPECT_EQ(page_content().root_node().children_nodes().size(), 1);
  const auto& image_node = page_content().root_node().children_nodes().at(0);

  ASSERT_TRUE(image_node.content_attributes().has_image_data());
  const auto& image_data = image_node.content_attributes().image_data();
  // TODO(crbug.com/382558422): Propagate image source URLs, this should be
  // b.com.
  EXPECT_TRUE(image_data.security_origin().value().empty());
}

IN_PROC_BROWSER_TEST_F(PageContentProtoProviderBrowserTest,
                       AIPageContentSandboxedIframe) {
  LoadPage(https_server()->GetURL("a.com", "/paragraph_iframe_sandbox.html"));

  EXPECT_EQ(page_content().root_node().children_nodes().size(), 1);

  const auto& iframe = page_content().root_node().children_nodes()[0];
  EXPECT_EQ(iframe.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME);
  const auto& iframe_data = iframe.content_attributes().iframe_data();
  AssertValidOrigin(iframe_data.frame_data().security_origin(),
                    ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0)
                        ->GetLastCommittedOrigin());
  EXPECT_FALSE(iframe_data.likely_ad_frame());

  EXPECT_EQ(iframe.children_nodes().size(), 1);
}

IN_PROC_BROWSER_TEST_F(PageContentProtoProviderBrowserTest,
                       AIPageContentIframeDataURL) {
  LoadPage(https_server()->GetURL("a.com", "/paragraph_iframe_data_url.html"));

  EXPECT_EQ(page_content().root_node().children_nodes().size(), 1);

  const auto& iframe = page_content().root_node().children_nodes()[0];
  EXPECT_EQ(iframe.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME);
  const auto& iframe_data = iframe.content_attributes().iframe_data();
  AssertValidOrigin(iframe_data.frame_data().security_origin(),
                    ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0)
                        ->GetLastCommittedOrigin());
  EXPECT_FALSE(iframe_data.likely_ad_frame());

  EXPECT_EQ(iframe.children_nodes().size(), 1);
}

class PageContentProtoProviderBrowserTestSiteIsolation
    : public PageContentProtoProviderBrowserTest,
      public ::testing::WithParamInterface<bool> {
 public:
  bool EnableCrossSiteFrames() const { return GetParam(); }

  std::string QueryParam() const {
    return EnableCrossSiteFrames() ? "?domain=/cross-site/b.com/" : "";
  }
};

IN_PROC_BROWSER_TEST_P(PageContentProtoProviderBrowserTestSiteIsolation,
                       LatencyMetrics) {
  base::HistogramTester tester;

  LoadPage(https_server()->GetURL(
      "a.com",
      base::StringPrintf("/paragraph_iframe_partially_offscreen.html%s",
                         QueryParam())));
  ASSERT_EQ(page_content().root_node().children_nodes().size(), 1);
  content::FetchHistogramsFromChildProcesses();

  constexpr char kMainFrame[] =
      "OptimizationGuide.AIPageContent.RendererLatency.MainFrame";
  constexpr char kMainFrameSchedulingDelay[] =
      "OptimizationGuide.AIPageContent.RendererLatencyWithSchedulingDelay."
      "Critical.MainFrame";
  constexpr char kRemoteSubframe[] =
      "OptimizationGuide.AIPageContent.RendererLatency.RemoteSubFrame";
  constexpr char kRemoteSubframeSchedulingDelay[] =
      "OptimizationGuide.AIPageContent.RendererLatencyWithSchedulingDelay."
      "Critical.RemoteSubFrame";
  constexpr char kTotal[] = "OptimizationGuide.AIPageContent.TotalLatency";

  tester.ExpectTotalCount(kMainFrame, 1);
  tester.ExpectTotalCount(kMainFrameSchedulingDelay, 1);
  tester.ExpectTotalCount(kTotal, 1);

#if BUILDFLAG(IS_ANDROID)
  // TODO(crbug.com/384585933): Enable this assert on Android.
  if (EnableCrossSiteFrames()) {
    return;
  }
#endif
  tester.ExpectTotalCount(kRemoteSubframe, EnableCrossSiteFrames() ? 1 : 0);
  tester.ExpectTotalCount(kRemoteSubframeSchedulingDelay,
                          EnableCrossSiteFrames() ? 1 : 0);
}

IN_PROC_BROWSER_TEST_P(PageContentProtoProviderBrowserTestSiteIsolation,
                       LatencyMetricsNotOnCriticalPath) {
  base::HistogramTester tester;


  LoadPage(https_server()->GetURL(
               "a.com", base::StringPrintf(
                            "/paragraph_iframe_partially_offscreen.html%s",
                            QueryParam())),
           /* with_page_content = */ false);

  auto request = optimization_guide::DefaultAIPageContentOptions();
  request->on_critical_path = false;
  LoadData(std::move(request));
  content::FetchHistogramsFromChildProcesses();


  ASSERT_EQ(page_content().root_node().children_nodes().size(), 1);

  constexpr char kMainFrame[] =
      "OptimizationGuide.AIPageContent.RendererLatency.MainFrame";
  constexpr char kMainFrameSchedulingDelay[] =
      "OptimizationGuide.AIPageContent.RendererLatencyWithSchedulingDelay."
      "NonCritical.MainFrame";
  constexpr char kRemoteSubframe[] =
      "OptimizationGuide.AIPageContent.RendererLatency.RemoteSubFrame";
  constexpr char kRemoteSubframeSchedulingDelay[] =
      "OptimizationGuide.AIPageContent.RendererLatencyWithSchedulingDelay."
      "Critical.RemoteSubFrame";
  constexpr char kRemoteSubframeSchedulingDelayNonCritical[] =
      "OptimizationGuide.AIPageContent.RendererLatencyWithSchedulingDelay."
      "NonCritical.RemoteSubFrame";
  constexpr char kTotal[] = "OptimizationGuide.AIPageContent.TotalLatency";

  tester.ExpectTotalCount(kMainFrame, 1);
  tester.ExpectTotalCount(kMainFrameSchedulingDelay, 1);
  tester.ExpectTotalCount(kTotal, 1);

#if BUILDFLAG(IS_ANDROID)
  // TODO(crbug.com/384585933): Enable this assert on Android.
  if (EnableCrossSiteFrames()) {
    return;
  }
#endif
  // TODO(crbug.com/389737599): We should have a metric for subframes once they
  // can use off critical path scheduling.
  tester.ExpectTotalCount(kRemoteSubframeSchedulingDelayNonCritical, 0);
  tester.ExpectTotalCount(kRemoteSubframe, EnableCrossSiteFrames() ? 1 : 0);
  tester.ExpectTotalCount(kRemoteSubframeSchedulingDelay,
                          EnableCrossSiteFrames() ? 1 : 0);
}

// Ensure that clip from an ancestor frame is included in visible rect
// computation.
IN_PROC_BROWSER_TEST_P(PageContentProtoProviderBrowserTestSiteIsolation,
                       AIPageContentIframePartiallyOffscreen) {
  LoadPage(https_server()->GetURL(
      "a.com",
      base::StringPrintf("/paragraph_iframe_partially_offscreen.html%s",
                         QueryParam())));
  ASSERT_EQ(page_content().root_node().children_nodes().size(), 1);

  const auto& iframe = page_content().root_node().children_nodes()[0];
  ASSERT_EQ(iframe.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME);

  ASSERT_EQ(iframe.children_nodes().size(), 1);
  const auto& iframe_root = iframe.children_nodes()[0];
  ASSERT_EQ(iframe_root.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_ROOT);

  ASSERT_EQ(iframe_root.children_nodes().size(), 1);
  const auto& p = iframe_root.children_nodes()[0];
  EXPECT_EQ(p.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_PARAGRAPH);
  EXPECT_EQ(p.content_attributes().annotated_roles().size(), 0);
  const auto& geometry = p.content_attributes().geometry();
  AssertRectsEqual(geometry.outer_bounding_box(),
                   gfx::Rect(-20, -10, 100, 200));
  AssertRectsEqual(geometry.visible_bounding_box(), gfx::Rect(0, 0, 80, 190));
}

// Ensure that clip from an ancestor frame's root scroller are included in
// visible rect computation.
IN_PROC_BROWSER_TEST_P(
    PageContentProtoProviderBrowserTestSiteIsolation,
    AIPageContentIframePartiallyOffscreenAncestorRootScroller) {
  LoadPage(https_server()->GetURL(
      "a.com", base::StringPrintf(
                   "/paragraph_iframe_partially_scrolled_offscreen.html%s",
                   QueryParam())));

  ASSERT_EQ(page_content().root_node().children_nodes().size(), 1);

  const auto& iframe = page_content().root_node().children_nodes()[0];
  ASSERT_EQ(iframe.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME);

  ASSERT_EQ(iframe.children_nodes().size(), 1);
  const auto& iframe_root = iframe.children_nodes()[0];
  ASSERT_EQ(iframe_root.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_ROOT);

  const auto& p = iframe_root.children_nodes()[0];
  EXPECT_EQ(p.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_PARAGRAPH);
  EXPECT_EQ(p.content_attributes().annotated_roles().size(), 0);

// TODO(khushalsagar): This is an existing bug where the scroll offset of the
// root scroller in the ancestor remote frame is not applied.
#if !BUILDFLAG(IS_ANDROID)
  const auto& geometry = p.content_attributes().geometry();
  AssertRectsEqual(geometry.outer_bounding_box(),
                   gfx::Rect(-20, -10, 100, 200));
  AssertRectsEqual(geometry.visible_bounding_box(), gfx::Rect(0, 0, 80, 190));
#endif
}

IN_PROC_BROWSER_TEST_P(PageContentProtoProviderBrowserTestSiteIsolation,
                       Selection) {
  LoadPage(https_server()->GetURL(
               "a.com", base::StringPrintf(
                            "/paragraph_iframe_partially_offscreen.html%s",
                            QueryParam())),
           false);

  SelectTextInBody(ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0));
  LoadData();

  const auto& iframe = page_content().root_node().children_nodes()[0];
  const auto& selection = iframe.content_attributes()
                              .iframe_data()
                              .frame_data()
                              .frame_interaction_info()
                              .selection();
  EXPECT_NE(selection.start_node_id(), 0);
  EXPECT_NE(selection.end_node_id(), 0);
  EXPECT_EQ(selection.selected_text(), "text");
}

INSTANTIATE_TEST_SUITE_P(All,
                         PageContentProtoProviderBrowserTestSiteIsolation,
                         testing::Bool());

class PageContentProtoProviderBrowserTestMultiProcess
    : public PageContentProtoProviderBrowserTest,
      public ::testing::WithParamInterface<bool> {
 public:
  bool EnableProcessIsolation() const { return GetParam(); }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    PageContentProtoProviderBrowserTest::SetUpCommandLine(command_line);

    if (EnableProcessIsolation()) {
      content::IsolateAllSitesForTesting(command_line);
    } else {
      // TODO(khushalsagar): Enable tests which force a single renderer process
      // for all frames.
      // content::RenderProcessHost::SetMaxRendererProcessCount(1) is not
      // sufficient for that.
      GTEST_SKIP();
    }
  }

 protected:
  content::test::FencedFrameTestHelper fenced_frame_helper_;
};

IN_PROC_BROWSER_TEST_P(PageContentProtoProviderBrowserTestMultiProcess,
                       AIPageContentMultipleCrossSiteFrames) {
  LoadPage(https_server()->GetURL("a.com", "/iframe_cross_site.html"));

  EXPECT_EQ(page_content().root_node().children_nodes().size(), 2);

  const auto& b_frame = page_content().root_node().children_nodes()[0];
  EXPECT_EQ(b_frame.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME);
  const auto& b_frame_data = b_frame.content_attributes().iframe_data();
  AssertValidOrigin(b_frame_data.frame_data().security_origin(),
                    ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0)
                        ->GetLastCommittedOrigin());
  EXPECT_FALSE(b_frame_data.likely_ad_frame());

  EXPECT_EQ(b_frame.children_nodes().size(), 1);
  AssertHasText(b_frame.children_nodes()[0], "This page has no title.\n\n");
  const auto& b_geometry = b_frame.content_attributes().geometry();
  AssertRectsEqual(b_geometry.outer_bounding_box(),
                   b_geometry.visible_bounding_box());

  const auto& c_frame = page_content().root_node().children_nodes()[1];
  EXPECT_EQ(c_frame.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME);
  const auto& c_frame_data = c_frame.content_attributes().iframe_data();
  AssertValidOrigin(c_frame_data.frame_data().security_origin(),
                    ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 1)
                        ->GetLastCommittedOrigin());
  EXPECT_FALSE(c_frame_data.likely_ad_frame());
  EXPECT_EQ(b_frame.children_nodes().size(), 1);
  AssertHasText(c_frame.children_nodes()[0], "This page has no title.\n\n");
  const auto& c_geometry = c_frame.content_attributes().geometry();
  AssertRectsEqual(c_geometry.outer_bounding_box(),
                   c_geometry.visible_bounding_box());

  EXPECT_ALMOST_EQ(b_geometry.outer_bounding_box().width(),
                   c_geometry.outer_bounding_box().width());
  EXPECT_ALMOST_EQ(b_geometry.outer_bounding_box().height(),
                   c_geometry.outer_bounding_box().height());
  EXPECT_ALMOST_EQ(b_geometry.outer_bounding_box().y(),
                   c_geometry.outer_bounding_box().y());
  EXPECT_NE(b_geometry.outer_bounding_box().x(),
            c_geometry.outer_bounding_box().x());
}

INSTANTIATE_TEST_SUITE_P(All,
                         PageContentProtoProviderBrowserTestMultiProcess,
                         testing::Bool());

class PageContentProtoProviderBrowserTestFencedFrame
    : public PageContentProtoProviderBrowserTest {
 protected:
  content::test::FencedFrameTestHelper fenced_frame_helper_;
};

IN_PROC_BROWSER_TEST_F(PageContentProtoProviderBrowserTestFencedFrame,
                       AIPageContentFencedFrame) {
  LoadPage(https_server()->GetURL("a.com", "/fenced_frame/basic.html"),
           /* with_page_content = */ false);

  const GURL fenced_frame_url =
      https_server()->GetURL("b.com", "/fenced_frame/simple.html");
  auto* fenced_frame_rfh = fenced_frame_helper_.CreateFencedFrame(
      web_contents()->GetPrimaryMainFrame(), fenced_frame_url);
  ASSERT_NE(nullptr, fenced_frame_rfh);
  LoadData();

  EXPECT_EQ(page_content().root_node().children_nodes().size(), 1);

  const auto& b_frame = page_content().root_node().children_nodes()[0];
  EXPECT_EQ(b_frame.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME);
  const auto& b_frame_data = b_frame.content_attributes().iframe_data();
  AssertValidOrigin(b_frame_data.frame_data().security_origin(),
                    fenced_frame_rfh->GetLastCommittedOrigin());
  EXPECT_FALSE(b_frame_data.likely_ad_frame());
  EXPECT_EQ(b_frame.children_nodes().size(), 1);
  AssertHasText(b_frame.children_nodes()[0], "Non empty simple page\n\n");
  const auto& b_geometry = b_frame.content_attributes().geometry();
  AssertRectsEqual(b_geometry.outer_bounding_box(),
                   b_geometry.visible_bounding_box());
}

IN_PROC_BROWSER_TEST_P(PageContentProtoProviderBrowserTestMultiProcess,
                       AIPageContentMetadata) {
  // TODO(crbug.com/403325367) When remote frames are supported, this same test
  // should also work with cross-site iframes ("/iframe_cross_site.html").
  LoadPage(https_server()->GetURL("a.com", "/iframe_same_site.html"),
           /* with_page_content = */ false);

  auto options = blink::mojom::AIPageContentOptions::New();
  options->max_meta_elements = 32;
  LoadData(std::move(options));

  EXPECT_EQ(metadata().frame_metadata.size(), 3u);

  const auto& main_frame_metadata = metadata().frame_metadata[0];
  EXPECT_EQ(main_frame_metadata->url.host(), "a.com");
  EXPECT_EQ(main_frame_metadata->meta_tags.size(), 1u);
  EXPECT_EQ(main_frame_metadata->meta_tags[0]->name, "author");
  EXPECT_EQ(main_frame_metadata->meta_tags[0]->content, "George");

  const auto& child_frame_metadata1 = metadata().frame_metadata[1];
  EXPECT_EQ(child_frame_metadata1->url.host(), "a.com");
  EXPECT_EQ(child_frame_metadata1->meta_tags.size(), 1u);
  EXPECT_EQ(child_frame_metadata1->meta_tags[0]->name, "author");
  EXPECT_EQ(child_frame_metadata1->meta_tags[0]->content, "Gary");

  const auto& child_frame_metadata2 = metadata().frame_metadata[2];
  EXPECT_EQ(child_frame_metadata2->url.host(), "a.com");
  EXPECT_EQ(child_frame_metadata2->meta_tags.size(), 1u);
  EXPECT_EQ(child_frame_metadata2->meta_tags[0]->name, "author");
  EXPECT_EQ(child_frame_metadata2->meta_tags[0]->content, "Gary");
}

IN_PROC_BROWSER_TEST_P(PageContentProtoProviderBrowserTestMultiProcess,
                       AIPageContentFrameIdentifiersTheSame) {
  LoadPage(https_server()->GetURL("a.com", "/fenced_frame/basic.html"),
           /* with_page_content = */ false);

  const GURL fenced_frame_url =
      https_server()->GetURL("b.com", "/fenced_frame/simple.html");
  auto* fenced_frame_rfh = fenced_frame_helper_.CreateFencedFrame(
      web_contents()->GetPrimaryMainFrame(), fenced_frame_url);
  ASSERT_NE(nullptr, fenced_frame_rfh);
  LoadData();
  auto document_identifiers_1 = document_identifiers();
  LoadData();
  auto document_identifiers_2 = document_identifiers();
  EXPECT_EQ(2u, document_identifiers_1.size());
  EXPECT_EQ(2u, document_identifiers_2.size());
  for (const auto& [document_identifier_key, doc_ptr] :
       document_identifiers_1) {
    EXPECT_NE(document_identifiers_2.end(),
              document_identifiers_2.find(document_identifier_key));
    EXPECT_NE(nullptr, doc_ptr.AsRenderFrameHostIfValid());
    EXPECT_EQ(doc_ptr.AsRenderFrameHostIfValid(),
              document_identifiers_2[document_identifier_key]
                  .AsRenderFrameHostIfValid());
  }
}

int TreeDepth(const optimization_guide::proto::ContentNode& node) {
  int depth = 0;
  for (const auto& child : node.children_nodes()) {
    depth = std::max(depth, TreeDepth(child));
  }
  return depth + 1;
}

IN_PROC_BROWSER_TEST_P(PageContentProtoProviderBrowserTestMultiProcess,
                       DeepTree) {
  // Listen for ukm metrics.
  base::test::TestFuture<void> future;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  ukm_recorder.SetOnAddEntryCallback(
      ukm::builders::OptimizationGuide_AIPageContentAgent::kEntryName,
      future.GetRepeatingCallback());

  LoadPage(https_server()->GetURL("/deep.html"));

  // deep.html has a tree depth of 202.  Expect mojo encoding to trim to less
  // than mojo's kMaxRecursionDepth of 200.
  EXPECT_LT(TreeDepth(page_content().root_node()), 200);

  // Ensure a ukm metric was recorded.
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuide_AIPageContentAgent::kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0].get();
  EXPECT_EQ(1, *ukm_recorder.GetEntryMetric(
                   entry, ukm::builders::OptimizationGuide_AIPageContentAgent::
                              kNodeDepthLimitExceededName));
}

IN_PROC_BROWSER_TEST_P(PageContentProtoProviderBrowserTestMultiProcess,
                       DeepSparseTree) {
  // Listen for ukm metrics.
  base::test::TestFuture<void> future;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  ukm_recorder.SetOnAddEntryCallback(
      ukm::builders::OptimizationGuide_AIPageContentAgent::kEntryName,
      future.GetRepeatingCallback());

  LoadPage(https_server()->GetURL("/deep_sparse.html"));

  // deep_sparse.html has a dom tree depth of 202. Every other DIV is one that
  // will be skipped and not included in the mojo encoding.  If depth counting
  // is working properly, the limit should not be reached and the encoded depth
  // should be 103 (one for each unskipped div plus root and attributes).
  EXPECT_EQ(TreeDepth(page_content().root_node()), 103);

  // Ensure that no ukm metric was recorded.
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuide_AIPageContentAgent::kEntryName);
  EXPECT_EQ(0u, entries.size());
}

class ScaledPageContentProtoProviderBrowserTest
    : public PageContentProtoProviderBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    content::ContentBrowserTest::SetUpCommandLine(command_line);

    // HTTPS server only serves a valid cert for localhost, so this is needed
    // to load pages from other hosts without an error.
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);

    command_line->AppendSwitchASCII(switches::kForceDeviceScaleFactor, "2.0");
  }
};

IN_PROC_BROWSER_TEST_F(ScaledPageContentProtoProviderBrowserTest, ScaleSizes) {
  const gfx::Size window_bounds(web_contents()->GetSize());
  LoadPage(https_server()->GetURL("/simple.html"));

  EXPECT_EQ(page_content().version(),
            optimization_guide::proto::ANNOTATED_PAGE_CONTENT_VERSION_1_0);
  EXPECT_EQ(page_content().root_node().children_nodes().size(), 1);
  AssertHasText(page_content().root_node(), "Non empty simple page\n\n");
  EXPECT_FALSE(
      page_content().root_node().content_attributes().has_interaction_info());

  // The viewport geometry should be scaled by the device scale factor.
  EXPECT_EQ(page_content().viewport_geometry().x(), 0);
  EXPECT_EQ(page_content().viewport_geometry().y(), 0);
  EXPECT_EQ(page_content().viewport_geometry().width(), window_bounds.width());
  EXPECT_EQ(page_content().viewport_geometry().height(),
            window_bounds.height());
}

bool ContainsRole(const optimization_guide::proto::ContentNode& node,
                  optimization_guide::proto::AnnotatedRole role) {
  for (const auto& r : node.content_attributes().annotated_roles()) {
    if (r == role) {
      return true;
    }
  }
  return false;
}

class PageContentProtoProviderBrowserTestPaidContentDisabled
    : public PageContentProtoProviderBrowserTest {
 public:
 PageContentProtoProviderBrowserTestPaidContentDisabled() {
    features_.InitAndDisableFeature(
        blink::features::kAIPageContentPaidContentAnnotation);
  }
 private:
  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_F(PageContentProtoProviderBrowserTest, PaidContent) {
  LoadPage(https_server()->GetURL("/paid_content.html"));

  // The page contains paid content.
  EXPECT_TRUE(page_content()
                  .main_frame_data()
                  .paid_content_metadata()
                  .contains_paid_content());

  auto& nodes = page_content().root_node().children_nodes();
  EXPECT_EQ(nodes.size(), 2);
  EXPECT_FALSE(ContainsRole(
      nodes[0], optimization_guide::proto::ANNOTATED_ROLE_PAID_CONTENT));
  EXPECT_TRUE(ContainsRole(
      nodes[1], optimization_guide::proto::ANNOTATED_ROLE_PAID_CONTENT));
}

IN_PROC_BROWSER_TEST_F(PageContentProtoProviderBrowserTestPaidContentDisabled,
                       PaidContentDisabled) {
  LoadPage(https_server()->GetURL("/paid_content.html"));

  // If the feature has been disabled, there should be no paid content metadata.
  EXPECT_FALSE(page_content().main_frame_data().has_paid_content_metadata());

  auto& nodes = page_content().root_node().children_nodes();
  EXPECT_EQ(nodes.size(), 2);
  EXPECT_FALSE(ContainsRole(
      nodes[0], optimization_guide::proto::ANNOTATED_ROLE_PAID_CONTENT));
  EXPECT_FALSE(ContainsRole(
      nodes[1], optimization_guide::proto::ANNOTATED_ROLE_PAID_CONTENT));
}

}  // namespace

}  // namespace optimization_guide
