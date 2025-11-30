// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/page_content_proto_provider.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/android/device_info.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "components/optimization_guide/content/browser/mock_media_transcript_provider.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/media_start_stop_observer.h"
#include "content/shell/browser/shell.h"
#include "content/shell/common/shell_switches.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "services/media_session/public/cpp/test/mock_media_session.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom-test-utils.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom.h"
#include "ui/display/display_switches.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace optimization_guide {

namespace {

// Allow 1px differences from rounding.
#define EXPECT_ALMOST_EQ(a, b) EXPECT_LE(abs(a - b), 1);

base::FilePath GetTestDataDir() {
  return base::FilePath(
      FILE_PATH_LITERAL("components/test/data/optimization_guide"));
}

void AssertIsTextNode(const optimization_guide::proto::ContentNode& text_node,
                      std::string text) {
  EXPECT_EQ(text_node.content_attributes().text_data().text_content(), text);
}

void AssertHasText(const optimization_guide::proto::ContentNode& node,
                   std::string text) {
  EXPECT_EQ(node.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_ROOT);
  EXPECT_EQ(node.children_nodes().size(), 1);
  const auto& text_node = node.children_nodes().at(0);
  AssertIsTextNode(text_node, text);
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
  auto request = DefaultAIPageContentOptions(/*on_critical_path =*/true);
  return request;
}

blink::mojom::AIPageContentOptionsPtr GetActionableAIPageContentOptions(
    bool include_same_site_only = false) {
  auto request = ActionableAIPageContentOptions(
      /*on_critical_path =*/true);
  request->include_same_site_only = include_same_site_only;
  return request;
}

// Given the root node for a Document, provides the body node which actually has
// the document's content.
const optimization_guide::proto::ContentNode&
ContentRootNodeForFrameActionableMode(
    const optimization_guide::proto::ContentNode& root) {
  EXPECT_EQ(root.children_nodes().size(), 1);
  const auto& html = root.children_nodes().at(0);

  EXPECT_EQ(html.children_nodes().size(), 1);
  const auto& body = html.children_nodes().at(0);

  return body;
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_FUCHSIA)
// Helper function to generate a click on the given RenderWidgetHost. The
// mouse event is forwarded directly to the RenderWidgetHost without any
// hit-testing.
void SimulateMouseClickAt(content::RenderWidgetHost* rwh, gfx::PointF point) {
  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::Type::kMouseDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.button = blink::WebPointerProperties::Button::kLeft;
  mouse_event.SetPositionInWidget(point.x(), point.y());
  rwh->ForwardMouseEvent(mouse_event);
}

#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_MAC) &&
        // !BUILDFLAG(IS_FUCHSIA)

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
    https_server_->SetCertHostnames({"a.com", "b.com", "c.com"});
    content::SetupCrossSiteRedirector(https_server_.get());

    ASSERT_TRUE(https_server_->Start());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    content::ContentBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kForceDeviceScaleFactor, "1.0");

    // Expose window.internals.setIsAdFrame for testing frame ad tagging.
    command_line->AppendSwitch(switches::kExposeInternalsForTesting);
  }

  void SetPageContent(base::OnceClosure quit_closure,
                      AIPageContentResultOrError page_content) {
    page_content_ = std::move(page_content->proto);
    metadata_ = std::move(page_content->metadata);
    document_identifiers_ = std::move(page_content->document_identifiers);
    std::move(quit_closure).Run();
  }

  const proto::AnnotatedPageContent& page_content() { return *page_content_; }
  const blink::mojom::PageMetadata& metadata() { return *metadata_; }
  const base::flat_map<std::string, content::WeakDocumentPtr>&
  document_identifiers() {
    return document_identifiers_;
  }

  // If `quit_closure` is null, will block until the load is complete.
  void LoadData(
      blink::mojom::AIPageContentOptionsPtr request = GetAIPageContentOptions(),
      base::OnceClosure quit_closure = base::OnceClosure()) {
    bool should_wait_for_page_content = quit_closure.is_null();
    base::RunLoop run_loop;
    GetAIPageContent(
        web_contents(), std::move(request),
        base::BindOnce(
            &PageContentProtoProviderBrowserTest::SetPageContent,
            base::Unretained(this),
            quit_closure ? std::move(quit_closure) : run_loop.QuitClosure()));
    if (should_wait_for_page_content) {
      run_loop.Run();
      CHECK(page_content_);
    }
  }

  void LoadPage(GURL url,
                blink::mojom::AIPageContentOptionsPtr options =
                    GetAIPageContentOptions()) {
    content::NavigateToURLBlockUntilNavigationsComplete(web_contents(), url, 1);

    {
      base::test::TestFuture<bool> future;
      web_contents()
          ->GetPrimaryMainFrame()
          ->GetRenderWidgetHost()
          ->InsertVisualStateCallback(future.GetCallback());
      ASSERT_TRUE(future.Wait()) << "Timeout waiting for syncing with renderer";
    }

    if (options) {
      LoadData(std::move(options));
    }
  }

  void SelectTextInBody(content::RenderFrameHost* rfh) {
    const std::string kSelectText =
        "window.getSelection().selectAllChildren(document.body);";
    ASSERT_TRUE(content::ExecJs(rfh, kSelectText));
  }

  net::EmbeddedTestServer* https_server() { return https_server_.get(); }

  const optimization_guide::proto::ContentNode& ActionableContentRootNode() {
    return ContentRootNodeForFrameActionableMode(page_content().root_node());
  }

 private:
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  std::optional<proto::AnnotatedPageContent> page_content_;
  blink::mojom::PageMetadataPtr metadata_;
  base::flat_map<std::string, content::WeakDocumentPtr> document_identifiers_;
};

IN_PROC_BROWSER_TEST_F(PageContentProtoProviderBrowserTest, BasicDefault) {
  LoadPage(https_server()->GetURL("/simple.html"));

  EXPECT_EQ(page_content().version(),
            optimization_guide::proto::ANNOTATED_PAGE_CONTENT_VERSION_1_0);
  EXPECT_EQ(page_content().root_node().children_nodes().size(), 1);
  AssertHasText(page_content().root_node(), "Non empty simple page\n\n");
  EXPECT_FALSE(
      page_content().root_node().content_attributes().has_interaction_info());
}

IN_PROC_BROWSER_TEST_F(PageContentProtoProviderBrowserTest, BasicActionable) {
  const gfx::Size window_bounds(web_contents()->GetSize());
  LoadPage(https_server()->GetURL("/simple.html"),
           GetActionableAIPageContentOptions());

  EXPECT_EQ(page_content().version(),
            optimization_guide::proto::
                ANNOTATED_PAGE_CONTENT_VERSION_ONLY_ACTIONABLE_ELEMENTS_1_0);
  EXPECT_EQ(page_content().mode(),
            optimization_guide::proto::
                ANNOTATED_PAGE_CONTENT_MODE_ACTIONABLE_ELEMENTS);
  const auto& root_node = ActionableContentRootNode();
  EXPECT_EQ(root_node.children_nodes().size(), 1);
  AssertIsTextNode(root_node.children_nodes()[0], "Non empty simple page\n\n");

  const auto& root_geometry = root_node.content_attributes().geometry();
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
  LoadPage(https_server()->GetURL("/simple.html"), nullptr);
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
  EXPECT_FALSE(child.content_attributes().interaction_info().is_disabled());
}

IN_PROC_BROWSER_TEST_F(PageContentProtoProviderBrowserTest, ForLabel) {
  LoadPage(https_server()->GetURL("/for_label.html"),
           GetActionableAIPageContentOptions());
  EXPECT_EQ(page_content().version(),
            optimization_guide::proto::
                ANNOTATED_PAGE_CONTENT_VERSION_ONLY_ACTIONABLE_ELEMENTS_1_0);

  EXPECT_EQ(ActionableContentRootNode().children_nodes().size(), 2);

  const auto& input = ActionableContentRootNode().children_nodes()[0];
  ASSERT_TRUE(input.content_attributes().has_interaction_info());
  EXPECT_THAT(
      input.content_attributes()
          .interaction_info()
          .debug_clickability_reasons(),
      testing::UnorderedElementsAre(
          optimization_guide::proto::CLICKABILITY_REASON_CLICKABLE_CONTROL));

  const auto& label = ActionableContentRootNode().children_nodes()[1];
  ASSERT_TRUE(label.content_attributes().has_interaction_info());
  EXPECT_TRUE(label.content_attributes()
                  .interaction_info()
                  .debug_clickability_reasons()
                  .empty());
  EXPECT_EQ(label.content_attributes().label_for_dom_node_id(),
            input.content_attributes().common_ancestor_dom_node_id());
}

