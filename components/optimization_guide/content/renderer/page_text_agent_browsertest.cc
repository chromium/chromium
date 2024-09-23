// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/renderer/page_text_agent.h"

#include <limits>
#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/test/render_view_test.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace optimization_guide {

namespace {

class TestConsumer : public mojom::PageTextConsumer {
 public:
  TestConsumer() = default;
  ~TestConsumer() override = default;

  std::u16string text() const { return base::StrCat(chunks_); }
  bool on_chunks_end_called() const { return on_chunks_end_called_; }
  size_t num_chunks() const { return chunks_.size(); }

  void Bind(mojo::PendingReceiver<mojom::PageTextConsumer> pending_receiver) {
    receiver_.Bind(std::move(pending_receiver));
  }

  // mojom::PageTextConsumer:
  void OnTextDumpChunk(const std::u16string& chunk) override {
    ASSERT_FALSE(on_chunks_end_called_);
    chunks_.push_back(chunk);
  }

  void OnChunksEnd() override { on_chunks_end_called_ = true; }

 private:
  mojo::Receiver<mojom::PageTextConsumer> receiver_{this};
  std::vector<std::u16string> chunks_;
  bool on_chunks_end_called_ = false;
};

}  // namespace

class PageTextAgentRenderViewTest : public content::RenderViewTest {
 public:
  PageTextAgentRenderViewTest() = default;
  ~PageTextAgentRenderViewTest() override = default;
};

TEST_F(PageTextAgentRenderViewTest, AMPSubframeFirstLayout) {
  // Create and get a subframe.
  LoadHTML(
      "<html><body>"
      "  <p>hello</p>"
      "  <iframe name=sub srcdoc=\"<p>world</p>\"></iframe>"
      "</body></html>");
  content::RenderFrame* subframe = content::RenderFrame::FromWebFrame(
      GetMainFrame()->FindFrameByName("sub")->ToWebLocalFrame());

  PageTextAgent subframe_agent(subframe);

  // Send the request.
  mojo::PendingRemote<mojom::PageTextConsumer> consumer_remote;
  TestConsumer consumer;
  consumer.Bind(consumer_remote.InitWithNewPipeAndPassReceiver());

  auto request = mojom::PageTextDumpRequest::New();
  request->max_size = 1024;
  request->event = mojom::TextDumpEvent::kFirstLayout;
  subframe_agent.RequestPageTextDump(std::move(request),
                                     std::move(consumer_remote));

  // Simulate a page load.
  subframe_agent.DidObserveLoadingBehavior(
      blink::LoadingBehaviorFlag::kLoadingBehaviorAmpDocumentLoaded);
  subframe_agent.DidFinishLoad();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(consumer.on_chunks_end_called());
}

TEST_F(PageTextAgentRenderViewTest, NotAMPSubframeLoadFinished) {
  // Create and get a subframe.
  LoadHTML(
      "<html><body>"
      "  <p>hello</p>"
      "  <iframe name=sub srcdoc=\"<p>world</p>\"></iframe>"
      "</body></html>");
  content::RenderFrame* subframe = content::RenderFrame::FromWebFrame(
      GetMainFrame()->FindFrameByName("sub")->ToWebLocalFrame());

  PageTextAgent subframe_agent(subframe);

  // Send the request.
  mojo::PendingRemote<mojom::PageTextConsumer> consumer_remote;
  TestConsumer consumer;
  consumer.Bind(consumer_remote.InitWithNewPipeAndPassReceiver());

  auto request = mojom::PageTextDumpRequest::New();
  request->max_size = 1024;
  request->event = mojom::TextDumpEvent::kFinishedLoad;
  subframe_agent.RequestPageTextDump(std::move(request),
                                     std::move(consumer_remote));

  // Simulate a page load.
  subframe_agent.DidObserveLoadingBehavior(
      blink::LoadingBehaviorFlag::kLoadingBehaviorNone);
  subframe_agent.DidFinishLoad();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(consumer.on_chunks_end_called());
}

TEST_F(PageTextAgentRenderViewTest, MainFrame) {
  // Create and get a subframe.
  LoadHTML(
      "<html><body>"
      "  <p>hello</p>"
      "</body></html>");
  content::RenderFrame* mainframe =
      content::RenderFrame::FromWebFrame(GetMainFrame());

  PageTextAgent subframe_agent(mainframe);

  // Send the request.
  mojo::PendingRemote<mojom::PageTextConsumer> consumer_remote;
  TestConsumer consumer;
  consumer.Bind(consumer_remote.InitWithNewPipeAndPassReceiver());

  auto request = mojom::PageTextDumpRequest::New();
  request->max_size = 1024;
  request->event = mojom::TextDumpEvent::kFinishedLoad;
  subframe_agent.RequestPageTextDump(std::move(request),
                                     std::move(consumer_remote));

  // Simulate a page load.
  subframe_agent.DidObserveLoadingBehavior(
      blink::LoadingBehaviorFlag::kLoadingBehaviorAmpDocumentLoaded);
  subframe_agent.DidFinishLoad();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(consumer.on_chunks_end_called());
}

TEST_F(PageTextAgentRenderViewTest, AMPSuccessCase) {
  // Create and get a subframe.
  LoadHTML(
      "<html><body>"
      "  <p>hello</p>"
      "  <iframe name=sub srcdoc=\"<p>world</p>\"></iframe>"
      "</body></html>");
  content::RenderFrame* subframe = content::RenderFrame::FromWebFrame(
      GetMainFrame()->FindFrameByName("sub")->ToWebLocalFrame());

  PageTextAgent subframe_agent(subframe);

  // Send the request.
  mojo::PendingRemote<mojom::PageTextConsumer> consumer_remote;
  TestConsumer consumer;
  consumer.Bind(consumer_remote.InitWithNewPipeAndPassReceiver());

  auto request = mojom::PageTextDumpRequest::New();
  request->max_size = 1024;
  request->event = mojom::TextDumpEvent::kFinishedLoad;
  subframe_agent.RequestPageTextDump(std::move(request),
                                     std::move(consumer_remote));

  // Simulate a page load.
  subframe_agent.DidObserveLoadingBehavior(
      blink::LoadingBehaviorFlag::kLoadingBehaviorAmpDocumentLoaded);
  subframe_agent.DidFinishLoad();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(consumer.on_chunks_end_called());
  EXPECT_EQ(u"world", consumer.text());
}

}  // namespace optimization_guide
