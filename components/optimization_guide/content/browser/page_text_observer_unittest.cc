// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/page_text_observer.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "components/optimization_guide/content/mojom/page_text_service.mojom.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace optimization_guide {

namespace {

class TestConsumer : public PageTextObserver::Consumer {
 public:
  TestConsumer() = default;
  ~TestConsumer() = default;

  void Reset() { was_called_ = false; }

  void PopulateRequest(uint32_t max_size,
                       const std::set<mojom::TextDumpEvent>& events) {
    request_ = std::make_unique<PageTextObserver::ConsumerTextDumpRequest>();
    request_->max_size = max_size;
    request_->events = events;
    request_->callback = base::BindRepeating(&TestConsumer::OnGotTextDump,
                                             base::Unretained(this));
  }

  void WaitForPageText() {
    if (text_) {
      return;
    }

    base::RunLoop run_loop;
    on_page_text_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  bool was_called() const { return was_called_; }

  const base::Optional<base::string16>& text() const { return text_; }

  // PageTextObserver::Consumer:
  std::unique_ptr<PageTextObserver::ConsumerTextDumpRequest>
  MaybeRequestFrameTextDump(content::NavigationHandle* handle) override {
    was_called_ = true;
    return std::move(request_);
  }

 private:
  void OnGotTextDump(const base::string16& text) {
    text_ = text;
    if (on_page_text_closure_) {
      std::move(on_page_text_closure_).Run();
    }
  }

  bool was_called_ = false;

  std::unique_ptr<PageTextObserver::ConsumerTextDumpRequest> request_;

  base::OnceClosure on_page_text_closure_;

  base::Optional<base::string16> text_;
};

class FakePageTextService : public mojom::PageTextService {
 public:
  FakePageTextService() = default;
  ~FakePageTextService() override = default;

  void BindPendingReceiver(mojo::ScopedInterfaceEndpointHandle handle) {
    receiver_.Bind(mojo::PendingAssociatedReceiver<mojom::PageTextService>(
        std::move(handle)));
  }

  const std::set<mojom::PageTextDumpRequest>& requests() { return requests_; }

  // Sets a sequence of responses to send on the next page dump request for the
  // given |event|. If an element has a value, |OnTextDumpChunk| is called with
  // the text chunk. If an element does not have a value, |OnChunksEnd| is
  // called.
  void SetRemoteResponsesForEvent(
      mojom::TextDumpEvent event,
      const std::vector<base::Optional<base::string16>> responses) {
    responses_.emplace(event, responses);
  }

  // mojom::PageTextService:
  void RequestPageTextDump(
      mojom::PageTextDumpRequestPtr request,
      mojo::PendingRemote<mojom::PageTextConsumer> consumer) override {
    auto responses_iter = responses_.find(request->event);
    requests_.emplace(*request);

    if (responses_iter == responses_.end()) {
      return;
    }

    mojo::Remote<mojom::PageTextConsumer> consumer_remote;
    consumer_remote.Bind(std::move(consumer));

    for (const base::Optional<base::string16>& resp : responses_iter->second) {
      if (resp) {
        consumer_remote->OnTextDumpChunk(*resp);
      } else {
        consumer_remote->OnChunksEnd();
      }
    }
  }

 private:
  mojo::AssociatedReceiver<mojom::PageTextService> receiver_{this};

  std::set<mojom::PageTextDumpRequest> requests_;

  // For each event, a sequence of responses to send on the next page dump
  // request. If an element has a value, |OnTextDumpChunk| is called with the
  // text chunk. If an element does not have a value, |OnChunksEnd| is called.
  std::map<mojom::TextDumpEvent, std::vector<base::Optional<base::string16>>>
      responses_;
};

}  // namespace

class PageTextObserverTest : public content::RenderViewHostTestHarness {
 public:
  PageTextObserverTest() = default;
  ~PageTextObserverTest() override = default;

  // content::RenderViewHostTestHarness:
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    PageTextObserver::CreateForWebContents(web_contents());
    ASSERT_TRUE(observer());
  }

  PageTextObserver* observer() {
    return PageTextObserver::FromWebContents(web_contents());
  }
};

TEST_F(PageTextObserverTest, ConsumerCalled) {
  TestConsumer consumer;
  observer()->AddConsumer(&consumer);

  EXPECT_FALSE(consumer.was_called());
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://test.com"));
  EXPECT_TRUE(consumer.was_called());
}