IN_PROC_BROWSER_TEST_F(PageContentProtoProviderBrowserTest,
                       ClickabilityReason) {
  LoadPage(https_server()->GetURL("/clickability_reason.html"),
           GetActionableAIPageContentOptions());
  EXPECT_EQ(page_content().version(),
            optimization_guide::proto::
                ANNOTATED_PAGE_CONTENT_VERSION_ONLY_ACTIONABLE_ELEMENTS_1_0);

  const auto& button_node = ActionableContentRootNode().children_nodes()[0];
  ASSERT_TRUE(button_node.content_attributes().has_interaction_info());
  EXPECT_THAT(
      button_node.content_attributes()
          .interaction_info()
          .debug_clickability_reasons(),
      testing::UnorderedElementsAre(
          optimization_guide::proto::CLICKABILITY_REASON_CLICKABLE_CONTROL,
          optimization_guide::proto::CLICKABILITY_REASON_CLICK_HANDLER,
          optimization_guide::proto::CLICKABILITY_REASON_MOUSE_EVENTS,
          optimization_guide::proto::CLICKABILITY_REASON_MOUSE_HOVER,
          optimization_guide::proto::CLICKABILITY_REASON_MOUSE_CLICK,
          optimization_guide::proto::CLICKABILITY_REASON_KEY_EVENTS,
          optimization_guide::proto::CLICKABILITY_REASON_EDITABLE,
          optimization_guide::proto::CLICKABILITY_REASON_CURSOR_POINTER,
          optimization_guide::proto::CLICKABILITY_REASON_ARIA_ROLE,
          optimization_guide::proto::CLICKABILITY_REASON_ARIA_HAS_POPUP,
          optimization_guide::proto::CLICKABILITY_REASON_TAB_INDEX,
          optimization_guide::proto::CLICKABILITY_REASON_HOVER_PSEUDO_CLASS));
  EXPECT_THAT(
      button_node.content_attributes()
          .interaction_info()
          .clickability_reasons(),
      testing::UnorderedElementsAre(
          optimization_guide::proto::CLICKABILITY_REASON_CLICKABLE_CONTROL,
          optimization_guide::proto::CLICKABILITY_REASON_CLICK_HANDLER,
          optimization_guide::proto::CLICKABILITY_REASON_MOUSE_EVENTS,
          optimization_guide::proto::CLICKABILITY_REASON_MOUSE_HOVER,
          optimization_guide::proto::CLICKABILITY_REASON_MOUSE_CLICK,
          optimization_guide::proto::CLICKABILITY_REASON_KEY_EVENTS,
          optimization_guide::proto::CLICKABILITY_REASON_EDITABLE,
          optimization_guide::proto::CLICKABILITY_REASON_CURSOR_POINTER,
          optimization_guide::proto::CLICKABILITY_REASON_ARIA_ROLE,
          optimization_guide::proto::CLICKABILITY_REASON_ARIA_HAS_POPUP,
          optimization_guide::proto::CLICKABILITY_REASON_TAB_INDEX,
          optimization_guide::proto::CLICKABILITY_REASON_HOVER_PSEUDO_CLASS));

  const auto& expanded = ActionableContentRootNode().children_nodes()[1];
  ASSERT_TRUE(expanded.content_attributes().has_interaction_info());
  EXPECT_THAT(
      expanded.content_attributes().interaction_info().clickability_reasons(),
      testing::UnorderedElementsAre(
          optimization_guide::proto::CLICKABILITY_REASON_ARIA_EXPANDED_TRUE));

  const auto& collapsed = ActionableContentRootNode().children_nodes()[2];
  ASSERT_TRUE(collapsed.content_attributes().has_interaction_info());
  EXPECT_THAT(
      collapsed.content_attributes().interaction_info().clickability_reasons(),
      testing::UnorderedElementsAre(
          optimization_guide::proto::CLICKABILITY_REASON_ARIA_EXPANDED_FALSE));
}

IN_PROC_BROWSER_TEST_F(PageContentProtoProviderBrowserTest,
                       LabelNotActionable) {
  LoadPage(https_server()->GetURL("/label_not_actionable.html"),
           GetActionableAIPageContentOptions());
  EXPECT_EQ(page_content().version(),
            optimization_guide::proto::
                ANNOTATED_PAGE_CONTENT_VERSION_ONLY_ACTIONABLE_ELEMENTS_1_0);

  EXPECT_EQ(ActionableContentRootNode().children_nodes().size(), 2);

  const auto& input = ActionableContentRootNode().children_nodes()[0];
  ASSERT_TRUE(input.content_attributes().has_interaction_info());
  EXPECT_THAT(
      input.content_attributes()
          .interaction_info()
          .debug_clickability_reasons(),
      testing::UnorderedElementsAre(
          optimization_guide::proto::CLICKABILITY_REASON_CLICKABLE_CONTROL));

  const auto& label = ActionableContentRootNode().children_nodes()[1];
  EXPECT_FALSE(label.content_attributes().has_interaction_info());
  EXPECT_EQ(label.content_attributes().label_for_dom_node_id(),
            input.content_attributes().common_ancestor_dom_node_id());
}

IN_PROC_BROWSER_TEST_F(PageContentProtoProviderBrowserTest, AriaRole) {
  LoadPage(https_server()->GetURL("/aria_role.html"),
           GetActionableAIPageContentOptions());
  EXPECT_EQ(page_content().version(),
            optimization_guide::proto::
                ANNOTATED_PAGE_CONTENT_VERSION_ONLY_ACTIONABLE_ELEMENTS_1_0);

  EXPECT_EQ(ActionableContentRootNode().children_nodes().size(), 1);
  const auto& button = ActionableContentRootNode().children_nodes()[0];
  ASSERT_TRUE(button.content_attributes().has_interaction_info());
  EXPECT_THAT(button.content_attributes()
                  .interaction_info()
                  .debug_clickability_reasons(),
              testing::UnorderedElementsAre(
                  optimization_guide::proto::CLICKABILITY_REASON_ARIA_ROLE));
  EXPECT_EQ(button.content_attributes().aria_role(),
            optimization_guide::proto::AXRole::AX_ROLE_BUTTON);
}

IN_PROC_BROWSER_TEST_F(PageContentProtoProviderBrowserTest, ZOrder) {
  LoadPage(https_server()->GetURL("/simple.html"),
           GetActionableAIPageContentOptions());

  EXPECT_EQ(page_content()
                .root_node()
                .content_attributes()
                .interaction_info()
                .document_scoped_z_order(),
            1);
}

IN_PROC_BROWSER_TEST_F(PageContentProtoProviderBrowserTest,
                       AIPageContentNoGeometry) {
  LoadPage(https_server()->GetURL("/simple.html"), nullptr);

  auto request = blink::mojom::AIPageContentOptions::New();
  LoadData(std::move(request));

  EXPECT_EQ(page_content().root_node().children_nodes().size(), 1);
  AssertHasText(page_content().root_node(), "Non empty simple page\n\n");
  EXPECT_FALSE(page_content().root_node().content_attributes().has_geometry());
}

IN_PROC_BROWSER_TEST_F(PageContentProtoProviderBrowserTest,
                       AIPageContentNoCriticalPath) {
  LoadPage(https_server()->GetURL("/simple.html"), nullptr);

  auto request = blink::mojom::AIPageContentOptions::New();
  request->on_critical_path = false;
  LoadData(std::move(request));

  EXPECT_EQ(page_content().root_node().children_nodes().size(), 1);
  AssertHasText(page_content().root_node(), "Non empty simple page\n\n");
  EXPECT_FALSE(page_content().root_node().content_attributes().has_geometry());
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

IN_PROC_BROWSER_TEST_F(PageContentProtoProviderBrowserTest, Video) {
  LoadPage(https_server()->GetURL("/video.html"));

  EXPECT_EQ(page_content().root_node().children_nodes().size(), 1);

  const auto& video_node = page_content().root_node().children_nodes().at(0);
  ASSERT_TRUE(video_node.content_attributes().has_video_data());
  EXPECT_EQ(video_node.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_VIDEO);
  EXPECT_EQ(video_node.content_attributes().video_data().url(),
            https_server()->GetURL("/video.mp4").spec());
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
  EXPECT_FALSE(iframe.content_attributes().is_ad_related());

  EXPECT_EQ(iframe.children_nodes().size(), 1);
}

// TODO(crbug.com/447642858): An end-to-end ad tagging test that uses the
// subresource filter should be added.
IN_PROC_BROWSER_TEST_F(PageContentProtoProviderBrowserTest,
                       AIPageContentAdIframe) {
  LoadPage(https_server()->GetURL("a.com", "/iframe_ad.html"));

  // Mark the iframe as an ad frame.
  ASSERT_TRUE(content::ExecJs(web_contents(), R"(
                          const iframe = document.getElementById('iframe1');
                          window.internals.setIsAdFrame(iframe.contentDocument);
                        )"));
  LoadData();

  EXPECT_EQ(page_content().root_node().children_nodes().size(), 1);

  const auto& iframe = page_content().root_node().children_nodes()[0];
  EXPECT_EQ(iframe.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME);
  const auto& iframe_data = iframe.content_attributes().iframe_data();
  AssertValidOrigin(iframe_data.frame_data().security_origin(),
                    ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0)
                        ->GetLastCommittedOrigin());
  EXPECT_TRUE(iframe.content_attributes().is_ad_related());

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
  EXPECT_FALSE(iframe.content_attributes().is_ad_related());

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
           nullptr);

  auto request = optimization_guide::DefaultAIPageContentOptions(
      /*on_critical_path =*/false);
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
               "a.com", base::StringPrintf(
                            "/paragraph_iframe_partially_offscreen.html%s",
                            QueryParam())),
           GetActionableAIPageContentOptions());

  const auto& root_node = ActionableContentRootNode();
  ASSERT_EQ(root_node.children_nodes().size(), 1);

  const auto& iframe = root_node.children_nodes()[0];
  ASSERT_EQ(iframe.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME);

  ASSERT_EQ(iframe.children_nodes().size(), 1);
  const auto& iframe_root =
      ContentRootNodeForFrameActionableMode(iframe.children_nodes()[0]);

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
  LoadPage(
      https_server()->GetURL(
          "a.com", base::StringPrintf(
                       "/paragraph_iframe_partially_scrolled_offscreen.html%s",
                       QueryParam())),
      GetActionableAIPageContentOptions());

  const auto& root_node = ActionableContentRootNode();
  ASSERT_EQ(root_node.children_nodes().size(), 2);

  const auto& iframe = root_node.children_nodes()[0];
  ASSERT_EQ(iframe.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME);

  ASSERT_EQ(iframe.children_nodes().size(), 1);
  const auto& iframe_root =
      ContentRootNodeForFrameActionableMode(iframe.children_nodes()[0]);

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
           nullptr);

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
  PageContentProtoProviderBrowserTestMultiProcess() {
    feature_list_.InitAndEnableFeature(
        blink::features::kAIPageContentIncludePopupWindows);
  }
  ~PageContentProtoProviderBrowserTestMultiProcess() override = default;

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

 private:
  base::test::ScopedFeatureList feature_list_;
};

