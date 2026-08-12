// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/page_content_image_extractor.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/containers/span.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/timer/elapsed_timer.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/optimization_guide/content/browser/page_content_test_utils.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom.h"

namespace optimization_guide {

class PageContentImageExtractorBrowserTest
    : public content::ContentBrowserTest {
 public:
  PageContentImageExtractorBrowserTest() = default;
  ~PageContentImageExtractorBrowserTest() override = default;

  void SetUpOnMainThread() override {
    content::ContentBrowserTest::SetUpOnMainThread();

    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->AddDefaultHandlers(base::FilePath(
        FILE_PATH_LITERAL("components/test/data/optimization_guide")));
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  content::WebContents* web_contents() const { return shell()->web_contents(); }
};

IN_PROC_BROWSER_TEST_F(PageContentImageExtractorBrowserTest, GetImageBytes) {
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(), embedded_test_server()->GetURL("/simple.html")));

  const std::string kImageBase64 =
      "R0lGODlhAQABAIAAAAAAAP///yH5BAEAAAAALAAAAAABAAEAAAIBRAA7";
  const std::string kImageSrc = "data:image/gif;base64," + kImageBase64;
  ASSERT_TRUE(content::ExecJs(
      web_contents(),
      base::StringPrintf("const img = document.createElement('img');"
                         "img.src = '%s';"
                         "img.alt = 'test_image_bytes';"
                         "document.body.appendChild(img);",
                         kImageSrc.c_str())));

  ASSERT_TRUE(
      content::EvalJs(web_contents(),
                      "new Promise(resolve => {"
                      "  const img = "
                      "document.querySelector('img[alt=\"test_image_bytes\"]');"
                      "  if (img.complete) { resolve(true); }"
                      "  else { img.onload = () => resolve(true); }"
                      "})")
          .ExtractBool());

  // Wait for rendering to sync.
  {
    base::test::TestFuture<bool> future;
    web_contents()
        ->GetPrimaryMainFrame()
        ->GetRenderWidgetHost()
        ->InsertVisualStateCallback(future.GetCallback());
    ASSERT_TRUE(future.Wait());
  }

  // Get the AI page content to extract the DOM node ID of the image.
  base::test::TestFuture<AIPageContentResultOrError> content_future;
  GetAIPageContent(web_contents(),
                   ActionableAIPageContentOptions(
                       /*on_critical_path =*/true),
                   content_future.GetCallback());

  auto result = content_future.Take();
  ASSERT_TRUE(result.has_value());

  const proto::ContentNode* image_node = FindFirstNodeWithAttributeType(
      result->proto.root_node(), proto::CONTENT_ATTRIBUTE_IMAGE);
  ASSERT_TRUE(image_node);
  int32_t dom_node_id =
      image_node->content_attributes().common_ancestor_dom_node_id();
  ASSERT_NE(dom_node_id, 0);

  // Get the document identifier for the main frame.
  std::optional<std::string> document_identifier =
      DocumentIdentifierUserData::GetDocumentIdentifier(
          web_contents()->GetPrimaryMainFrame()->GetGlobalFrameToken());
  ASSERT_TRUE(document_identifier.has_value());

  base::HistogramTester histogram_tester;
  base::ScopedMockElapsedTimersForTest mock_elapsed_timers;

  base::test::TestFuture<blink::mojom::AIPageContentImageBytesResultPtr>
      image_future;
  GetImageBytes(web_contents(), *document_identifier, dom_node_id,
                image_future.GetCallback());

  auto image_bytes_result = image_future.Take();
  ASSERT_TRUE(image_bytes_result);
  std::optional<std::vector<uint8_t>> expected_bytes =
      base::Base64Decode(kImageBase64);
  ASSERT_TRUE(expected_bytes.has_value());
  EXPECT_EQ(image_bytes_result->image_bytes.size(), expected_bytes->size());
  EXPECT_EQ(base::span<const uint8_t>(image_bytes_result->image_bytes),
            base::span<const uint8_t>(*expected_bytes));
  EXPECT_EQ(image_bytes_result->image_info->mime_type, "image/gif");
  ASSERT_TRUE(image_bytes_result->image_info);
  EXPECT_EQ(image_bytes_result->image_info->image_caption, "test_image_bytes");

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.AIPageContent.GetImageBytes.Timeout", false, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.AIPageContent.GetImageBytes.Size",
      image_bytes_result->image_bytes.size(), 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.AIPageContent.GetImageBytes.Latency",
      base::ScopedMockElapsedTimersForTest::kMockElapsedTime.InMilliseconds(),
      1);
}

