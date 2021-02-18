// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/renderer/page_text_agent.h"

#include <limits>

#include "base/bind.h"
#include "base/callback.h"
#include "base/optional.h"
#include "base/strings/strcat.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
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

  base::string16 text() const { return base::StrCat(chunks_); }
  bool on_chunks_end_called() const { return on_chunks_end_called_; }
  size_t num_chunks() const { return chunks_.size(); }

  void Bind(mojo::PendingReceiver<mojom::PageTextConsumer> pending_receiver) {
    receiver_.Bind(std::move(pending_receiver));
  }

  // mojom::PageTextConsumer:
  void OnTextDumpChunk(const base::string16& chunk) override {
    ASSERT_FALSE(on_chunks_end_called_);
    chunks_.push_back(chunk);
  }

  void OnChunksEnd() override { on_chunks_end_called_ = true; }

 private:
  mojo::Receiver<mojom::PageTextConsumer> receiver_{this};
  std::vector<base::string16> chunks_;
  bool on_chunks_end_called_ = false;
};

}  // namespace

class PageTextAgentRenderViewTest : public content::RenderViewTest {
 public:
  PageTextAgentRenderViewTest() = default;
  ~PageTextAgentRenderViewTest() override = default;
};

TEST_F(PageTextAgentRenderViewTest, SubframeFirstLayout) {
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
  request->min_frame_pixel_area = 0;
  subframe_agent.RequestPageTextDump(std::move(request),
                                     std::move(consumer_remote));

  // Simulate a page load.
  subframe_agent.DidFinishLoad();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(consumer.on_chunks_end_called());
}

TEST_F(PageTextAgentRenderViewTest, SubframeLoadFinished) {
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
  request->min_frame_pixel_area = 0;
  subframe_agent.RequestPageTextDump(std::move(request),
                                     std::move(consumer_remote));

  // Simulate a page load.
  subframe_agent.DidFinishLoad();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(consumer.on_chunks_end_called());
  EXPECT_EQ(base::ASCIIToUTF16("world"), consumer.text());
}

TEST_F(PageTextAgentRenderViewTest, SubframeTooSmall) {
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
  request->min_frame_pixel_area = std::numeric_limits<uint64_t>::max();
  subframe_agent.RequestPageTextDump(std::move(request),
                                     std::move(consumer_remote));

  // Simulate a page load.
  subframe_agent.DidFinishLoad();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(consumer.on_chunks_end_called());
}

}  // namespace optimization_guide