// TODO(crbug.com/438250758): Test is flaky.
IN_PROC_BROWSER_TEST_P(PageContentProtoProviderBrowserTestMultiProcess,
                       DISABLED_AIPageContentMultipleCrossSiteFrames) {
  LoadPage(https_server()->GetURL("a.com", "/iframe_cross_site.html"),
           GetActionableAIPageContentOptions());

  const auto& root_node = ActionableContentRootNode();
  EXPECT_EQ(root_node.children_nodes().size(), 2);

  const auto& b_frame = root_node.children_nodes()[0];
  EXPECT_EQ(b_frame.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME);
  const auto& b_frame_data = b_frame.content_attributes().iframe_data();
  AssertValidOrigin(b_frame_data.frame_data().security_origin(),
                    ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0)
                        ->GetLastCommittedOrigin());
  EXPECT_FALSE(b_frame.content_attributes().is_ad_related());

  const auto& b_frame_root =
      ContentRootNodeForFrameActionableMode(b_frame.children_nodes()[0]);
  EXPECT_EQ(b_frame_root.children_nodes().size(), 1);
  AssertIsTextNode(b_frame_root.children_nodes()[0],
                   "This page has no title.\n\n");
  const auto& b_geometry = b_frame.content_attributes().geometry();
  AssertRectsEqual(b_geometry.outer_bounding_box(),
                   b_geometry.visible_bounding_box());

  const auto& c_frame = root_node.children_nodes()[1];
  EXPECT_EQ(c_frame.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME);
  const auto& c_frame_data = c_frame.content_attributes().iframe_data();
  AssertValidOrigin(c_frame_data.frame_data().security_origin(),
                    ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 1)
                        ->GetLastCommittedOrigin());
  EXPECT_FALSE(c_frame.content_attributes().is_ad_related());

  const auto& c_frame_root =
      ContentRootNodeForFrameActionableMode(c_frame.children_nodes()[0]);
  EXPECT_EQ(c_frame_root.children_nodes().size(), 1);
  AssertIsTextNode(c_frame_root.children_nodes()[0],
                   "This page has no title.\n\n");
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

IN_PROC_BROWSER_TEST_P(PageContentProtoProviderBrowserTestMultiProcess,
                       AIPageContentMultipleMixedCrossSiteFrames) {
  LoadPage(https_server()->GetURL("a.com", "/iframe_mixed_cross_site.html"),
           GetActionableAIPageContentOptions(/*include_same_site_only=*/true));

  const auto& root_node = ActionableContentRootNode();

  EXPECT_EQ(root_node.children_nodes().size(), 2);

  const auto& same_site_frame = root_node.children_nodes()[0];
  EXPECT_EQ(same_site_frame.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME);
  const auto& same_site_frame_data =
      same_site_frame.content_attributes().iframe_data();
  AssertValidOrigin(same_site_frame_data.frame_data().security_origin(),
                    ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0)
                        ->GetLastCommittedOrigin());
  EXPECT_FALSE(same_site_frame.content_attributes().is_ad_related());

  const auto& same_site_frame_root = ContentRootNodeForFrameActionableMode(
      same_site_frame.children_nodes()[0]);
  EXPECT_EQ(same_site_frame_root.children_nodes().size(), 1);
  AssertIsTextNode(same_site_frame_root.children_nodes()[0],
                   "This page has no title.\n\n");
  const auto& same_site_geometry =
      same_site_frame.content_attributes().geometry();
  AssertRectsEqual(same_site_geometry.outer_bounding_box(),
                   same_site_geometry.visible_bounding_box());

  const auto& cross_site_frame = root_node.children_nodes()[1];
  EXPECT_EQ(cross_site_frame.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME);
  const auto& cross_site_frame_data =
      cross_site_frame.content_attributes().iframe_data();

  // Ensure the frame data isn't populated and a redaction reason is included.
  EXPECT_FALSE(cross_site_frame.content_attributes().is_ad_related());
  EXPECT_FALSE(cross_site_frame_data.has_frame_data());
  EXPECT_TRUE(cross_site_frame_data.has_redacted_frame_metadata());
  EXPECT_EQ(cross_site_frame_data.redacted_frame_metadata().reason(),
            optimization_guide::proto::IframeData_RedactedFrameMetadata::
                REASON_CROSS_SITE);

  // The cross-site frame itself should have no children.
  EXPECT_EQ(cross_site_frame.children_nodes().size(), 0);

  const auto& cross_site_frame_geometry =
      cross_site_frame.content_attributes().geometry();
  AssertRectsEqual(cross_site_frame_geometry.outer_bounding_box(),
                   cross_site_frame_geometry.visible_bounding_box());

  EXPECT_ALMOST_EQ(same_site_geometry.outer_bounding_box().width(),
                   cross_site_frame_geometry.outer_bounding_box().width());
  EXPECT_ALMOST_EQ(same_site_geometry.outer_bounding_box().height(),
                   cross_site_frame_geometry.outer_bounding_box().height());
  EXPECT_ALMOST_EQ(same_site_geometry.outer_bounding_box().y(),
                   cross_site_frame_geometry.outer_bounding_box().y());
  EXPECT_NE(same_site_geometry.outer_bounding_box().x(),
            cross_site_frame_geometry.outer_bounding_box().x());
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
           nullptr);

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
  EXPECT_FALSE(b_frame.content_attributes().is_ad_related());
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
  LoadPage(https_server()->GetURL("a.com", "/iframe_same_site.html"), nullptr);

  auto options = blink::mojom::AIPageContentOptions::New();
  options->max_meta_elements = 32;
  LoadData(std::move(options));

  EXPECT_EQ(metadata().frame_metadata.size(), 3u);

  const auto& main_frame_metadata = metadata().frame_metadata[0];
  EXPECT_EQ(main_frame_metadata->url.GetHost(), "a.com");
  EXPECT_EQ(main_frame_metadata->meta_tags.size(), 1u);
  EXPECT_EQ(main_frame_metadata->meta_tags[0]->name, "author");
  EXPECT_EQ(main_frame_metadata->meta_tags[0]->content, "George");

  const auto& child_frame_metadata1 = metadata().frame_metadata[1];
  EXPECT_EQ(child_frame_metadata1->url.GetHost(), "a.com");
  EXPECT_EQ(child_frame_metadata1->meta_tags.size(), 1u);
  EXPECT_EQ(child_frame_metadata1->meta_tags[0]->name, "author");
  EXPECT_EQ(child_frame_metadata1->meta_tags[0]->content, "Gary");

  const auto& child_frame_metadata2 = metadata().frame_metadata[2];
  EXPECT_EQ(child_frame_metadata2->url.GetHost(), "a.com");
  EXPECT_EQ(child_frame_metadata2->meta_tags.size(), 1u);
  EXPECT_EQ(child_frame_metadata2->meta_tags[0]->name, "author");
  EXPECT_EQ(child_frame_metadata2->meta_tags[0]->content, "Gary");
}

IN_PROC_BROWSER_TEST_P(PageContentProtoProviderBrowserTestMultiProcess,
                       AIPageContentFrameIdentifiersTheSame) {
  LoadPage(https_server()->GetURL("a.com", "/fenced_frame/basic.html"),
           nullptr);

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

#if BUILDFLAG(IS_WIN)
#define MAYBE_DeepTree DISABLED_DeepTree
#else
#define MAYBE_DeepTree DeepTree
#endif
// TODO(crbug.com/425717554): This test is flaking on windows due to a renderer
// crash.
IN_PROC_BROWSER_TEST_P(PageContentProtoProviderBrowserTestMultiProcess,
                       MAYBE_DeepTree) {
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

// Popups may be rendered as native OS-level widgets on Android and MacOS.
//
// TODO: b/450618828 - Enable on Fuchsia with proper geometry comparison.
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_FUCHSIA)
IN_PROC_BROWSER_TEST_P(PageContentProtoProviderBrowserTestMultiProcess,
                       SelectInCrossOriginIframe) {
  LoadPage(https_server()->GetURL(
      "a.com", "/open_popup_iframe.html?domain=/cross-site/b.com/"));

  content::RenderFrameHost* iframe =
      content::ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);

  content::ShowPopupWidgetWaiter new_popup_waiter(web_contents(), iframe);

  // showPicker() is not allowed from cross-origin iframe for security reasons,
  // therefore simulating a user click.
  SimulateMouseClickAt(
      iframe->GetRenderWidgetHost(),
      GetCenterCoordinatesOfElementWithId(iframe, "select_input"));
  new_popup_waiter.Wait();

  LoadData(GetActionableAIPageContentOptions());
  ASSERT_TRUE(page_content().has_popup_window());

  const auto& popup_window = page_content().popup_window();

  const auto& iframe_node = ActionableContentRootNode().children_nodes()[0];
  EXPECT_EQ(popup_window.opener_document_id().serialized_token(),
            iframe_node.content_attributes()
                .iframe_data()
                .frame_data()
                .document_identifier()
                .serialized_token());

  EXPECT_EQ(iframe_node.children_nodes().size(), 1);
  const auto& iframe_node_root =
      ContentRootNodeForFrameActionableMode(iframe_node.children_nodes()[0]);
  const auto& select_node = iframe_node_root.children_nodes()[0];
  EXPECT_EQ(select_node.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_FORM_CONTROL);
  EXPECT_EQ(popup_window.opener_common_ancestor_dom_node_id(),
            select_node.content_attributes().common_ancestor_dom_node_id());

  const auto& popup_root = ContentRootNodeForFrameActionableMode(
      popup_window.root_node().children_nodes()[0]);
  const auto& select_node_in_popup = popup_root.children_nodes()[0];
  EXPECT_EQ(select_node_in_popup.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_FORM_CONTROL);

  const auto& select_node_geometry =
      select_node.content_attributes().geometry();
  const auto& select_node_in_popup_geometry =
      select_node_in_popup.content_attributes().geometry();
  // The y value is the bottom edge of the form element. The height of the form
  // element is 10px.
  EXPECT_EQ(select_node_in_popup_geometry.outer_bounding_box().x(),
            select_node_geometry.outer_bounding_box().x());
  EXPECT_EQ(select_node_in_popup_geometry.outer_bounding_box().y(),
            select_node_geometry.outer_bounding_box().y() + 10);
  EXPECT_EQ(select_node_in_popup_geometry.visible_bounding_box().x(),
            select_node_geometry.visible_bounding_box().x());
  EXPECT_EQ(select_node_in_popup_geometry.visible_bounding_box().y(),
            select_node_geometry.visible_bounding_box().y() + 10);

  EXPECT_EQ(popup_window.visible_bounding_box().x(),
            select_node_geometry.visible_bounding_box().x());
  EXPECT_EQ(popup_window.visible_bounding_box().y(),
            select_node_geometry.visible_bounding_box().y() + 10);
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_MAC) &&
        // !BUILDFLAG(IS_FUCHSIA)