// Test `GetImageBytes` works on .webp image.
IN_PROC_BROWSER_TEST_F(PageContentImageExtractorBrowserTest,
                       GetImageBytes_Webp) {
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(), embedded_test_server()->GetURL("/simple.html")));

  // 1x1 WebP image in Base64 encoding.
  const std::string kImageBase64 =
      "UklGRiQAAABXRUJQVlA4IBgAAAAwAQCdASoBAAEAAwA0JaQAA3AA/vuUAAA=";
  const std::string kImageSrc = "data:image/webp;base64," + kImageBase64;
  ASSERT_TRUE(content::ExecJs(
      web_contents(),
      base::StringPrintf("const img = document.createElement('img');"
                         "img.src = '%s';"
                         "img.alt = 'test_image_bytes';"
                         "document.body.appendChild(img);",
                         kImageSrc.c_str())));

  ASSERT_TRUE(
      content::EvalJs(web_contents(),
                      "new Promise(resolve => {"
                      "  const img = "
                      "document.querySelector('img[alt=\"test_image_bytes\"]');"
                      "  if (img.complete) { resolve(true); }"
                      "  else { img.onload = () => resolve(true); }"
                      "})")
          .ExtractBool());

  // Wait for rendering to sync.
  {
    base::test::TestFuture<bool> future;
    web_contents()
        ->GetPrimaryMainFrame()
        ->GetRenderWidgetHost()
        ->InsertVisualStateCallback(future.GetCallback());
    ASSERT_TRUE(future.Wait());
  }

  // Get the AI page content to extract the DOM node ID of the image.
  base::test::TestFuture<AIPageContentResultOrError> content_future;
  GetAIPageContent(web_contents(),
                   ActionableAIPageContentOptions(
                       /*on_critical_path =*/true),
                   content_future.GetCallback());

  auto result = content_future.Take();
  ASSERT_TRUE(result.has_value());

  const proto::ContentNode* image_node = FindFirstNodeWithAttributeType(
      result->proto.root_node(), proto::CONTENT_ATTRIBUTE_IMAGE);
  ASSERT_TRUE(image_node);
  int32_t dom_node_id =
      image_node->content_attributes().common_ancestor_dom_node_id();
  ASSERT_NE(dom_node_id, 0);

  // Get the document identifier for the main frame.
  std::optional<std::string> document_identifier =
      DocumentIdentifierUserData::GetDocumentIdentifier(
          web_contents()->GetPrimaryMainFrame()->GetGlobalFrameToken());
  ASSERT_TRUE(document_identifier.has_value());

  base::HistogramTester histogram_tester;
  base::ScopedMockElapsedTimersForTest mock_elapsed_timers;

  base::test::TestFuture<blink::mojom::AIPageContentImageBytesResultPtr>
      image_future;
  GetImageBytes(web_contents(), *document_identifier, dom_node_id,
                image_future.GetCallback());

  auto image_bytes_result = image_future.Take();
  ASSERT_TRUE(image_bytes_result);
  std::optional<std::vector<uint8_t>> expected_bytes =
      base::Base64Decode(kImageBase64);
  ASSERT_TRUE(expected_bytes.has_value());
  EXPECT_EQ(image_bytes_result->image_bytes.size(), expected_bytes->size());
  EXPECT_EQ(base::span<const uint8_t>(image_bytes_result->image_bytes),
            base::span<const uint8_t>(*expected_bytes));
  ASSERT_TRUE(image_bytes_result->image_info);
  EXPECT_EQ(image_bytes_result->image_info->mime_type, "image/webp");
  EXPECT_EQ(image_bytes_result->image_info->image_caption, "test_image_bytes");

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.AIPageContent.GetImageBytes.Timeout", false, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.AIPageContent.GetImageBytes.Size",
      image_bytes_result->image_bytes.size(), 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.AIPageContent.GetImageBytes.Latency",
      base::ScopedMockElapsedTimersForTest::kMockElapsedTime.InMilliseconds(),
      1);
}

