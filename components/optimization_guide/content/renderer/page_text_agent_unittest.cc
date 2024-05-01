// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/renderer/page_text_agent.h"

#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

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

class PageTextAgentTest : public testing::Test {
 public:
  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(PageTextAgentTest, IncreasesMax) {
  PageTextAgent agent(nullptr);

  mojo::PendingRemote<mojom::PageTextConsumer> consumer_remote;
  TestConsumer consumer;
  consumer.Bind(consumer_remote.InitWithNewPipeAndPassReceiver());

  auto request = mojom::PageTextDumpRequest::New();
  request->max_size = 1024;
  request->event = mojom::TextDumpEvent::kFirstLayout;
  agent.RequestPageTextDump(std::move(request), std::move(consumer_remote));

  uint32_t size = 123;
  auto callback = agent.MaybeRequestTextDumpOnLayoutEvent(
      blink::WebMeaningfulLayout::kFinishedParsing, &size);
  ASSERT_TRUE(callback);
  EXPECT_EQ(1024U, size);

  uint32_t other_size = 1234;
  EXPECT_FALSE(agent.MaybeRequestTextDumpOnLayoutEvent(
      blink::WebMeaningfulLayout::kFinishedLoading, &other_size));
  EXPECT_FALSE(agent.MaybeRequestTextDumpOnLayoutEvent(
      blink::WebMeaningfulLayout::kVisuallyNonEmpty, &other_size));
  EXPECT_EQ(1234U, other_size);

  std::move(callback).Run(
      base::MakeRefCounted<const base::RefCountedString16>(u"abc"));
  RunUntilIdle();

  EXPECT_EQ(u"abc", consumer.text());
  EXPECT_TRUE(consumer.on_chunks_end_called());
}

TEST_F(PageTextAgentTest, MaxStaysSame) {
  PageTextAgent agent(nullptr);

  mojo::PendingRemote<mojom::PageTextConsumer> consumer_remote;
  TestConsumer consumer;
  consumer.Bind(consumer_remote.InitWithNewPipeAndPassReceiver());

  auto request = mojom::PageTextDumpRequest::New();
  request->max_size = 10;
  request->event = mojom::TextDumpEvent::kFirstLayout;
  agent.RequestPageTextDump(std::move(request), std::move(consumer_remote));

  uint32_t size = 123;
  auto callback = agent.MaybeRequestTextDumpOnLayoutEvent(
      blink::WebMeaningfulLayout::kFinishedParsing, &size);
  ASSERT_TRUE(callback);
  EXPECT_EQ(123U, size);

  uint32_t other_size = 1234;
  EXPECT_FALSE(agent.MaybeRequestTextDumpOnLayoutEvent(
      blink::WebMeaningfulLayout::kFinishedLoading, &other_size));
  EXPECT_FALSE(agent.MaybeRequestTextDumpOnLayoutEvent(
      blink::WebMeaningfulLayout::kVisuallyNonEmpty, &other_size));
  EXPECT_EQ(1234U, other_size);

  std::move(callback).Run(
      base::MakeRefCounted<const base::RefCountedString16>(u"abc"));
  RunUntilIdle();

  EXPECT_EQ(u"abc", consumer.text());
  EXPECT_TRUE(consumer.on_chunks_end_called());
}

TEST_F(PageTextAgentTest, FinishedLoading) {
  PageTextAgent agent(nullptr);

  mojo::PendingRemote<mojom::PageTextConsumer> consumer_remote;
  TestConsumer consumer;
  consumer.Bind(consumer_remote.InitWithNewPipeAndPassReceiver());

  auto request = mojom::PageTextDumpRequest::New();
  request->max_size = 1024;
  request->event = mojom::TextDumpEvent::kFinishedLoad;
  agent.RequestPageTextDump(std::move(request), std::move(consumer_remote));

  uint32_t size = 123;
  auto callback = agent.MaybeRequestTextDumpOnLayoutEvent(
      blink::WebMeaningfulLayout::kFinishedLoading, &size);
  EXPECT_TRUE(callback);
  EXPECT_EQ(1024U, size);

  uint32_t other_size = 1234;
  EXPECT_FALSE(agent.MaybeRequestTextDumpOnLayoutEvent(
      blink::WebMeaningfulLayout::kFinishedParsing, &other_size));
  EXPECT_FALSE(agent.MaybeRequestTextDumpOnLayoutEvent(
      blink::WebMeaningfulLayout::kVisuallyNonEmpty, &other_size));
  EXPECT_EQ(1234U, other_size);

  std::move(callback).Run(
      base::MakeRefCounted<const base::RefCountedString16>(u"abc"));
  RunUntilIdle();

  EXPECT_EQ(u"abc", consumer.text());
  EXPECT_TRUE(consumer.on_chunks_end_called());
}

TEST_F(PageTextAgentTest, LongTextOnChunkEdge) {
  PageTextAgent agent(nullptr);

  mojo::PendingRemote<mojom::PageTextConsumer> consumer_remote;
  TestConsumer consumer;
  consumer.Bind(consumer_remote.InitWithNewPipeAndPassReceiver());

  auto request = mojom::PageTextDumpRequest::New();
  request->max_size = 1 << 16;
  request->event = mojom::TextDumpEvent::kFirstLayout;
  agent.RequestPageTextDump(std::move(request), std::move(consumer_remote));

  uint32_t size = 123;
  auto callback = agent.MaybeRequestTextDumpOnLayoutEvent(
      blink::WebMeaningfulLayout::kFinishedParsing, &size);
  EXPECT_TRUE(callback);
  EXPECT_EQ(uint32_t(1 << 16), size);

  uint32_t other_size = 1234;
  EXPECT_FALSE(agent.MaybeRequestTextDumpOnLayoutEvent(
      blink::WebMeaningfulLayout::kFinishedLoading, &other_size));
  EXPECT_FALSE(agent.MaybeRequestTextDumpOnLayoutEvent(
      blink::WebMeaningfulLayout::kVisuallyNonEmpty, &other_size));
  EXPECT_EQ(1234U, other_size);

  std::u16string text(1 << 16, 'a');
  std::move(callback).Run(
      base::MakeRefCounted<const base::RefCountedString16>(text));
  RunUntilIdle();

  EXPECT_EQ(text, consumer.text());
  EXPECT_TRUE(consumer.on_chunks_end_called());
  EXPECT_EQ(uint32_t((1 << 16) / 4096), consumer.num_chunks());
}

TEST_F(PageTextAgentTest, LongTextOffOfChunkEdge) {
  PageTextAgent agent(nullptr);

  mojo::PendingRemote<mojom::PageTextConsumer> consumer_remote;
  TestConsumer consumer;
  consumer.Bind(consumer_remote.InitWithNewPipeAndPassReceiver());

  auto request = mojom::PageTextDumpRequest::New();
  request->max_size = 1 << 16;
  request->event = mojom::TextDumpEvent::kFirstLayout;
  agent.RequestPageTextDump(std::move(request), std::move(consumer_remote));

  uint32_t size = 123;
  auto callback = agent.MaybeRequestTextDumpOnLayoutEvent(
      blink::WebMeaningfulLayout::kFinishedParsing, &size);
  EXPECT_TRUE(callback);
  EXPECT_EQ(uint32_t(1 << 16), size);

  uint32_t other_size = 1234;
  EXPECT_FALSE(agent.MaybeRequestTextDumpOnLayoutEvent(
      blink::WebMeaningfulLayout::kFinishedLoading, &other_size));
  EXPECT_FALSE(agent.MaybeRequestTextDumpOnLayoutEvent(
      blink::WebMeaningfulLayout::kVisuallyNonEmpty, &other_size));
  EXPECT_EQ(1234U, other_size);

  std::u16string text((1 << 15) + 3, 'a');
  std::move(callback).Run(
      base::MakeRefCounted<const base::RefCountedString16>(text));
  RunUntilIdle();

  EXPECT_EQ(text, consumer.text());
  EXPECT_TRUE(consumer.on_chunks_end_called());
  EXPECT_EQ(uint32_t(((1 << 15) + 3) / 4096) + 1, consumer.num_chunks());
}

TEST_F(PageTextAgentTest, NoRequests) {
  PageTextAgent agent(nullptr);

  uint32_t size = 123;
  EXPECT_FALSE(agent.MaybeRequestTextDumpOnLayoutEvent(
      blink::WebMeaningfulLayout::kFinishedParsing, &size));
  EXPECT_FALSE(agent.MaybeRequestTextDumpOnLayoutEvent(
      blink::WebMeaningfulLayout::kFinishedLoading, &size));
  EXPECT_FALSE(agent.MaybeRequestTextDumpOnLayoutEvent(
      blink::WebMeaningfulLayout::kVisuallyNonEmpty, &size));
  EXPECT_EQ(123U, size);
}

TEST_F(PageTextAgentTest, MultipleBindWithSet) {
  PageTextAgent agent(nullptr);

  mojo::PendingAssociatedRemote<mojom::PageTextService> remote_1;
  mojo::PendingAssociatedRemote<mojom::PageTextService> remote_2;

  agent.Bind(remote_1.InitWithNewEndpointAndPassReceiver());
  agent.Bind(remote_2.InitWithNewEndpointAndPassReceiver());
}

}  // namespace optimization_guide