class ScaledPageContentProtoProviderBrowserTest
    : public PageContentProtoProviderBrowserTest {
 public:
  ScaledPageContentProtoProviderBrowserTest() {
    feature_list_.InitAndEnableFeature(
        blink::features::kAIPageContentIncludePopupWindows);
  }
  ~ScaledPageContentProtoProviderBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    content::ContentBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kForceDeviceScaleFactor, "2.0");
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ScaledPageContentProtoProviderBrowserTest, ScaleSizes) {
  // TODO(crbug.com/456812241): Re-enable this test on automotive once the test
  // is fixed.
#if BUILDFLAG(IS_ANDROID)
  if (base::android::device_info::is_automotive()) {
    GTEST_SKIP() << "This test is disabled on automotive due to flakiness.";
  }
#endif

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

// Popups may be rendered as native OS-level widgets on Android and Apple OSs.
//
// TODO: b/450618828 - Enable on Fuchsia with proper geometry comparison.
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_APPLE) && !BUILDFLAG(IS_FUCHSIA)
IN_PROC_BROWSER_TEST_F(ScaledPageContentProtoProviderBrowserTest,
                       SelectInMainFrame) {
  LoadPage(https_server()->GetURL("/open_popup.html"));

  content::ShowPopupWidgetWaiter new_popup_waiter(
      web_contents(), web_contents()->GetPrimaryMainFrame());
  ASSERT_TRUE(content::ExecJs(
      web_contents(), "document.getElementById('select_input').showPicker();"));
  new_popup_waiter.Wait();

  LoadData(GetActionableAIPageContentOptions());
  ASSERT_TRUE(page_content().has_popup_window());

  const auto& select_node = ActionableContentRootNode().children_nodes()[0];
  EXPECT_EQ(select_node.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_FORM_CONTROL);

  const auto& popup_root = ContentRootNodeForFrameActionableMode(
      page_content().popup_window().root_node().children_nodes()[0]);
  const auto& select_node_in_popup = popup_root.children_nodes()[0];
  EXPECT_EQ(select_node_in_popup.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_FORM_CONTROL);

  const auto& select_node_geometry =
      select_node.content_attributes().geometry();
  const auto& select_node_in_popup_geometry =
      select_node_in_popup.content_attributes().geometry();
  // The y value is the bottom edge of the form element. The height of the form
  // element is 10px.
  EXPECT_EQ(select_node_in_popup_geometry.outer_bounding_box().x(),
            select_node_geometry.outer_bounding_box().x());
  EXPECT_EQ(select_node_in_popup_geometry.outer_bounding_box().y(),
            select_node_geometry.outer_bounding_box().y() + 10 * 2);
  EXPECT_EQ(select_node_in_popup_geometry.visible_bounding_box().x(),
            select_node_geometry.visible_bounding_box().x());
  EXPECT_EQ(select_node_in_popup_geometry.visible_bounding_box().y(),
            select_node_geometry.visible_bounding_box().y() + 10 * 2);

  EXPECT_EQ(page_content().popup_window().visible_bounding_box().x(),
            select_node_geometry.outer_bounding_box().x());
  EXPECT_EQ(page_content().popup_window().visible_bounding_box().y(),
            select_node_geometry.outer_bounding_box().y() + 10 * 2);
}
#endif  //  !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_MAC) &&
        //  !BUILDFLAG(IS_FUCHSIA)

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