IN_PROC_BROWSER_TEST_F(PageContentImageExtractorBrowserTest,
                       GetImageBytes_FromIframe) {
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(), embedded_test_server()->GetURL("/simple.html")));

  ASSERT_TRUE(content::ExecJs(web_contents(),
                              "const iframe = document.createElement('iframe');"
                              "iframe.id = 'test_iframe';"
                              "document.body.appendChild(iframe);"));

  content::RenderFrameHost* child_rfh =
      content::ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(child_rfh);

  const std::string kImageBase64 =
      "R0lGODlhAQABAIAAAAAAAP///yH5BAEAAAAALAAAAAABAAEAAAIBRAA7";
  const std::string kImageSrc = "data:image/gif;base64," + kImageBase64;
  ASSERT_TRUE(content::ExecJs(
      child_rfh, base::StringPrintf("const img = document.createElement('img');"
                                    "img.src = '%s';"
                                    "img.alt = 'test_image_bytes';"
                                    "document.body.appendChild(img);",
                                    kImageSrc.c_str())));

  ASSERT_TRUE(
      content::EvalJs(child_rfh,
                      "new Promise(resolve => {"
                      "  const img = "
                      "document.querySelector('img[alt=\"test_image_bytes\"]');"
                      "  if (img.complete) { resolve(true); }"
                      "  else { img.onload = () => resolve(true); }"
                      "})")
          .ExtractBool());

  // Wait for rendering to sync.
  {
    base::test::TestFuture<bool> future;
    child_rfh->GetRenderWidgetHost()->InsertVisualStateCallback(
        future.GetCallback());
    ASSERT_TRUE(future.Wait());
  }

  // Get the AI page content to extract the DOM node ID of the image.
  base::test::TestFuture<AIPageContentResultOrError> content_future;
  GetAIPageContent(web_contents(),
                   ActionableAIPageContentOptions(
                       /*on_critical_path =*/true),
                   content_future.GetCallback());

  auto result = content_future.Take();
  ASSERT_TRUE(result.has_value());

  const proto::ContentNode* image_node = FindFirstNodeWithAttributeType(
      result->proto.root_node(), proto::CONTENT_ATTRIBUTE_IMAGE);
  ASSERT_TRUE(image_node);
  int32_t dom_node_id =
      image_node->content_attributes().common_ancestor_dom_node_id();
  ASSERT_NE(dom_node_id, 0);

  // Get the document identifier for the child frame.
  std::optional<std::string> child_document_identifier =
      DocumentIdentifierUserData::GetDocumentIdentifier(
          child_rfh->GetGlobalFrameToken());
  ASSERT_TRUE(child_document_identifier.has_value());

  base::HistogramTester histogram_tester;
  base::ScopedMockElapsedTimersForTest mock_elapsed_timers;

  base::test::TestFuture<blink::mojom::AIPageContentImageBytesResultPtr>
      image_future;
  GetImageBytes(web_contents(), *child_document_identifier, dom_node_id,
                image_future.GetCallback());

  auto image_bytes_result = image_future.Take();
  ASSERT_TRUE(image_bytes_result);
  std::optional<std::vector<uint8_t>> expected_bytes =
      base::Base64Decode(kImageBase64);
  ASSERT_TRUE(expected_bytes.has_value());
  EXPECT_EQ(image_bytes_result->image_bytes.size(), expected_bytes->size());
  EXPECT_EQ(base::span<const uint8_t>(image_bytes_result->image_bytes),
            base::span<const uint8_t>(*expected_bytes));
  EXPECT_EQ(image_bytes_result->image_info->mime_type, "image/gif");
  ASSERT_TRUE(image_bytes_result->image_info);
  EXPECT_EQ(image_bytes_result->image_info->image_caption, "test_image_bytes");

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.AIPageContent.GetImageBytes.Timeout", false, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.AIPageContent.GetImageBytes.Size",
      image_bytes_result->image_bytes.size(), 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.AIPageContent.GetImageBytes.Latency",
      base::ScopedMockElapsedTimersForTest::kMockElapsedTime.InMilliseconds(),
      1);
}