TEST_F(PageTextObserverTest, ConsumerCalledSameDocument) {
  TestConsumer consumer;
  observer()->AddConsumer(&consumer);

  content::NavigationSimulator::CreateRendererInitiated(
      GURL("http://test.com/"), main_rfh())
      ->Commit();

  EXPECT_TRUE(consumer.was_called());
  consumer.Reset();

  content::NavigationSimulator::CreateRendererInitiated(
      GURL("http://test.com/#fragment"), main_rfh())
      ->CommitSameDocument();

  EXPECT_TRUE(consumer.was_called());
}

TEST_F(PageTextObserverTest, ConsumerNotCalledSubframe) {
  TestConsumer consumer;
  observer()->AddConsumer(&consumer);

  content::NavigationSimulator::CreateRendererInitiated(
      GURL("http://test.com/"), main_rfh())
      ->Commit();

  consumer.Reset();

  content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("http://subframe.com"),
      content::RenderFrameHostTester::For(web_contents()->GetMainFrame())
          ->AppendChild("subframe"));

  EXPECT_FALSE(consumer.was_called());
}

TEST_F(PageTextObserverTest, ConsumerNotCalledNoCommit) {
  TestConsumer consumer;
  observer()->AddConsumer(&consumer);

  auto simulator = content::NavigationSimulator::CreateRendererInitiated(
      GURL("http://test.com/document.pdf"), main_rfh());
  simulator->Start();
  simulator->SetResponseHeaders(base::MakeRefCounted<net::HttpResponseHeaders>(
      "HTTP/1.1 204 No Content\r\n"));
  simulator->Commit();

  EXPECT_FALSE(consumer.was_called());
}

TEST_F(PageTextObserverTest, MojoPlumbingSuccessCase) {
  TestConsumer consumer;
  observer()->AddConsumer(&consumer);

  consumer.PopulateRequest(/*max_size=*/1024,
                           /*events=*/{mojom::TextDumpEvent::kFirstLayout});

  FakePageTextService fake_renderer_service;
  fake_renderer_service.SetRemoteResponsesForEvent(
      mojom::TextDumpEvent::kFirstLayout, {
                                              base::ASCIIToUTF16("a"),
                                              base::ASCIIToUTF16("b"),
                                              base::ASCIIToUTF16("c"),
                                              base::nullopt,
                                          });

  blink::AssociatedInterfaceProvider* remote_interfaces =
      main_rfh()->GetRemoteAssociatedInterfaces();
  remote_interfaces->OverrideBinderForTesting(
      mojom::PageTextService::Name_,
      base::BindRepeating(&FakePageTextService::BindPendingReceiver,
                          base::Unretained(&fake_renderer_service)));

  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://test.com"));

  EXPECT_TRUE(consumer.was_called());

  consumer.WaitForPageText();
  EXPECT_EQ(base::ASCIIToUTF16("abc"), *consumer.text());

  EXPECT_THAT(fake_renderer_service.requests(),
              ::testing::UnorderedElementsAreArray({
                  mojom::PageTextDumpRequest(
                      1024U, mojom::TextDumpEvent::kFirstLayout, 0),
              }));
}

TEST_F(PageTextObserverTest, MaxLengthOnChunkBorder) {
  TestConsumer consumer;
  observer()->AddConsumer(&consumer);

  consumer.PopulateRequest(/*max_size=*/3,
                           /*events=*/{mojom::TextDumpEvent::kFirstLayout});

  FakePageTextService fake_renderer_service;
  fake_renderer_service.SetRemoteResponsesForEvent(
      mojom::TextDumpEvent::kFirstLayout, {
                                              base::ASCIIToUTF16("abc"),
                                              base::ASCIIToUTF16("def"),
                                              base::nullopt,
                                          });

  blink::AssociatedInterfaceProvider* remote_interfaces =
      main_rfh()->GetRemoteAssociatedInterfaces();
  remote_interfaces->OverrideBinderForTesting(
      mojom::PageTextService::Name_,
      base::BindRepeating(&FakePageTextService::BindPendingReceiver,
                          base::Unretained(&fake_renderer_service)));

  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://test.com"));

  EXPECT_TRUE(consumer.was_called());

  consumer.WaitForPageText();
  EXPECT_EQ(base::ASCIIToUTF16("abc"), *consumer.text());

  EXPECT_THAT(
      fake_renderer_service.requests(),
      ::testing::UnorderedElementsAreArray({
          mojom::PageTextDumpRequest(3U, mojom::TextDumpEvent::kFirstLayout, 0),
      }));
}