class PageContentProtoProviderBrowserTestScriptTools
    : public PageContentProtoProviderBrowserTest {
 public:
  PageContentProtoProviderBrowserTestScriptTools() {
    features_.InitAndEnableFeature(blink::features::kWebMCP);
  }

 private:
  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_F(PageContentProtoProviderBrowserTestScriptTools, Basic) {
  LoadPage(https_server()->GetURL("/script_tool.html"));

  const auto& frame_data = page_content().main_frame_data();
  ASSERT_EQ(frame_data.script_tools().size(), 1u);

  const auto& tool = frame_data.script_tools().at(0);
  EXPECT_EQ(tool.name(), "echo");
  EXPECT_EQ(tool.description(), "echo input");
  EXPECT_EQ(tool.input_schema(),
            "{\"type\":\"object\",\"properties\":{\"text\":{\"description\":"
            "\"Value to echo\",\"type\":\"string\"}},\"required\":[\"text\"]}");
  EXPECT_TRUE(tool.annotations().read_only());
}

IN_PROC_BROWSER_TEST_F(PageContentProtoProviderBrowserTestScriptTools,
                       NoAnnotations) {
  LoadPage(https_server()->GetURL("/script_tool_no_annotation.html"));

  const auto& frame_data = page_content().main_frame_data();
  ASSERT_EQ(frame_data.script_tools().size(), 1u);

  const auto& tool = frame_data.script_tools().at(0);
  EXPECT_EQ(tool.name(), "echo");
  EXPECT_EQ(tool.description(), "echo input");
  EXPECT_EQ(tool.input_schema(),
            "{\"type\":\"object\",\"properties\":{\"text\":{\"description\":"
            "\"Value to echo\",\"type\":\"string\"}},\"required\":[\"text\"]}");
  EXPECT_FALSE(tool.annotations().read_only());
}

IN_PROC_BROWSER_TEST_F(PageContentProtoProviderBrowserTestScriptTools,
                       NoInputSchema) {
  LoadPage(https_server()->GetURL("/script_tool_no_input_schema.html"));

  const auto& frame_data = page_content().main_frame_data();
  ASSERT_EQ(frame_data.script_tools().size(), 1u);

  const auto& tool = frame_data.script_tools().at(0);
  EXPECT_EQ(tool.name(), "echo");
  EXPECT_EQ(tool.description(), "echo input");
  EXPECT_FALSE(tool.has_input_schema());
  EXPECT_FALSE(tool.annotations().read_only());
}

class PageContentProtoProviderBrowserTestMediaData
    : public PageContentProtoProviderBrowserTest {
 public:
  PageContentProtoProviderBrowserTestMediaData()
      : features_(features::kAnnotatedPageContentWithMediaData) {}

  void WaitForMediaPlaybackStart(content::WebContents* web_contents) {
    content::MediaStartStopObserver observer(
        web_contents, content::MediaStartStopObserver::Type::kStart);
    ASSERT_EQ(base::Value(), content::EvalJs(web_contents, "play()"));
    observer.Wait();
  }

  void WaitForMediaPlaybackStop(content::WebContents* web_contents) {
    content::MediaStartStopObserver observer(
        web_contents, content::MediaStartStopObserver::Type::kStop);
    ASSERT_EQ(base::Value(), content::EvalJs(web_contents, "pause()"));
    observer.Wait();
  }

  media_session::MediaMetadata GetExpectedMetadata() {
    media_session::MediaMetadata metadata;
    metadata.title = u"test title";
    metadata.artist = u"test artist";
    metadata.album = u"test album";
    metadata.source_title = base::ASCIIToUTF16(base::StringPrintf(
        "%s:%u", https_server()->GetIPLiteralString().c_str(),
        https_server()->port()));
    return metadata;
  }

 private:
  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_F(PageContentProtoProviderBrowserTestMediaData,
                       NoMediaData) {
  LoadPage(https_server()->GetURL("/media_data/video.html"));
  EXPECT_FALSE(page_content().main_frame_data().has_media_data());
}

IN_PROC_BROWSER_TEST_F(PageContentProtoProviderBrowserTestMediaData,
                       VideoInMainFrame) {
  media_session::test::MockMediaSessionMojoObserver observer(
      *content::MediaSession::Get(web_contents()));
  LoadPage(https_server()->GetURL("/media_data/video.html"), nullptr);

  auto mock_provider = std::make_unique<MockMediaTranscriptProvider>();
  proto::MediaTranscript transcript;
  transcript.set_text("foo");
  transcript.set_start_timestamp_milliseconds(1000);
  EXPECT_CALL(*mock_provider, GetTranscriptsForFrame)
      .WillOnce(
          testing::Return(std::vector<proto::MediaTranscript>{transcript}));
  MediaTranscriptProvider::SetFor(web_contents(), std::move(mock_provider));

  WaitForMediaPlaybackStart(web_contents());
  ASSERT_EQ(base::Value(), content::EvalJs(web_contents(), "setupPosition()"));
  media_session::MediaPosition position(
      /*playback_rate=*/1.0, /*duration=*/base::Seconds(10),
      /*position=*/base::Seconds(5), /*end_of_media=*/false);
  observer.WaitForExpectedPosition(position);
  LoadData();

  // Check that the main frame has media data.
  EXPECT_TRUE(page_content().main_frame_data().has_media_data());
  const auto& media_data = page_content().main_frame_data().media_data();
  EXPECT_EQ(media_data.media_data_type(),
            optimization_guide::proto::MediaDataType::MEDIA_DATA_TYPE_VIDEO);
  EXPECT_EQ(media_data.duration_milliseconds(), 10000);
  EXPECT_TRUE(media_data.is_playing());
  EXPECT_EQ(media_data.transcripts().size(), 1);
  EXPECT_EQ(media_data.transcripts(0).text(), "foo");
  EXPECT_EQ(media_data.transcripts(0).start_timestamp_milliseconds(), 1000);

  // The metadata title is default to the page title if not set.
  EXPECT_EQ(media_data.title(), "Test page showing a video");
}

IN_PROC_BROWSER_TEST_F(PageContentProtoProviderBrowserTestMediaData,
                       UpdateVideoInMainFrame) {
  media_session::test::MockMediaSessionMojoObserver observer(
      *content::MediaSession::Get(web_contents()));
  LoadPage(https_server()->GetURL("/media_data/video.html"));

  // Start the media playback to ensure the media player is added to the media
  // session and then stop playing.
  WaitForMediaPlaybackStart(web_contents());
  WaitForMediaPlaybackStop(web_contents());

  // Update the video with media session API calls.
  ASSERT_EQ(base::Value(), content::EvalJs(web_contents(), "setupMetadata()"));
  observer.WaitForExpectedMetadata(GetExpectedMetadata());
  ASSERT_EQ(base::Value(), content::EvalJs(web_contents(), "setupPosition()"));
  media_session::MediaPosition position(
      /*playback_rate=*/1.0, /*duration=*/base::Seconds(10),
      /*position=*/base::Seconds(5), /*end_of_media=*/false);
  observer.WaitForExpectedPosition(position);
  LoadData();

  // Check that the main frame has media data with updated fields.
  EXPECT_TRUE(page_content().main_frame_data().has_media_data());
  const auto& media_data = page_content().main_frame_data().media_data();
  EXPECT_EQ(media_data.media_data_type(),
            optimization_guide::proto::MediaDataType::MEDIA_DATA_TYPE_VIDEO);
  EXPECT_EQ(media_data.duration_milliseconds(), 10000);
  EXPECT_FALSE(media_data.is_playing());
  EXPECT_EQ(media_data.title(), "test title");
  EXPECT_EQ(media_data.artist(), "test artist");
  EXPECT_EQ(media_data.album(), "test album");
  EXPECT_EQ(media_data.transcripts().size(), 0);
}

IN_PROC_BROWSER_TEST_F(PageContentProtoProviderBrowserTestMediaData,
                       VideoInIframe) {
  media_session::test::MockMediaSessionMojoObserver observer(
      *content::MediaSession::Get(web_contents()));
  LoadPage(https_server()->GetURL("/media_data/video_in_iframe.html"), nullptr);
  WaitForMediaPlaybackStart(web_contents());
  ASSERT_EQ(base::Value(), content::EvalJs(web_contents(), "setupPosition()"));
  media_session::MediaPosition position(
      /*playback_rate=*/1.0, /*duration=*/base::Seconds(10),
      /*position=*/base::Seconds(5), /*end_of_media=*/false);
  observer.WaitForExpectedPosition(position);
  LoadData();

  EXPECT_FALSE(page_content().main_frame_data().has_media_data());

  // Check that the iframe has media data.
  EXPECT_EQ(page_content().root_node().children_nodes().size(), 1);
  const auto& iframe = page_content().root_node().children_nodes()[0];
  EXPECT_EQ(iframe.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME);
  const auto& iframe_data = iframe.content_attributes().iframe_data();
  EXPECT_TRUE(iframe_data.frame_data().has_media_data());
  const auto& media_data = iframe_data.frame_data().media_data();

  EXPECT_EQ(media_data.media_data_type(),
            optimization_guide::proto::MediaDataType::MEDIA_DATA_TYPE_VIDEO);
  EXPECT_EQ(media_data.duration_milliseconds(), 10000);
  EXPECT_TRUE(media_data.is_playing());
  EXPECT_EQ(media_data.transcripts().size(), 0);
}

IN_PROC_BROWSER_TEST_F(PageContentProtoProviderBrowserTestMediaData,
                       VideoHasOnlyTranscripts) {
  LoadPage(https_server()->GetURL("/media_data/video.html"), nullptr);

  auto mock_provider = std::make_unique<MockMediaTranscriptProvider>();
  proto::MediaTranscript transcript;
  transcript.set_text("foo");
  transcript.set_start_timestamp_milliseconds(1000);
  EXPECT_CALL(*mock_provider, GetTranscriptsForFrame)
      .WillOnce(
          testing::Return(std::vector<proto::MediaTranscript>{transcript}));
  MediaTranscriptProvider::SetFor(web_contents(), std::move(mock_provider));

  LoadData();

  // Check that the main frame has media data with transcripts.
  EXPECT_TRUE(page_content().main_frame_data().has_media_data());
  const auto& media_data = page_content().main_frame_data().media_data();
  EXPECT_EQ(media_data.media_data_type(),
            optimization_guide::proto::MediaDataType::MEDIA_DATA_TYPE_UNKNOWN);
  EXPECT_EQ(media_data.transcripts().size(), 1);
  EXPECT_EQ(media_data.transcripts(0).text(), "foo");
  EXPECT_EQ(media_data.transcripts(0).start_timestamp_milliseconds(), 1000);
}

IN_PROC_BROWSER_TEST_F(PageContentProtoProviderBrowserTest,
                       FormRedactionDecisions) {
  LoadPage(https_server()->GetURL("/redaction.html"));

  EXPECT_EQ(page_content().root_node().children_nodes().size(), 1);
  const auto& form_node = page_content().root_node().children_nodes()[0];
  EXPECT_EQ(form_node.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_FORM);

  ASSERT_TRUE(form_node.content_attributes().has_form_data());
  const auto& form_data = form_node.content_attributes().form_data();
  // The fixture sets the form action to an absolute-path relative URL, which
  // should resolve against the document URL.
  EXPECT_EQ(form_data.action_url(), https_server()->GetURL("/initial").spec());

  ASSERT_EQ(form_node.children_nodes().size(), 3);

  // Text input should have no redaction necessary
  const auto& text_input = form_node.children_nodes()[0];
  EXPECT_EQ(text_input.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_FORM_CONTROL);
  ASSERT_TRUE(text_input.content_attributes().has_form_control_data());
  EXPECT_EQ(
      text_input.content_attributes().form_control_data().redaction_decision(),
      optimization_guide::proto::REDACTION_DECISION_NO_REDACTION_NECESSARY);

  // Empty password should be unredacted
  const auto& empty_password = form_node.children_nodes()[1];
  EXPECT_EQ(empty_password.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_FORM_CONTROL);
  ASSERT_TRUE(empty_password.content_attributes().has_form_control_data());
  EXPECT_EQ(
      empty_password.content_attributes()
          .form_control_data()
          .redaction_decision(),
      optimization_guide::proto::REDACTION_DECISION_UNREDACTED_EMPTY_PASSWORD);

  // Filled password should be redacted
  const auto& filled_password = form_node.children_nodes()[2];
  EXPECT_EQ(filled_password.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_FORM_CONTROL);
  ASSERT_TRUE(filled_password.content_attributes().has_form_control_data());
  EXPECT_EQ(
      filled_password.content_attributes()
          .form_control_data()
          .redaction_decision(),
      optimization_guide::proto::REDACTION_DECISION_REDACTED_HAS_BEEN_PASSWORD);

  // Change the filled password field to text type and verify it's still
  // redacted
  ASSERT_TRUE(content::ExecJs(
      web_contents(),
      "const form = document.querySelector('form');"
      "form.action = 'https://example.com/submit';"
      "document.getElementById('filled-password').type = 'text';"));
  LoadData();

  // Re-examine the form after updating both the action and field type.
  const auto& form_node_after = page_content().root_node().children_nodes()[0];
  ASSERT_EQ(form_node_after.children_nodes().size(), 3);
  ASSERT_TRUE(form_node_after.content_attributes().has_form_data());
  EXPECT_EQ(form_node_after.content_attributes().form_data().action_url(),
            "https://example.com/submit");
  const auto& changed_field = form_node_after.children_nodes()[2];
  EXPECT_EQ(changed_field.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_FORM_CONTROL);
  ASSERT_TRUE(changed_field.content_attributes().has_form_control_data());
  // Should still be redacted even though type changed to text
  EXPECT_EQ(
      changed_field.content_attributes()
          .form_control_data()
          .redaction_decision(),
      optimization_guide::proto::REDACTION_DECISION_REDACTED_HAS_BEEN_PASSWORD);

  // Finally, swap to a relative action path to confirm it resolves against the
  // document URL rather than remaining relative.
  ASSERT_TRUE(content::ExecJs(
      web_contents(),
      "document.querySelector('form').action = 'relative/next';"));
  LoadData();

  const auto& form_node_relative =
      page_content().root_node().children_nodes()[0];
  ASSERT_TRUE(form_node_relative.content_attributes().has_form_data());
  EXPECT_EQ(form_node_relative.content_attributes().form_data().action_url(),
            https_server()->GetURL("/relative/next").spec());
}

IN_PROC_BROWSER_TEST_F(PageContentProtoProviderBrowserTest,
                       FragmentVisibleBoundingBoxes) {
  LoadPage(https_server()->GetURL("/fragment_boxes.html"),
           GetActionableAIPageContentOptions());

  // Contents of fragment_boxes.html:
  // <!DOCTYPE html>
  // <body style = "font: 10px/10px monospace">
  //   <!--
  //   The really super [quick
  //   brown fox] jumps over the
  //   lazy dog.
  //   -->
  //.  <section style = "width: 25ch;">
  //     The really super <a href = "#">quick brown fox</a>
  //     jumps over the lazy dog.
  //   </section>
  // <body>

  EXPECT_EQ(page_content().version(),
            optimization_guide::proto::
                ANNOTATED_PAGE_CONTENT_VERSION_ONLY_ACTIONABLE_ELEMENTS_1_0);

  EXPECT_EQ(ActionableContentRootNode().children_nodes().size(), 1);
  const auto& section = ActionableContentRootNode().children_nodes()[0];
  EXPECT_EQ(section.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_CONTAINER);
  EXPECT_EQ(section.children_nodes().size(), 3);

  const auto& before_text = section.children_nodes()[0];
  const auto& link = section.children_nodes()[1];
  EXPECT_EQ(link.children_nodes().size(), 1);
  const auto& link_text = link.children_nodes()[0];
  const auto& after_text = section.children_nodes()[2];

  EXPECT_EQ(before_text.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_TEXT);
  EXPECT_EQ(link.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_ANCHOR);
  EXPECT_EQ(after_text.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_TEXT);

  // Nodes with 0 fragment_visible_bounding_boxes: before_text.
  // Nodes with 2 fragment_visible_bounding_boxes: link, link_text and
  // after_text.
  // For the nodes that have them, the following is true:
  // 1. The fragment boxes fit inside the visible_bounding_box
  // 2. The second fragment box is below the first box.

  ASSERT_EQ(before_text.content_attributes()
                .geometry()
                .fragment_visible_bounding_boxes()
                .size(),
            0);

  ASSERT_EQ(link.content_attributes()
                .geometry()
                .fragment_visible_bounding_boxes()
                .size(),
            2);
  // The top of the link's first fragment == top of its visible box.
  ASSERT_EQ(link.content_attributes().geometry().visible_bounding_box().y(),
            link.content_attributes()
                .geometry()
                .fragment_visible_bounding_boxes(0)
                .y());
  // The left of the link's first fragment > left of its visible box.
  ASSERT_LT(link.content_attributes().geometry().visible_bounding_box().x(),
            link.content_attributes()
                .geometry()
                .fragment_visible_bounding_boxes(0)
                .x());
  // The left of the second fragment == the the left of its visible box.
  ASSERT_EQ(link.content_attributes().geometry().visible_bounding_box().x(),
            link.content_attributes()
                .geometry()
                .fragment_visible_bounding_boxes(1)
                .x());
  // The second fragment box starts below the first.
  ASSERT_LT(link.content_attributes()
                .geometry()
                .fragment_visible_bounding_boxes(0)
                .y(),
            link.content_attributes()
                .geometry()
                .fragment_visible_bounding_boxes(1)
                .y());

  ASSERT_EQ(link_text.content_attributes()
                .geometry()
                .fragment_visible_bounding_boxes()
                .size(),
            2);

  // The link and link text have the same fragment bounding boxes.
  ASSERT_EQ(link.content_attributes()
                .geometry()
                .fragment_visible_bounding_boxes(0)
                .x(),
            link_text.content_attributes()
                .geometry()
                .fragment_visible_bounding_boxes(0)
                .x());
  ASSERT_EQ(link.content_attributes()
                .geometry()
                .fragment_visible_bounding_boxes(0)
                .y(),
            link_text.content_attributes()
                .geometry()
                .fragment_visible_bounding_boxes(0)
                .y());
  ASSERT_EQ(link.content_attributes()
                .geometry()
                .fragment_visible_bounding_boxes(0)
                .width(),
            link_text.content_attributes()
                .geometry()
                .fragment_visible_bounding_boxes(0)
                .width());
  ASSERT_EQ(link.content_attributes()
                .geometry()
                .fragment_visible_bounding_boxes(0)
                .height(),
            link_text.content_attributes()
                .geometry()
                .fragment_visible_bounding_boxes(0)
                .height());
  ASSERT_EQ(link.content_attributes()
                .geometry()
                .fragment_visible_bounding_boxes(1)
                .x(),
            link_text.content_attributes()
                .geometry()
                .fragment_visible_bounding_boxes(1)
                .x());
  ASSERT_EQ(link.content_attributes()
                .geometry()
                .fragment_visible_bounding_boxes(1)
                .y(),
            link_text.content_attributes()
                .geometry()
                .fragment_visible_bounding_boxes(1)
                .y());
  ASSERT_EQ(link.content_attributes()
                .geometry()
                .fragment_visible_bounding_boxes(1)
                .width(),
            link_text.content_attributes()
                .geometry()
                .fragment_visible_bounding_boxes(1)
                .width());
  ASSERT_EQ(link.content_attributes()
                .geometry()
                .fragment_visible_bounding_boxes(1)
                .height(),
            link_text.content_attributes()
                .geometry()
                .fragment_visible_bounding_boxes(1)
                .height());

  ASSERT_EQ(after_text.content_attributes()
                .geometry()
                .fragment_visible_bounding_boxes()
                .size(),
            2);

  // The second fragment bounding box starts below the first.
  ASSERT_LT(after_text.content_attributes()
                .geometry()
                .fragment_visible_bounding_boxes(0)
                .y(),
            after_text.content_attributes()
                .geometry()
                .fragment_visible_bounding_boxes(1)
                .y());
}

IN_PROC_BROWSER_TEST_F(PageContentProtoProviderBrowserTest, DisabledButton) {
  LoadPage(https_server()->GetURL("/disabled_button.html"),
           GetActionableAIPageContentOptions());
  EXPECT_EQ(page_content().version(),
            optimization_guide::proto::
                ANNOTATED_PAGE_CONTENT_VERSION_ONLY_ACTIONABLE_ELEMENTS_1_0);

  EXPECT_EQ(ActionableContentRootNode().children_nodes().size(), 1);
  const auto& button = ActionableContentRootNode().children_nodes()[0];
  ASSERT_TRUE(button.content_attributes().has_interaction_info());
  EXPECT_TRUE(button.content_attributes()
                  .interaction_info()
                  .debug_clickability_reasons()
                  .empty());
  EXPECT_TRUE(button.content_attributes().interaction_info().is_disabled());
  EXPECT_FALSE(button.content_attributes().interaction_info().is_clickable());
}

IN_PROC_BROWSER_TEST_F(PageContentProtoProviderBrowserTest,
                       AriaDisabledButton) {
  LoadPage(https_server()->GetURL("/aria_disabled_button.html"),
           GetActionableAIPageContentOptions());
  EXPECT_EQ(page_content().version(),
            optimization_guide::proto::
                ANNOTATED_PAGE_CONTENT_VERSION_ONLY_ACTIONABLE_ELEMENTS_1_0);

  EXPECT_EQ(ActionableContentRootNode().children_nodes().size(), 1);
  const auto& button = ActionableContentRootNode().children_nodes()[0];
  ASSERT_TRUE(button.content_attributes().has_interaction_info());
  EXPECT_TRUE(button.content_attributes()
                  .interaction_info()
                  .debug_clickability_reasons()
                  .empty());
  EXPECT_TRUE(button.content_attributes().interaction_info().is_disabled());
  EXPECT_FALSE(button.content_attributes().interaction_info().is_clickable());
}

// Popups may be rendered as native OS-level widgets on Android, MacOS, and iOS.
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_IOS)
class PageContentProtoProviderPopupBrowserTest
    : public PageContentProtoProviderBrowserTest {
 public:
  PageContentProtoProviderPopupBrowserTest() {
    feature_list_.InitAndEnableFeature(
        blink::features::kAIPageContentIncludePopupWindows);
  }
  ~PageContentProtoProviderPopupBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

// TODO: b/450618828 - Enable on Fuchsia with proper geometry comparison.
#if !BUILDFLAG(IS_FUCHSIA)
IN_PROC_BROWSER_TEST_F(PageContentProtoProviderPopupBrowserTest,
                       SelectInMainFrame) {
  LoadPage(https_server()->GetURL("/open_popup.html"));

  content::ShowPopupWidgetWaiter new_popup_waiter(
      web_contents(), web_contents()->GetPrimaryMainFrame());
  ASSERT_TRUE(content::ExecJs(
      web_contents(), "document.getElementById('select_input').showPicker();"));
  new_popup_waiter.Wait();

  LoadData(GetActionableAIPageContentOptions());
  ASSERT_TRUE(page_content().has_popup_window());

  const auto& popup_window = page_content().popup_window();
  EXPECT_EQ(popup_window.opener_document_id().serialized_token(),
            page_content()
                .main_frame_data()
                .document_identifier()
                .serialized_token());

  const auto& select_node = ActionableContentRootNode().children_nodes()[0];
  EXPECT_EQ(select_node.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_FORM_CONTROL);
  EXPECT_EQ(popup_window.opener_common_ancestor_dom_node_id(),
            select_node.content_attributes().common_ancestor_dom_node_id());

  const auto& popup_root = ContentRootNodeForFrameActionableMode(
      popup_window.root_node().children_nodes()[0]);

  const auto& select_node_in_popup = popup_root.children_nodes()[0];
  EXPECT_EQ(select_node_in_popup.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_FORM_CONTROL);
  const auto& select_options_in_popup =
      select_node_in_popup.content_attributes()
          .form_control_data()
          .select_options();
  ASSERT_EQ(select_options_in_popup.size(), 2);
  EXPECT_EQ(select_options_in_popup[0].text(), "Option 1");
  EXPECT_EQ(select_options_in_popup[1].text(), "Option 2");
  EXPECT_GT(select_node_in_popup.content_attributes()
                .interaction_info()
                .document_scoped_z_order(),
            0);

  const auto& select_node_geometry =
      select_node.content_attributes().geometry();
  const auto& select_node_in_popup_geometry =
      select_node_in_popup.content_attributes().geometry();
  // The y value is the bottom edge of the form element. The height of the form
  // element is 10px.
  EXPECT_EQ(select_node_in_popup_geometry.outer_bounding_box().x(),
            select_node_geometry.outer_bounding_box().x());
  EXPECT_EQ(select_node_in_popup_geometry.outer_bounding_box().y(),
            select_node_geometry.outer_bounding_box().y() + 10);
  EXPECT_EQ(select_node_in_popup_geometry.visible_bounding_box().x(),
            select_node_geometry.visible_bounding_box().x());
  EXPECT_EQ(select_node_in_popup_geometry.visible_bounding_box().y(),
            select_node_geometry.visible_bounding_box().y() + 10);

  EXPECT_EQ(popup_window.visible_bounding_box().x(),
            select_node_geometry.outer_bounding_box().x());
  EXPECT_EQ(popup_window.visible_bounding_box().y(),
            select_node_geometry.outer_bounding_box().y() + 10);
}

IN_PROC_BROWSER_TEST_F(PageContentProtoProviderPopupBrowserTest,
                       SelectInSameOriginIframe) {
  LoadPage(https_server()->GetURL("a.com", "/open_popup_iframe.html"));

  content::RenderFrameHost* iframe =
      content::ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);

  content::ShowPopupWidgetWaiter new_popup_waiter(web_contents(), iframe);
  ASSERT_TRUE(content::ExecJs(
      iframe, "document.getElementById('select_input').showPicker();"));
  new_popup_waiter.Wait();

  LoadData(GetActionableAIPageContentOptions());
  ASSERT_TRUE(page_content().has_popup_window());

  const auto& popup_window = page_content().popup_window();

  const auto& iframe_node = ActionableContentRootNode().children_nodes()[0];
  EXPECT_EQ(popup_window.opener_document_id().serialized_token(),
            iframe_node.content_attributes()
                .iframe_data()
                .frame_data()
                .document_identifier()
                .serialized_token());

  EXPECT_EQ(iframe_node.children_nodes().size(), 1);
  const auto& iframe_node_root =
      ContentRootNodeForFrameActionableMode(iframe_node.children_nodes()[0]);
  const auto& select_node = iframe_node_root.children_nodes()[0];
  EXPECT_EQ(select_node.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_FORM_CONTROL);
  EXPECT_EQ(popup_window.opener_common_ancestor_dom_node_id(),
            select_node.content_attributes().common_ancestor_dom_node_id());

  const auto& popup_root = ContentRootNodeForFrameActionableMode(
      popup_window.root_node().children_nodes()[0]);
  const auto& select_node_in_popup = popup_root.children_nodes()[0];
  EXPECT_EQ(select_node_in_popup.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_FORM_CONTROL);
  const auto& select_options_in_popup =
      select_node_in_popup.content_attributes()
          .form_control_data()
          .select_options();
  ASSERT_EQ(select_options_in_popup.size(), 2);
  EXPECT_EQ(select_options_in_popup[0].text(), "Option 1");
  EXPECT_EQ(select_options_in_popup[1].text(), "Option 2");
  EXPECT_GT(select_node_in_popup.content_attributes()
                .interaction_info()
                .document_scoped_z_order(),
            0);

  const auto& select_node_geometry =
      select_node.content_attributes().geometry();
  const auto& select_node_in_popup_geometry =
      select_node_in_popup.content_attributes().geometry();
  // The y value is the bottom edge of the form element. The height of the form
  // element is 10px.
  EXPECT_EQ(select_node_in_popup_geometry.outer_bounding_box().x(),
            select_node_geometry.outer_bounding_box().x());
  EXPECT_EQ(select_node_in_popup_geometry.outer_bounding_box().y(),
            select_node_geometry.outer_bounding_box().y() + 10);
  EXPECT_EQ(select_node_in_popup_geometry.visible_bounding_box().x(),
            select_node_geometry.visible_bounding_box().x());
  EXPECT_EQ(select_node_in_popup_geometry.visible_bounding_box().y(),
            select_node_geometry.visible_bounding_box().y() + 10);

  EXPECT_EQ(popup_window.visible_bounding_box().x(),
            select_node_geometry.outer_bounding_box().x());
  EXPECT_EQ(popup_window.visible_bounding_box().y(),
            select_node_geometry.outer_bounding_box().y() + 10);
}

IN_PROC_BROWSER_TEST_F(PageContentProtoProviderPopupBrowserTest,
                       SelectInCrossOriginIframe) {
  LoadPage(https_server()->GetURL(
      "a.com", "/open_popup_iframe.html?domain=/cross-site/b.com/"));

  content::RenderFrameHost* iframe =
      content::ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);

  content::ShowPopupWidgetWaiter new_popup_waiter(web_contents(), iframe);

  // showPicker() is not allowed from cross-origin iframe for security reasons,
  // therefore simulating a user click.
  SimulateMouseClickAt(
      iframe->GetRenderWidgetHost(),
      GetCenterCoordinatesOfElementWithId(iframe, "select_input"));
  new_popup_waiter.Wait();

  LoadData(GetActionableAIPageContentOptions());
  ASSERT_TRUE(page_content().has_popup_window());

  const auto& popup_window = page_content().popup_window();

  const auto& iframe_node = ActionableContentRootNode().children_nodes()[0];
  EXPECT_EQ(popup_window.opener_document_id().serialized_token(),
            iframe_node.content_attributes()
                .iframe_data()
                .frame_data()
                .document_identifier()
                .serialized_token());

  EXPECT_EQ(iframe_node.children_nodes().size(), 1);
  const auto& iframe_node_root =
      ContentRootNodeForFrameActionableMode(iframe_node.children_nodes()[0]);
  const auto& select_node = iframe_node_root.children_nodes()[0];
  EXPECT_EQ(select_node.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_FORM_CONTROL);
  EXPECT_EQ(popup_window.opener_common_ancestor_dom_node_id(),
            select_node.content_attributes().common_ancestor_dom_node_id());

  const auto& popup_root = ContentRootNodeForFrameActionableMode(
      popup_window.root_node().children_nodes()[0]);
  const auto& select_node_in_popup = popup_root.children_nodes()[0];
  EXPECT_EQ(select_node_in_popup.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_FORM_CONTROL);

  const auto& select_node_geometry =
      select_node.content_attributes().geometry();
  const auto& select_node_in_popup_geometry =
      select_node_in_popup.content_attributes().geometry();
  // The y value is the bottom edge of the form element. The height of the form
  // element is 10px.
  EXPECT_EQ(select_node_in_popup_geometry.outer_bounding_box().x(),
            select_node_geometry.outer_bounding_box().x());
  EXPECT_EQ(select_node_in_popup_geometry.outer_bounding_box().y(),
            select_node_geometry.outer_bounding_box().y() + 10);
  EXPECT_EQ(select_node_in_popup_geometry.visible_bounding_box().x(),
            select_node_geometry.visible_bounding_box().x());
  EXPECT_EQ(select_node_in_popup_geometry.visible_bounding_box().y(),
            select_node_geometry.visible_bounding_box().y() + 10);

  EXPECT_EQ(popup_window.visible_bounding_box().x(),
            select_node_geometry.outer_bounding_box().x());
  EXPECT_EQ(popup_window.visible_bounding_box().y(),
            select_node_geometry.outer_bounding_box().y() + 10);
}
#endif  // !BUILDFLAG(IS_FUCHSIA)