class PageContentImageExtractorTimeoutBrowserTest
    : public PageContentImageExtractorBrowserTest {
 public:
  PageContentImageExtractorTimeoutBrowserTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kGetAIPageContentGetImageBytesTimeoutEnabled,
        {{"timeout", "0s"}});
  }
  ~PageContentImageExtractorTimeoutBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(PageContentImageExtractorTimeoutBrowserTest,
                       GetImageBytes_Timeout) {
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(), embedded_test_server()->GetURL("/simple.html")));

  const std::string kImageBase64 =
      "R0lGODlhAQABAIAAAAAAAP///yH5BAEAAAAALAAAAAABAAEAAAIBRAA7";
  const std::string kImageSrc = "data:image/gif;base64," + kImageBase64;
  ASSERT_TRUE(content::ExecJs(
      web_contents(),
      base::StringPrintf("const img = document.createElement('img');"
                         "img.src = '%s';"
                         "img.alt = 'test_image_bytes';"
                         "document.body.appendChild(img);",
                         kImageSrc.c_str())));

  ASSERT_TRUE(
      content::EvalJs(web_contents(),
                      "new Promise(resolve => {"
                      "  const img = "
                      "document.querySelector('img[alt=\"test_image_bytes\"]');"
                      "  if (img.complete) { resolve(true); }"
                      "  else { img.onload = () => resolve(true); }"
                      "})")
          .ExtractBool());

  // Wait for rendering to sync.
  {
    base::test::TestFuture<bool> future;
    web_contents()
        ->GetPrimaryMainFrame()
        ->GetRenderWidgetHost()
        ->InsertVisualStateCallback(future.GetCallback());
    ASSERT_TRUE(future.Wait());
  }

  // Get the AI page content to extract the DOM node ID of the image.
  base::test::TestFuture<AIPageContentResultOrError> content_future;
  GetAIPageContent(web_contents(),
                   ActionableAIPageContentOptions(
                       /*on_critical_path =*/true),
                   content_future.GetCallback());

  auto result = content_future.Take();
  ASSERT_TRUE(result.has_value());

  const proto::ContentNode* image_node = FindFirstNodeWithAttributeType(
      result->proto.root_node(), proto::CONTENT_ATTRIBUTE_IMAGE);
  ASSERT_TRUE(image_node);
  int32_t dom_node_id =
      image_node->content_attributes().common_ancestor_dom_node_id();
  ASSERT_NE(dom_node_id, 0);

  // Get the document identifier for the main frame.
  std::optional<std::string> document_identifier =
      DocumentIdentifierUserData::GetDocumentIdentifier(
          web_contents()->GetPrimaryMainFrame()->GetGlobalFrameToken());
  ASSERT_TRUE(document_identifier.has_value());

  base::HistogramTester histogram_tester;

  base::test::TestFuture<blink::mojom::AIPageContentImageBytesResultPtr>
      image_future;
  GetImageBytes(web_contents(), *document_identifier, dom_node_id,
                image_future.GetCallback());

  // The request will timeout on the browser side immediately because timeout is
  // 0s.
  auto image_bytes_result = image_future.Take();
  EXPECT_FALSE(image_bytes_result);

  // Timeout should be true.
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.AIPageContent.GetImageBytes.Timeout", true, 1);

  // Latency should NOT be logged.
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.AIPageContent.GetImageBytes.Latency", 0);

  // Size should NOT be logged.
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.AIPageContent.GetImageBytes.Size", 0);
}

}  // namespace optimization_guide