TEST_F(PageTextObserverTest, MaxLengthWithinChunk) {
  TestConsumer consumer;
  observer()->AddConsumer(&consumer);

  consumer.PopulateRequest(/*max_size=*/4,
                           /*events=*/{mojom::TextDumpEvent::kFirstLayout});

  FakePageTextService fake_renderer_service;
  fake_renderer_service.SetRemoteResponsesForEvent(
      mojom::TextDumpEvent::kFirstLayout, {
                                              base::ASCIIToUTF16("abc"),
                                              base::ASCIIToUTF16("def"),
                                              base::nullopt,
                                          });

  blink::AssociatedInterfaceProvider* remote_interfaces =
      main_rfh()->GetRemoteAssociatedInterfaces();
  remote_interfaces->OverrideBinderForTesting(
      mojom::PageTextService::Name_,
      base::BindRepeating(&FakePageTextService::BindPendingReceiver,
                          base::Unretained(&fake_renderer_service)));

  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://test.com"));

  EXPECT_TRUE(consumer.was_called());

  consumer.WaitForPageText();
  EXPECT_EQ(base::ASCIIToUTF16("abcd"), *consumer.text());

  EXPECT_THAT(
      fake_renderer_service.requests(),
      ::testing::UnorderedElementsAreArray({
          mojom::PageTextDumpRequest(4U, mojom::TextDumpEvent::kFirstLayout, 0),
      }));
}

TEST_F(PageTextObserverTest, MaxLengthWithoutOnEnd) {
  TestConsumer consumer;
  observer()->AddConsumer(&consumer);

  consumer.PopulateRequest(/*max_size=*/4,
                           /*events=*/{mojom::TextDumpEvent::kFirstLayout});

  FakePageTextService fake_renderer_service;
  fake_renderer_service.SetRemoteResponsesForEvent(
      mojom::TextDumpEvent::kFirstLayout, {
                                              base::ASCIIToUTF16("abc"),
                                              base::ASCIIToUTF16("def"),
                                          });

  blink::AssociatedInterfaceProvider* remote_interfaces =
      main_rfh()->GetRemoteAssociatedInterfaces();
  remote_interfaces->OverrideBinderForTesting(
      mojom::PageTextService::Name_,
      base::BindRepeating(&FakePageTextService::BindPendingReceiver,
                          base::Unretained(&fake_renderer_service)));

  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://test.com"));

  EXPECT_TRUE(consumer.was_called());

  consumer.WaitForPageText();
  EXPECT_EQ(base::ASCIIToUTF16("abcd"), *consumer.text());

  EXPECT_THAT(
      fake_renderer_service.requests(),
      ::testing::UnorderedElementsAreArray({
          mojom::PageTextDumpRequest(4U, mojom::TextDumpEvent::kFirstLayout, 0),
      }));
}

TEST_F(PageTextObserverTest, TwoConsumers) {
  TestConsumer consumer1;
  TestConsumer consumer2;
  observer()->AddConsumer(&consumer1);
  observer()->AddConsumer(&consumer2);

  consumer1.PopulateRequest(/*max_size=*/2,
                            /*events=*/{mojom::TextDumpEvent::kFirstLayout});
  consumer2.PopulateRequest(/*max_size=*/3,
                            /*events=*/{mojom::TextDumpEvent::kFirstLayout});

  FakePageTextService fake_renderer_service;
  fake_renderer_service.SetRemoteResponsesForEvent(
      mojom::TextDumpEvent::kFirstLayout, {
                                              base::ASCIIToUTF16("a"),
                                              base::ASCIIToUTF16("b"),
                                              base::ASCIIToUTF16("c"),
                                              base::nullopt,
                                          });

  blink::AssociatedInterfaceProvider* remote_interfaces =
      main_rfh()->GetRemoteAssociatedInterfaces();
  remote_interfaces->OverrideBinderForTesting(
      mojom::PageTextService::Name_,
      base::BindRepeating(&FakePageTextService::BindPendingReceiver,
                          base::Unretained(&fake_renderer_service)));

  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://test.com"));

  EXPECT_TRUE(consumer1.was_called());
  EXPECT_TRUE(consumer2.was_called());

  consumer1.WaitForPageText();
  consumer2.WaitForPageText();
  EXPECT_EQ(base::ASCIIToUTF16("abc"), *consumer1.text());
  EXPECT_EQ(base::ASCIIToUTF16("abc"), *consumer2.text());

  EXPECT_THAT(
      fake_renderer_service.requests(),
      ::testing::UnorderedElementsAreArray({
          mojom::PageTextDumpRequest(3U, mojom::TextDumpEvent::kFirstLayout, 0),
      }));
}