IN_PROC_BROWSER_TEST_F(PageContentProtoProviderPopupBrowserTest, ColorPicker) {
  LoadPage(https_server()->GetURL("/open_popup.html"), nullptr);

  content::ShowPopupWidgetWaiter new_popup_waiter(
      web_contents(), web_contents()->GetPrimaryMainFrame());
  ASSERT_TRUE(content::ExecJs(
      web_contents(), "document.getElementById('color_input').click();"));
  new_popup_waiter.Wait();

  LoadData();
  ASSERT_TRUE(page_content().has_popup_window());

  const auto& popup_window = page_content().popup_window();
  EXPECT_EQ(popup_window.opener_document_id().serialized_token(),
            page_content()
                .main_frame_data()
                .document_identifier()
                .serialized_token());

  const auto& color_node = page_content().root_node().children_nodes()[1];
  EXPECT_EQ(popup_window.opener_common_ancestor_dom_node_id(),
            color_node.content_attributes().common_ancestor_dom_node_id());
}
#endif

// Overrides the AIPageContentAgent interface for the given frame to simulate a
// non-responsive renderer. Saves the arguments to respond later.
class NoResponseAIPageContentAgent
    : public blink::mojom::AIPageContentAgentInterceptorForTesting {
 public:
  explicit NoResponseAIPageContentAgent(
      content::RenderFrameHost* render_frame_host)
      : render_frame_host_(render_frame_host) {
    service_manager::InterfaceProvider::TestApi(
        render_frame_host_->GetRemoteInterfaces())
        .SetBinderForName(
            blink::mojom::AIPageContentAgent::Name_,
            base::BindRepeating(&NoResponseAIPageContentAgent::Bind,
                                base::Unretained(this)));
  }
  ~NoResponseAIPageContentAgent() override = default;

  void Bind(mojo::ScopedMessagePipeHandle handle) {
    receiver_.Bind(mojo::PendingReceiver<blink::mojom::AIPageContentAgent>(
        std::move(handle)));
  }

  void GetAIPageContent(blink::mojom::AIPageContentOptionsPtr options,
                        GetAIPageContentCallback callback) override {
    // Do nothing, simulating a non-responsive renderer, but save the arguments
    // to allow later processing.
    saved_options_ = std::move(options);
    saved_callback_ = std::move(callback);
  }

  void Respond() {
    service_manager::InterfaceProvider::TestApi test_api(
        render_frame_host_->GetRemoteInterfaces());

    CHECK(test_api.HasBinderForName(blink::mojom::AIPageContentAgent::Name_));
    test_api.ClearBinderForName(blink::mojom::AIPageContentAgent::Name_);

    render_frame_host_->GetRemoteInterfaces()->GetInterface(
        agent_.BindNewPipeAndPassReceiver());
    agent_->GetAIPageContent(std::move(saved_options_),
                             std::move(saved_callback_));
  }

  blink::mojom::AIPageContentAgent* GetForwardingInterface() override {
    NOTREACHED();
  }

 private:
  blink::mojom::AIPageContentOptionsPtr saved_options_;
  GetAIPageContentCallback saved_callback_;
  raw_ptr<content::RenderFrameHost> render_frame_host_;
  mojo::Receiver<blink::mojom::AIPageContentAgent> receiver_{this};
  mojo::Remote<blink::mojom::AIPageContentAgent> agent_;
};

