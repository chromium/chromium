// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_reader.h"
#include "base/strings/utf_string_conversions.h"
#include "components/content_capture/browser/content_capture_test_helper.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
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
    main_frame_ = web_contents()->GetPrimaryMainFrame();
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

  void TearDownOnMainThread() override {
    main_frame_ = nullptr;
    fenced_frame_ = nullptr;
    content::ContentBrowserTest::TearDownOnMainThread();
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

// TODO(crbug.com/40285326): This fails with the field trial testing config.
class ContentCaptureBrowserTestNoTestingConfig
    : public ContentCaptureBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentCaptureBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch("disable-field-trial-config");
  }
};

IN_PROC_BROWSER_TEST_F(ContentCaptureBrowserTestNoTestingConfig,
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

IN_PROC_BROWSER_TEST_F(ContentCaptureBrowserTest,
                       DoNotUpdateFaviconURLsInFencedFrame) {
  // Simulate to capture the content from main frame.
  main_frame_sender()->DidCaptureContent(helper()->test_data(),
                                         true /* first_data */);

  // Simulate to capture the content from fenced frame.
  fenced_frame_sender()->DidCaptureContent(helper()->test_data2(),
                                           true /* first_data */);

  // Insert the favicon dynamically to the primary main frame.
  ASSERT_TRUE(ExecJs(web_contents(),
                     "let l = document.createElement('link');"
                     "l.rel='icon'; l.type='image/png'; "
                     "l.href='https://example.com/favicon.ico';"
                     "document.head.appendChild(l)"));

  std::string expected_json =
      R"JSON([{
          "type":"favicon",
          "url":"https://example.com/favicon.ico"
      }])JSON";
  std::optional<base::Value> expected = base::JSONReader::Read(expected_json);

  // Verify that the captured data's favicon url from the primary main frame is
  // valid.
  auto* main_frame_receiver =
      provider()->ContentCaptureReceiverForFrameForTesting(main_frame_);
  std::optional<base::Value> main_frame_actual = base::JSONReader::Read(
      main_frame_receiver->GetContentCaptureFrame().favicon);
  EXPECT_TRUE(main_frame_actual);
  EXPECT_EQ(expected, main_frame_actual);

  // Verify that the captured data's favicon url from the fenced frame is empty.
  auto* fenced_frame_receiver =
      provider()->ContentCaptureReceiverForFrameForTesting(fenced_frame_);
  EXPECT_FALSE(base::JSONReader::Read(
      fenced_frame_receiver->GetContentCaptureFrame().favicon));

  // Insert the favicon dynamically to the fenced frame.
  ASSERT_TRUE(ExecJs(fenced_frame_.get(),
                     "let l = document.createElement('link');"
                     "l.rel='icon'; l.type='image/png'; "
                     "l.href='https://example.com/favicon.ico';"
                     "document.head.appendChild(l)"));

  // Verify that the captured data's favicon url from the fenced frame is still
  // empty.
  EXPECT_FALSE(base::JSONReader::Read(
      fenced_frame_receiver->GetContentCaptureFrame().favicon));
}

}  // namespace content_capture