TEST_F(PageTextObserverTest, RemoveConsumer) {
  TestConsumer consumer1;
  TestConsumer consumer2;
  observer()->AddConsumer(&consumer1);
  observer()->AddConsumer(&consumer2);
  observer()->RemoveConsumer(&consumer2);

  consumer1.PopulateRequest(/*max_size=*/3,
                            /*events=*/{mojom::TextDumpEvent::kFirstLayout});
  consumer2.PopulateRequest(/*max_size=*/4,
                            /*events=*/{mojom::TextDumpEvent::kFirstLayout});

  FakePageTextService fake_renderer_service;
  fake_renderer_service.SetRemoteResponsesForEvent(
      mojom::TextDumpEvent::kFirstLayout, {
                                              base::ASCIIToUTF16("a"),
                                              base::ASCIIToUTF16("b"),
                                              base::ASCIIToUTF16("c"),
                                              base::nullopt,
                                          });

  blink::AssociatedInterfaceProvider* remote_interfaces =
      main_rfh()->GetRemoteAssociatedInterfaces();
  remote_interfaces->OverrideBinderForTesting(
      mojom::PageTextService::Name_,
      base::BindRepeating(&FakePageTextService::BindPendingReceiver,
                          base::Unretained(&fake_renderer_service)));

  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://test.com"));

  EXPECT_TRUE(consumer1.was_called());
  EXPECT_FALSE(consumer2.was_called());

  consumer1.WaitForPageText();
  EXPECT_EQ(base::ASCIIToUTF16("abc"), *consumer1.text());

  EXPECT_THAT(
      fake_renderer_service.requests(),
      ::testing::UnorderedElementsAreArray({
          mojom::PageTextDumpRequest(3U, mojom::TextDumpEvent::kFirstLayout, 0),
      }));
}

TEST_F(PageTextObserverTest, TwoEventsRequested) {
  TestConsumer consumer1;
  TestConsumer consumer2;
  observer()->AddConsumer(&consumer1);
  observer()->AddConsumer(&consumer2);

  consumer1.PopulateRequest(/*max_size=*/4,
                            /*events=*/{mojom::TextDumpEvent::kFirstLayout});
  consumer2.PopulateRequest(/*max_size=*/4,
                            /*events=*/{mojom::TextDumpEvent::kFinishedLoad});

  FakePageTextService fake_renderer_service;
  fake_renderer_service.SetRemoteResponsesForEvent(
      mojom::TextDumpEvent::kFirstLayout, {
                                              base::ASCIIToUTF16("abc"),
                                              base::nullopt,
                                          });
  fake_renderer_service.SetRemoteResponsesForEvent(
      mojom::TextDumpEvent::kFinishedLoad, {
                                               base::ASCIIToUTF16("xyz"),
                                               base::nullopt,
                                           });

  blink::AssociatedInterfaceProvider* remote_interfaces =
      main_rfh()->GetRemoteAssociatedInterfaces();
  remote_interfaces->OverrideBinderForTesting(
      mojom::PageTextService::Name_,
      base::BindRepeating(&FakePageTextService::BindPendingReceiver,
                          base::Unretained(&fake_renderer_service)));

  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://test.com"));

  EXPECT_TRUE(consumer1.was_called());
  EXPECT_TRUE(consumer2.was_called());

  consumer1.WaitForPageText();
  consumer2.WaitForPageText();
  EXPECT_EQ(base::ASCIIToUTF16("abc"), *consumer1.text());
  EXPECT_EQ(base::ASCIIToUTF16("xyz"), *consumer2.text());

  EXPECT_THAT(
      fake_renderer_service.requests(),
      ::testing::UnorderedElementsAreArray({
          mojom::PageTextDumpRequest(4U, mojom::TextDumpEvent::kFirstLayout, 0),
          mojom::PageTextDumpRequest(4U, mojom::TextDumpEvent::kFinishedLoad,
                                     0),
      }));
}

}  // namespace optimization_guide
