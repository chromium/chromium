// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/content/renderer/content_previews_render_frame_observer.h"

#include "base/test/gtest_util.h"
#include "content/public/common/previews_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_url_response.h"

namespace data_reduction_proxy {
class ContentPreviewsRenderFrameObserverTest {
 public:
  // Exposes private static method for tests.
  static bool ValidatePreviewsStateWithResponse(
      content::PreviewsState previews_state,
      const blink::WebURLResponse& web_url_response) {
    return ContentPreviewsRenderFrameObserver::
        ValidatePreviewsStateWithResponse(previews_state, web_url_response);
  }
};

TEST(ContentPreviewsRenderFrameObserverTest,
     ValidatePreviewsStateWithResponseNoHeaders) {
  blink::WebURLResponse response_no_headers;

  EXPECT_TRUE(
      ContentPreviewsRenderFrameObserverTest::ValidatePreviewsStateWithResponse(
          content::PREVIEWS_UNSPECIFIED, response_no_headers));
  EXPECT_TRUE(
      ContentPreviewsRenderFrameObserverTest::ValidatePreviewsStateWithResponse(
          content::NOSCRIPT_ON, response_no_headers));

  EXPECT_DCHECK_DEATH(
      ContentPreviewsRenderFrameObserverTest::ValidatePreviewsStateWithResponse(
          content::SERVER_LITE_PAGE_ON, response_no_headers));
}

TEST(ContentPreviewsRenderFrameObserverTest,
     ValidatePreviewsStateWithResponseLitePageHeader) {
  blink::WebURLResponse response_with_lite_page;
  response_with_lite_page.AddHttpHeaderField("chrome-proxy-content-transform",
                                             "lite-page");

  EXPECT_TRUE(
      ContentPreviewsRenderFrameObserverTest::ValidatePreviewsStateWithResponse(
          content::SERVER_LITE_PAGE_ON, response_with_lite_page));

  EXPECT_DCHECK_DEATH(
      ContentPreviewsRenderFrameObserverTest::ValidatePreviewsStateWithResponse(
          content::PREVIEWS_UNSPECIFIED, response_with_lite_page));
  EXPECT_DCHECK_DEATH(
      ContentPreviewsRenderFrameObserverTest::ValidatePreviewsStateWithResponse(
          content::NOSCRIPT_ON, response_with_lite_page));
}

}  // namespace data_reduction_proxy