class PageContentProtoProviderSubframeTimeoutBrowserTest
    : public PageContentProtoProviderBrowserTestMultiProcess {
 public:
  PageContentProtoProviderSubframeTimeoutBrowserTest() {
    // Shorter timeout for quicker tests
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kGetAIPageContentSubframeTimeoutEnabled,
        {{"timeout", "100ms"}});
  }

  base::TimeDelta GetTimeout() { return base::Milliseconds(100); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         PageContentProtoProviderSubframeTimeoutBrowserTest,
                         testing::Bool());

class PageContentProtoProviderSubframeTimeoutDisabledBrowserTest
    : public PageContentProtoProviderBrowserTestMultiProcess {
 public:
  PageContentProtoProviderSubframeTimeoutDisabledBrowserTest() {
    scoped_feature_list_.InitAndDisableFeature(
        features::kGetAIPageContentSubframeTimeoutEnabled);
  }

  base::TimeDelta GetTimeout() { return base::Milliseconds(100); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    PageContentProtoProviderSubframeTimeoutDisabledBrowserTest,
    testing::Bool());

IN_PROC_BROWSER_TEST_P(PageContentProtoProviderSubframeTimeoutBrowserTest,
                       IFrameAIPageContentAgentRespondsSlowly) {
  // Load the page, but don't load the APC yet.
  LoadPage(https_server()->GetURL("a.com", "/iframe_cross_site.html"),
           /*options=*/nullptr);

  // Make the second child frame non-responsive.
  content::RenderFrameHost* child_frame_2 =
      ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 1);
  ASSERT_TRUE(child_frame_2);
  NoResponseAIPageContentAgent no_response_agent(child_frame_2);

  base::TimeTicks start = base::TimeTicks::Now();

  // Request the APC for the main frame.
  LoadData(GetActionableAIPageContentOptions());

  base::TimeDelta elapsed_time = base::TimeTicks::Now() - start;

  // The APC should have waited for the subframe to timeout.
  EXPECT_GE(elapsed_time, GetTimeout());

  const auto& root_node = ActionableContentRootNode();
  EXPECT_EQ(root_node.children_nodes().size(), 2);

  const auto& b_frame = root_node.children_nodes()[0];
  EXPECT_EQ(b_frame.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME);
  const auto& b_frame_data = b_frame.content_attributes().iframe_data();
  AssertValidOrigin(b_frame_data.frame_data().security_origin(),
                    ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0)
                        ->GetLastCommittedOrigin());
  EXPECT_FALSE(b_frame.content_attributes().is_ad_related());

  const auto& c_frame = root_node.children_nodes()[1];
  EXPECT_EQ(c_frame.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME);

  // We didn't wait for the frame to respond, so the iframe data is defaulted.
  const auto& c_frame_data = c_frame.content_attributes().iframe_data();
  EXPECT_FALSE(c_frame_data.frame_data().has_security_origin());
  EXPECT_EQ(c_frame.children_nodes_size(), 0);
  EXPECT_FALSE(c_frame.content_attributes().is_ad_related());
}

