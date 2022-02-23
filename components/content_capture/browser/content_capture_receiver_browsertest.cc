// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_capture/browser/content_capture_test_helper.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/shell/browser/shell.h"

namespace content_capture {

static constexpr char kMainFrameUrl[] = "/title1.html";
static constexpr char kFencedFrameUrl[] = "/fenced_frames/title1.html";

class ContentCaptureBrowserTest : public content::ContentBrowserTest {
 public:
  void SetUpOnMainThread() override {
    content::ContentBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());

    // Navigate to the initial page.
    const GURL main_frame_url = embedded_test_server()->GetURL(kMainFrameUrl);
    ASSERT_TRUE(NavigateToURL(web_contents(), main_frame_url));
    main_frame_ = web_contents()->GetMainFrame();
    EXPECT_NE(nullptr, main_frame_);

    // Create a provider and add a consumer.
    helper_.CreateProviderAndConsumer(web_contents());

    // Bind sender with receiver for main frame.
    main_frame_sender_.Bind(main_frame_);

    // Create a fenced frame.
    const GURL fenced_frame_url =
        embedded_test_server()->GetURL(kFencedFrameUrl);
    fenced_frame_ =
        fenced_frame_helper_.CreateFencedFrame(main_frame_, fenced_frame_url);
    EXPECT_NE(nullptr, fenced_frame_);

    // Bind sender with receiver for fenced frame.
    fenced_frame_sender_.Bind(fenced_frame_);

    helper_.InitTestData(base::UTF8ToUTF16(main_frame_url.spec()),
                         base::UTF8ToUTF16(fenced_frame_url.spec()));
  }

  int64_t GetFrameId(bool main_frame) {
    return ContentCaptureReceiver::GetIdFrom(main_frame ? main_frame_.get()
                                                        : fenced_frame_.get());
  }

  content::WebContents* web_contents() { return shell()->web_contents(); }

  OnscreenContentProvider* provider() const {
    return helper_.onscreen_content_provider();
  }

  ContentCaptureConsumerHelper* consumer() const {
    return helper_.content_capture_consumer();
  }

  FakeContentCaptureSender* main_frame_sender() { return &main_frame_sender_; }
  FakeContentCaptureSender* fenced_frame_sender() {
    return &fenced_frame_sender_;
  }

  ContentCaptureTestHelper* helper() { return &helper_; }

 protected:
  ContentCaptureTestHelper helper_;

  raw_ptr<content::RenderFrameHost> main_frame_ = nullptr;
  raw_ptr<content::RenderFrameHost> fenced_frame_ = nullptr;

  FakeContentCaptureSender main_frame_sender_;
  FakeContentCaptureSender fenced_frame_sender_;

  content::test::FencedFrameTestHelper fenced_frame_helper_;
};

IN_PROC_BROWSER_TEST_F(ContentCaptureBrowserTest,
                       FencedFrameDidCaptureContent) {
  // 2 frames - main & fenced
  EXPECT_EQ(2u, provider()->GetFrameMapSizeForTesting());

  // Simulate to capture the content from main frame.
  main_frame_sender()->DidCaptureContent(helper()->test_data(),
                                         true /* first_data */);
  // Verifies to get test_data() with correct frame content id.
  EXPECT_TRUE(consumer()->parent_session().empty());
  EXPECT_TRUE(consumer()->removed_sessions().empty());
  EXPECT_EQ(GetExpectedTestData(helper()->test_data(),
                                GetFrameId(true /* main_frame */)),
            consumer()->captured_data());

  // Simulate to capture the content from fenced frame.
  fenced_frame_sender()->DidCaptureContent(helper()->test_data2(),
                                           true /* first_data */);
  // Verifies that the parent_session was set correctly.
  EXPECT_FALSE(consumer()->parent_session().empty());
  std::vector<ContentCaptureFrame> expected{GetExpectedTestData(
      helper()->test_data(), GetFrameId(true /* main_frame */))};
  VerifySession(expected, consumer()->parent_session());
  EXPECT_TRUE(consumer()->removed_sessions().empty());
  // Verifies that we receive the correct content from fenced frame.
  EXPECT_EQ(GetExpectedTestData(helper()->test_data2(),
                                GetFrameId(false /* main_frame */)),
            consumer()->captured_data());
}

}  // namespace content_capture