IN_PROC_BROWSER_TEST_P(
    PageContentProtoProviderSubframeTimeoutDisabledBrowserTest,
    InFrameAIPageContentAgentRespondsSlowly) {
  // Load the page, but don't load the APC yet.
  LoadPage(https_server()->GetURL("a.com", "/iframe_cross_site.html"),
           /*options=*/nullptr);

  // Make the second child frame non-responsive.
  content::RenderFrameHost* child_frame_2 =
      ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 1);
  ASSERT_TRUE(child_frame_2);
  NoResponseAIPageContentAgent no_response_agent(child_frame_2);

  // Request the APC for the main frame, but don't wait for a response.
  base::RunLoop loading_run_loop;
  LoadData(GetActionableAIPageContentOptions(), loading_run_loop.QuitClosure());

  // Wait for the default timeout to pass.
  base::RunLoop timer_run_loop;
  base::OneShotTimer timer;
  base::TimeDelta default_timeout =
      features::kGetAIPageContentSubframeTimeoutParam.default_value;
  timer.Start(FROM_HERE, default_timeout, timer_run_loop.QuitClosure());
  timer_run_loop.Run();

  // The APC should still be pending as the timeout was disabled.
  EXPECT_FALSE(loading_run_loop.AnyQuitCalled());

  // Now, let the subframe respond.
  no_response_agent.Respond();
  loading_run_loop.Run();

  const auto& root_node = ActionableContentRootNode();
  EXPECT_EQ(root_node.children_nodes().size(), 2);

  const auto& b_frame = root_node.children_nodes()[0];
  EXPECT_EQ(b_frame.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME);
  const auto& b_frame_data = b_frame.content_attributes().iframe_data();
  AssertValidOrigin(b_frame_data.frame_data().security_origin(),
                    ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0)
                        ->GetLastCommittedOrigin());
  EXPECT_FALSE(b_frame.content_attributes().is_ad_related());

  const auto& c_frame = root_node.children_nodes()[1];
  EXPECT_EQ(c_frame.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME);
  const auto& c_frame_data = c_frame.content_attributes().iframe_data();
  AssertValidOrigin(c_frame_data.frame_data().security_origin(),
                    ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 1)
                        ->GetLastCommittedOrigin());
  EXPECT_FALSE(c_frame.content_attributes().is_ad_related());
}

IN_PROC_BROWSER_TEST_P(PageContentProtoProviderSubframeTimeoutBrowserTest,
                       MainFrameAIPageContentAgentRespondsSlowly) {
  // Load the page, but don't load the APC yet.
  LoadPage(https_server()->GetURL("a.com", "/iframe_cross_site.html"),
           /*options=*/nullptr);

  // Make the main frame non-responsive.
  NoResponseAIPageContentAgent no_response_agent(
      web_contents()->GetPrimaryMainFrame());

  // Request the APC for the main frame, but don't wait for a response.
  base::RunLoop loading_run_loop;
  LoadData(GetActionableAIPageContentOptions(), loading_run_loop.QuitClosure());

  // Wait for the timeout time to pass.
  base::RunLoop timer_run_loop;
  base::OneShotTimer timer;
  timer.Start(FROM_HERE, GetTimeout(), timer_run_loop.QuitClosure());
  timer_run_loop.Run();

  // The APC should still be pending as the timeout only applies to subframes.
  EXPECT_FALSE(loading_run_loop.AnyQuitCalled());

  // Now, let the main frame respond.
  no_response_agent.Respond();
  loading_run_loop.Run();

  const auto& root_node = ActionableContentRootNode();
  EXPECT_EQ(root_node.children_nodes().size(), 2);

  const auto& b_frame = root_node.children_nodes()[0];
  EXPECT_EQ(b_frame.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME);
  const auto& b_frame_data = b_frame.content_attributes().iframe_data();
  AssertValidOrigin(b_frame_data.frame_data().security_origin(),
                    ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0)
                        ->GetLastCommittedOrigin());
  EXPECT_FALSE(b_frame.content_attributes().is_ad_related());

  const auto& c_frame = root_node.children_nodes()[1];
  EXPECT_EQ(c_frame.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME);
  const auto& c_frame_data = c_frame.content_attributes().iframe_data();
  AssertValidOrigin(c_frame_data.frame_data().security_origin(),
                    ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 1)
                        ->GetLastCommittedOrigin());
  EXPECT_FALSE(c_frame.content_attributes().is_ad_related());
}

}  // namespace

}  // namespace optimization_guide
