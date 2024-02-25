// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/page_text_observer.h"

#include <optional>
#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/types/optional_util.h"
#include "components/optimization_guide/content/mojom/page_text_service.mojom.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/http/http_response_headers.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/features.h"

namespace optimization_guide {

namespace {

FrameTextDumpResult MakeFrameDump(mojom::TextDumpEvent event,
                                  content::GlobalRenderFrameHostId rfh_id,
                                  bool amp_frame,
                                  int unique_navigation_id,
                                  const std::u16string& contents) {
  return FrameTextDumpResult::Initialize(event, rfh_id, amp_frame,
                                         unique_navigation_id)
      .CompleteWithContents(contents);
}

class TestConsumer : public PageTextObserver::Consumer {
 public:
  TestConsumer() = default;
  ~TestConsumer() = default;

  void Reset() { was_called_ = false; }

  void PopulateRequest(uint32_t max_size,
                       const std::set<mojom::TextDumpEvent>& events,
                       bool request_amp = false) {
    request_ = std::make_unique<PageTextObserver::ConsumerTextDumpRequest>();
    request_->max_size = max_size;
    request_->events = events;
    request_->callback =
        base::BindOnce(&TestConsumer::OnGotTextDump, base::Unretained(this));
    request_->dump_amp_subframes = request_amp;
  }

  void WaitForPageText() {
    if (result_) {
      return;
    }

    base::RunLoop run_loop;
    on_page_text_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  bool was_called() const { return was_called_; }

  std::optional<PageTextDumpResult> result() {
    return base::OptionalFromPtr(result_.get());
  }

  // PageTextObserver::Consumer:
  std::unique_ptr<PageTextObserver::ConsumerTextDumpRequest>
  MaybeRequestFrameTextDump(content::NavigationHandle* handle) override {
    was_called_ = true;
    return std::move(request_);
  }

 private:
  void OnGotTextDump(const PageTextDumpResult& result) {
    result_ = std::make_unique<PageTextDumpResult>(result);
    if (on_page_text_closure_) {
      std::move(on_page_text_closure_).Run();
    }
  }

  bool was_called_ = false;
  std::unique_ptr<PageTextObserver::ConsumerTextDumpRequest> request_;

  base::OnceClosure on_page_text_closure_;
  std::unique_ptr<PageTextDumpResult> result_;
};

class FakePageTextService : public mojom::PageTextService {
 public:
  FakePageTextService() = default;
  ~FakePageTextService() override = default;

  void BindPendingReceiver(mojo::ScopedInterfaceEndpointHandle handle) {
    receiver_.Bind(mojo::PendingAssociatedReceiver<mojom::PageTextService>(
        std::move(handle)));

    if (disconnect_all_) {
      receiver_.reset();
    }
  }

  void set_disconnect_all(bool disconnect_all) {
    disconnect_all_ = disconnect_all;
  }

  const std::set<mojom::PageTextDumpRequest>& requests() { return requests_; }

  // Sets a sequence of responses to send on the next page dump request for the
  // given |event|. If an element has a value, |OnTextDumpChunk| is called with
  // the text chunk. If an element does not have a value, |OnChunksEnd| is
  // called.
  void SetRemoteResponsesForEvent(
      mojom::TextDumpEvent event,
      const std::vector<std::optional<std::u16string>> responses) {
    responses_.emplace(event, responses);
  }

  // A single request that matches the given |event| will hang until the class
  // is destroyed, or another request for the same event is sent.
  void SetEventToHangForver(mojom::TextDumpEvent event) { hang_event_ = event; }

  // mojom::PageTextService:
  void RequestPageTextDump(
      mojom::PageTextDumpRequestPtr request,
      mojo::PendingRemote<mojom::PageTextConsumer> consumer) override {
    auto responses_iter = responses_.find(request->event);
    requests_.emplace(*request);

    if (hang_event_ && hang_event_.value() == request->event) {
      hung_consumer_remote_.Bind(std::move(consumer));
      return;
    }

    if (responses_iter == responses_.end()) {
      return;
    }

    mojo::Remote<mojom::PageTextConsumer> consumer_remote;
    consumer_remote.Bind(std::move(consumer));

    for (const std::optional<std::u16string>& resp : responses_iter->second) {
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

  bool disconnect_all_ = false;

  // Used to timeout a request.
  std::optional<mojom::TextDumpEvent> hang_event_;
  mojo::Remote<mojom::PageTextConsumer> hung_consumer_remote_;

  // For each event, a sequence of responses to send on the next page dump
  // request. If an element has a value, |OnTextDumpChunk| is called with the
  // text chunk. If an element does not have a value, |OnChunksEnd| is called.
  std::map<mojom::TextDumpEvent, std::vector<std::optional<std::u16string>>>
      responses_;
};

class TestPageTextObserver : public PageTextObserver {
 public:
  explicit TestPageTextObserver(content::WebContents* web_contents)
      : PageTextObserver(web_contents) {}
  ~TestPageTextObserver() override = default;

  void SetIsOOPIF(content::RenderFrameHost* rfh, bool is_oopif) {
    oopif_overrides_.erase(rfh);
    oopif_overrides_.emplace(rfh, is_oopif);
  }

  bool IsOOPIF(content::RenderFrameHost* rfh) const override {
    auto iter = oopif_overrides_.find(rfh);
    if (iter != oopif_overrides_.end()) {
      return iter->second;
    }
    return false;
  }

  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override {
    // Intentionally do nothing so that subframes can be added in tests.
  }

  void CallDidFinishLoad() {
    PageTextObserver::DidFinishLoad(web_contents()->GetPrimaryMainFrame(),
                                    GURL());
  }

 private:
  std::map<content::RenderFrameHost*, bool> oopif_overrides_;
};

}  // namespace

class PageTextObserverTest : public content::RenderViewHostTestHarness {
 public:
  PageTextObserverTest() = default;
  ~PageTextObserverTest() override = default;

  // content::RenderViewHostTestHarness:
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    observer_ = std::make_unique<TestPageTextObserver>(web_contents());
    ASSERT_TRUE(observer());
  }

  TestPageTextObserver* observer() const { return observer_.get(); }

 private:
  std::unique_ptr<TestPageTextObserver> observer_;
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
  observer()->CallDidFinishLoad();

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
      content::RenderFrameHostTester::For(web_contents()->GetPrimaryMainFrame())
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
                                              u"a",
                                              u"b",
                                              u"c",
                                              std::nullopt,
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

  observer()->CallDidFinishLoad();
  consumer.WaitForPageText();

  ASSERT_TRUE(consumer.result());
  EXPECT_THAT(
      consumer.result()->frame_results(),
      ::testing::UnorderedElementsAreArray({
          MakeFrameDump(
              mojom::TextDumpEvent::kFirstLayout, main_rfh()->GetGlobalId(),
              /*amp_frame=*/false,
              web_contents()->GetController().GetVisibleEntry()->GetUniqueID(),
              u"abc"),
      }));

  EXPECT_THAT(
      fake_renderer_service.requests(),
      ::testing::UnorderedElementsAreArray({
          mojom::PageTextDumpRequest(1024U, mojom::TextDumpEvent::kFirstLayout),
      }));
}

TEST_F(PageTextObserverTest, CompletedFrameDumpMetrics_Empty) {
  base::HistogramTester histogram_tester;
  TestConsumer consumer;
  observer()->AddConsumer(&consumer);

  consumer.PopulateRequest(/*max_size=*/1024,
                           /*events=*/{mojom::TextDumpEvent::kFirstLayout});

  FakePageTextService fake_renderer_service;
  fake_renderer_service.SetRemoteResponsesForEvent(
      mojom::TextDumpEvent::kFirstLayout, {
                                              std::nullopt,
                                          });

  blink::AssociatedInterfaceProvider* remote_interfaces =
      main_rfh()->GetRemoteAssociatedInterfaces();
  remote_interfaces->OverrideBinderForTesting(
      mojom::PageTextService::Name_,
      base::BindRepeating(&FakePageTextService::BindPendingReceiver,
                          base::Unretained(&fake_renderer_service)));

  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://test.com"));
  ASSERT_TRUE(consumer.was_called());
  observer()->CallDidFinishLoad();
  consumer.WaitForPageText();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageTextDump.FrameDumpLength.FirstLayout", 0, 1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PageTextDump.OutstandingRequests.DidFinishLoad", 1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PageTextDump.TimeUntilFrameDumpCompleted.FirstLayout",
      1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PageTextDump.TimeUntilFrameDisconnected.FirstLayout",
      0);
}

TEST_F(PageTextObserverTest, CompletedFrameDumpMetrics_NotEmpty) {
  base::HistogramTester histogram_tester;
  TestConsumer consumer;
  observer()->AddConsumer(&consumer);

  consumer.PopulateRequest(/*max_size=*/1024,
                           /*events=*/{mojom::TextDumpEvent::kFirstLayout});

  FakePageTextService fake_renderer_service;
  fake_renderer_service.SetRemoteResponsesForEvent(
      mojom::TextDumpEvent::kFirstLayout, {
                                              u"a",
                                              u"b",
                                              u"c",
                                              std::nullopt,
                                          });

  blink::AssociatedInterfaceProvider* remote_interfaces =
      main_rfh()->GetRemoteAssociatedInterfaces();
  remote_interfaces->OverrideBinderForTesting(
      mojom::PageTextService::Name_,
      base::BindRepeating(&FakePageTextService::BindPendingReceiver,
                          base::Unretained(&fake_renderer_service)));

  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://test.com"));
  ASSERT_TRUE(consumer.was_called());
  observer()->CallDidFinishLoad();
  consumer.WaitForPageText();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageTextDump.FrameDumpLength.FirstLayout", 3, 1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PageTextDump.OutstandingRequests.DidFinishLoad", 1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PageTextDump.TimeUntilFrameDumpCompleted.FirstLayout",
      1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PageTextDump.TimeUntilFrameDisconnected.FirstLayout",
      0);
}

TEST_F(PageTextObserverTest, DisconnectedFrameDumpMetrics) {
  base::HistogramTester histogram_tester;
  TestConsumer consumer;
  observer()->AddConsumer(&consumer);

  consumer.PopulateRequest(/*max_size=*/1024,
                           /*events=*/{mojom::TextDumpEvent::kFirstLayout});

  FakePageTextService fake_renderer_service;
  fake_renderer_service.set_disconnect_all(true);

  blink::AssociatedInterfaceProvider* remote_interfaces =
      main_rfh()->GetRemoteAssociatedInterfaces();
  remote_interfaces->OverrideBinderForTesting(
      mojom::PageTextService::Name_,
      base::BindRepeating(&FakePageTextService::BindPendingReceiver,
                          base::Unretained(&fake_renderer_service)));

  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://test.com"));
  ASSERT_TRUE(consumer.was_called());
  base::RunLoop().RunUntilIdle();
  observer()->CallDidFinishLoad();

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PageTextDump.FrameDumpLength.FirstLayout", 0);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PageTextDump.OutstandingRequests.DidFinishLoad", 1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PageTextDump.TimeUntilFrameDumpCompleted.FirstLayout",
      0);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PageTextDump.TimeUntilFrameDisconnected.FirstLayout",
      1);
}

TEST_F(PageTextObserverTest, MaxLengthOnChunkBorder) {
  TestConsumer consumer;
  observer()->AddConsumer(&consumer);

  consumer.PopulateRequest(/*max_size=*/3,
                           /*events=*/{mojom::TextDumpEvent::kFirstLayout});

  FakePageTextService fake_renderer_service;
  fake_renderer_service.SetRemoteResponsesForEvent(
      mojom::TextDumpEvent::kFirstLayout, {
                                              u"abc",
                                              u"def",
                                              std::nullopt,
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

  observer()->CallDidFinishLoad();
  consumer.WaitForPageText();

  ASSERT_TRUE(consumer.result());
  EXPECT_THAT(
      consumer.result()->frame_results(),
      ::testing::UnorderedElementsAreArray({
          MakeFrameDump(
              mojom::TextDumpEvent::kFirstLayout, main_rfh()->GetGlobalId(),
              /*amp_frame=*/false,
              web_contents()->GetController().GetVisibleEntry()->GetUniqueID(),
              u"abc"),
      }));

  EXPECT_THAT(
      fake_renderer_service.requests(),
      ::testing::UnorderedElementsAreArray({
          mojom::PageTextDumpRequest(3U, mojom::TextDumpEvent::kFirstLayout),
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
                                              u"abc",
                                              u"def",
                                              std::nullopt,
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

  observer()->CallDidFinishLoad();
  consumer.WaitForPageText();

  ASSERT_TRUE(consumer.result());
  EXPECT_THAT(
      consumer.result()->frame_results(),
      ::testing::UnorderedElementsAreArray({
          MakeFrameDump(
              mojom::TextDumpEvent::kFirstLayout, main_rfh()->GetGlobalId(),
              /*amp_frame=*/false,
              web_contents()->GetController().GetVisibleEntry()->GetUniqueID(),
              u"abcd"),
      }));

  EXPECT_THAT(
      fake_renderer_service.requests(),
      ::testing::UnorderedElementsAreArray({
          mojom::PageTextDumpRequest(4U, mojom::TextDumpEvent::kFirstLayout),
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
                                              u"abc",
                                              u"def",
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

  observer()->CallDidFinishLoad();
  consumer.WaitForPageText();

  ASSERT_TRUE(consumer.result());
  EXPECT_THAT(
      consumer.result()->frame_results(),
      ::testing::UnorderedElementsAreArray({
          MakeFrameDump(
              mojom::TextDumpEvent::kFirstLayout, main_rfh()->GetGlobalId(),
              /*amp_frame=*/false,
              web_contents()->GetController().GetVisibleEntry()->GetUniqueID(),
              u"abcd"),
      }));

  EXPECT_THAT(
      fake_renderer_service.requests(),
      ::testing::UnorderedElementsAreArray({
          mojom::PageTextDumpRequest(4U, mojom::TextDumpEvent::kFirstLayout),
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
                                              u"a",
                                              u"b",
                                              u"c",
                                              std::nullopt,
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

  observer()->CallDidFinishLoad();
  consumer1.WaitForPageText();
  consumer2.WaitForPageText();

  ASSERT_TRUE(consumer1.result());
  EXPECT_THAT(
      consumer1.result()->frame_results(),
      ::testing::UnorderedElementsAreArray({
          MakeFrameDump(
              mojom::TextDumpEvent::kFirstLayout, main_rfh()->GetGlobalId(),
              /*amp_frame=*/false,
              web_contents()->GetController().GetVisibleEntry()->GetUniqueID(),
              u"abc"),
      }));
  ASSERT_TRUE(consumer2.result());
  EXPECT_THAT(
      consumer2.result()->frame_results(),
      ::testing::UnorderedElementsAreArray({
          MakeFrameDump(
              mojom::TextDumpEvent::kFirstLayout, main_rfh()->GetGlobalId(),
              /*amp_frame=*/false,
              web_contents()->GetController().GetVisibleEntry()->GetUniqueID(),
              u"abc"),
      }));

  EXPECT_THAT(
      fake_renderer_service.requests(),
      ::testing::UnorderedElementsAreArray({
          mojom::PageTextDumpRequest(3U, mojom::TextDumpEvent::kFirstLayout),
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
                                              u"a",
                                              u"b",
                                              u"c",
                                              std::nullopt,
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

  observer()->CallDidFinishLoad();
  consumer1.WaitForPageText();

  ASSERT_TRUE(consumer1.result());
  EXPECT_THAT(
      consumer1.result()->frame_results(),
      ::testing::UnorderedElementsAreArray({
          MakeFrameDump(
              mojom::TextDumpEvent::kFirstLayout, main_rfh()->GetGlobalId(),
              /*amp_frame=*/false,
              web_contents()->GetController().GetVisibleEntry()->GetUniqueID(),
              u"abc"),
      }));
  EXPECT_FALSE(consumer2.result());

  EXPECT_THAT(
      fake_renderer_service.requests(),
      ::testing::UnorderedElementsAreArray({
          mojom::PageTextDumpRequest(3U, mojom::TextDumpEvent::kFirstLayout),
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
                                              u"abc",
                                              std::nullopt,
                                          });
  fake_renderer_service.SetRemoteResponsesForEvent(
      mojom::TextDumpEvent::kFinishedLoad, {
                                               u"xyz",
                                               std::nullopt,
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

  observer()->CallDidFinishLoad();
  consumer1.WaitForPageText();
  consumer2.WaitForPageText();

  ASSERT_TRUE(consumer1.result());
  EXPECT_THAT(
      consumer1.result()->frame_results(),
      ::testing::UnorderedElementsAreArray({
          MakeFrameDump(
              mojom::TextDumpEvent::kFirstLayout, main_rfh()->GetGlobalId(),
              /*amp_frame=*/false,
              web_contents()->GetController().GetVisibleEntry()->GetUniqueID(),
              u"abc"),
          MakeFrameDump(
              mojom::TextDumpEvent::kFinishedLoad, main_rfh()->GetGlobalId(),
              /*amp_frame=*/false,
              web_contents()->GetController().GetVisibleEntry()->GetUniqueID(),
              u"xyz"),
      }));
  EXPECT_EQ(consumer1.result(), consumer2.result());

  EXPECT_THAT(
      fake_renderer_service.requests(),
      ::testing::UnorderedElementsAreArray({
          mojom::PageTextDumpRequest(4U, mojom::TextDumpEvent::kFirstLayout),
          mojom::PageTextDumpRequest(4U, mojom::TextDumpEvent::kFinishedLoad),
      }));
}

TEST_F(PageTextObserverTest, AbandonedRequest) {
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
                                              u"abc",
                                              std::nullopt,
                                          });
  fake_renderer_service.SetEventToHangForver(
      mojom::TextDumpEvent::kFinishedLoad);

  blink::AssociatedInterfaceProvider* remote_interfaces =
      main_rfh()->GetRemoteAssociatedInterfaces();
  remote_interfaces->OverrideBinderForTesting(
      mojom::PageTextService::Name_,
      base::BindRepeating(&FakePageTextService::BindPendingReceiver,
                          base::Unretained(&fake_renderer_service)));

  base::HistogramTester histogram_tester;
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("http://test.com"));
  EXPECT_TRUE(consumer1.was_called());
  EXPECT_TRUE(consumer2.was_called());

  observer()->CallDidFinishLoad();
  consumer1.WaitForPageText();
  consumer2.WaitForPageText();

  ASSERT_TRUE(consumer1.result());
  EXPECT_THAT(
      consumer1.result()->frame_results(),
      ::testing::UnorderedElementsAreArray({
          MakeFrameDump(
              mojom::TextDumpEvent::kFirstLayout, main_rfh()->GetGlobalId(),
              /*amp_frame=*/false,
              web_contents()->GetController().GetVisibleEntry()->GetUniqueID(),
              u"abc"),
      }));
  EXPECT_EQ(consumer1.result(), consumer2.result());

  EXPECT_THAT(
      fake_renderer_service.requests(),
      ::testing::UnorderedElementsAreArray({
          mojom::PageTextDumpRequest(4U, mojom::TextDumpEvent::kFirstLayout),
          mojom::PageTextDumpRequest(4U, mojom::TextDumpEvent::kFinishedLoad),
      }));

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageTextDump.AbandonedRequests", 1, 1);
}

TEST_F(PageTextObserverTest, AMPRequestedOnOOPIF) {
  TestConsumer consumer;
  observer()->AddConsumer(&consumer);

  consumer.PopulateRequest(
      /*max_size=*/1024,
      /*events=*/{mojom::TextDumpEvent::kFirstLayout},
      /*request_amp=*/true);

  FakePageTextService fake_renderer_service;
  fake_renderer_service.SetRemoteResponsesForEvent(
      mojom::TextDumpEvent::kFirstLayout, {
                                              u"abc",
                                              u"def",
                                              std::nullopt,
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

  // Add an OOPIF subframe.
  content::RenderFrameHost* oopif_subframe =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("subframe");
  observer()->SetIsOOPIF(oopif_subframe, true);

  FakePageTextService subframe_fake_renderer_service;
  blink::AssociatedInterfaceProvider* subframe_remote_interfaces =
      oopif_subframe->GetRemoteAssociatedInterfaces();
  subframe_remote_interfaces->OverrideBinderForTesting(
      mojom::PageTextService::Name_,
      base::BindRepeating(&FakePageTextService::BindPendingReceiver,
                          base::Unretained(&subframe_fake_renderer_service)));
  subframe_fake_renderer_service.SetRemoteResponsesForEvent(
      mojom::TextDumpEvent::kFinishedLoad, {
                                               u"amp",
                                               std::nullopt,
                                           });

  observer()->RenderFrameCreated(oopif_subframe);
  observer()->CallDidFinishLoad();
  consumer.WaitForPageText();

  EXPECT_THAT(
      fake_renderer_service.requests(),
      ::testing::UnorderedElementsAreArray({
          mojom::PageTextDumpRequest(1024U, mojom::TextDumpEvent::kFirstLayout),
      }));
  EXPECT_THAT(subframe_fake_renderer_service.requests(),
              ::testing::UnorderedElementsAreArray({
                  mojom::PageTextDumpRequest(
                      1024U, mojom::TextDumpEvent::kFinishedLoad),
              }));

  ASSERT_TRUE(consumer.result());
  EXPECT_THAT(
      consumer.result()->frame_results(),
      ::testing::UnorderedElementsAreArray({
          MakeFrameDump(
              mojom::TextDumpEvent::kFinishedLoad,
              oopif_subframe->GetGlobalId(),
              /*amp_frame=*/true,
              web_contents()->GetController().GetVisibleEntry()->GetUniqueID(),
              u"amp"),
          MakeFrameDump(
              mojom::TextDumpEvent::kFirstLayout, main_rfh()->GetGlobalId(),
              /*amp_frame=*/false,
              web_contents()->GetController().GetVisibleEntry()->GetUniqueID(),
              u"abcdef"),
      }));
}

TEST_F(PageTextObserverTest, AMPNotRequestedOnOOPIF) {
  TestConsumer consumer;
  observer()->AddConsumer(&consumer);

  consumer.PopulateRequest(
      /*max_size=*/1024,
      /*events=*/{mojom::TextDumpEvent::kFirstLayout},
      /*request_amp=*/false);

  FakePageTextService fake_renderer_service;
  fake_renderer_service.SetRemoteResponsesForEvent(
      mojom::TextDumpEvent::kFirstLayout, {
                                              u"abc",
                                              u"def",
                                              std::nullopt,
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

  // Add an OOPIF subframe.
  content::RenderFrameHost* oopif_subframe =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("subframe");
  observer()->SetIsOOPIF(oopif_subframe, true);

  FakePageTextService subframe_fake_renderer_service;
  blink::AssociatedInterfaceProvider* subframe_remote_interfaces =
      oopif_subframe->GetRemoteAssociatedInterfaces();
  subframe_remote_interfaces->OverrideBinderForTesting(
      mojom::PageTextService::Name_,
      base::BindRepeating(&FakePageTextService::BindPendingReceiver,
                          base::Unretained(&subframe_fake_renderer_service)));
  subframe_fake_renderer_service.SetRemoteResponsesForEvent(
      mojom::TextDumpEvent::kFinishedLoad, {
                                               u"\n",
                                               u"amp",
                                               std::nullopt,
                                           });

  observer()->RenderFrameCreated(oopif_subframe);
  observer()->CallDidFinishLoad();
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(
      fake_renderer_service.requests(),
      ::testing::UnorderedElementsAreArray({
          mojom::PageTextDumpRequest(1024U, mojom::TextDumpEvent::kFirstLayout),
      }));
  EXPECT_TRUE(subframe_fake_renderer_service.requests().empty());

  ASSERT_TRUE(consumer.result());
  EXPECT_THAT(
      consumer.result()->frame_results(),
      ::testing::UnorderedElementsAreArray({
          MakeFrameDump(
              mojom::TextDumpEvent::kFirstLayout, main_rfh()->GetGlobalId(),
              /*amp_frame=*/false,
              web_contents()->GetController().GetVisibleEntry()->GetUniqueID(),
              u"abcdef"),
      }));
}

TEST_F(PageTextObserverTest, AMPRequestedOnNonOOPIF) {
  TestConsumer consumer;
  observer()->AddConsumer(&consumer);

  consumer.PopulateRequest(
      /*max_size=*/1024,
      /*events=*/{mojom::TextDumpEvent::kFirstLayout},
      /*request_amp=*/true);

  FakePageTextService fake_renderer_service;
  fake_renderer_service.SetRemoteResponsesForEvent(
      mojom::TextDumpEvent::kFirstLayout, {
                                              u"abc",
                                              u"def",
                                              std::nullopt,
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

  // Add a non-OOPIF subframe.
  content::RenderFrameHost* subframe =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("subframe");
  observer()->SetIsOOPIF(subframe, false);

  FakePageTextService subframe_fake_renderer_service;
  blink::AssociatedInterfaceProvider* subframe_remote_interfaces =
      subframe->GetRemoteAssociatedInterfaces();
  subframe_remote_interfaces->OverrideBinderForTesting(
      mojom::PageTextService::Name_,
      base::BindRepeating(&FakePageTextService::BindPendingReceiver,
                          base::Unretained(&subframe_fake_renderer_service)));
  subframe_fake_renderer_service.SetRemoteResponsesForEvent(
      mojom::TextDumpEvent::kFinishedLoad, {
                                               u"\n",
                                               u"amp",
                                               std::nullopt,
                                           });

  observer()->RenderFrameCreated(subframe);
  observer()->CallDidFinishLoad();
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(
      fake_renderer_service.requests(),
      ::testing::UnorderedElementsAreArray({
          mojom::PageTextDumpRequest(1024U, mojom::TextDumpEvent::kFirstLayout),
      }));
  EXPECT_TRUE(subframe_fake_renderer_service.requests().empty());

  ASSERT_TRUE(consumer.result());
  EXPECT_THAT(
      consumer.result()->frame_results(),
      ::testing::UnorderedElementsAreArray({
          MakeFrameDump(
              mojom::TextDumpEvent::kFirstLayout, main_rfh()->GetGlobalId(),
              /*amp_frame=*/false,
              web_contents()->GetController().GetVisibleEntry()->GetUniqueID(),
              u"abcdef"),
      }));
}

class PageTextObserverWithPrerenderTest : public PageTextObserverTest {
 public:
  PageTextObserverWithPrerenderTest() = default;

  content::RenderFrameHost* AddPrerender(const GURL& prerender_url) {
    content::RenderFrameHost* prerender_frame =
        content::WebContentsTester::For(web_contents())
            ->AddPrerenderAndCommitNavigation(prerender_url);
    DCHECK(prerender_frame);
    EXPECT_EQ(prerender_frame->GetLifecycleState(),
              content::RenderFrameHost::LifecycleState::kPrerendering);
    EXPECT_EQ(prerender_frame->GetLastCommittedURL(), prerender_url);
    return prerender_frame;
  }

 private:
  content::test::ScopedPrerenderFeatureList prerender_feature_list_;
};

TEST_F(PageTextObserverWithPrerenderTest,
       PrerenderingShouldNotResetOutstandingRequest) {
  content::test::ScopedPrerenderWebContentsDelegate web_contents_delegate(
      *web_contents());

  TestConsumer consumer;
  observer()->AddConsumer(&consumer);
  EXPECT_FALSE(consumer.was_called());

  consumer.PopulateRequest(
      /*max_size=*/1024,
      /*events=*/{mojom::TextDumpEvent::kFirstLayout},
      /*request_amp=*/true);
  EXPECT_EQ(observer()->outstanding_requests(), 0U);

  NavigateAndCommit(GURL("http://www.test.com"));
  EXPECT_EQ(observer()->outstanding_requests(), 1U);
  consumer.Reset();

  // Add a prerender page.
  const GURL prerender_url = GURL("http://www.test.com");
  content::RenderFrameHost* prerender_frame = AddPrerender(prerender_url);
  EXPECT_FALSE(consumer.was_called());
  // |outstanding_requests_| should not be reset to 0 by prerendering.
  EXPECT_EQ(observer()->outstanding_requests(), 1U);
  consumer.Reset();

  // Activate the prerendered page.
  content::NavigationSimulator::NavigateAndCommitFromDocument(
      prerender_url, web_contents()->GetPrimaryMainFrame());
  EXPECT_EQ(prerender_frame->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kActive);
  EXPECT_TRUE(consumer.was_called());
  // |outstanding_requests_| should be reset to 0 after activating.
  EXPECT_EQ(observer()->outstanding_requests(), 0U);
}

TEST_F(PageTextObserverWithPrerenderTest, AMPRequestedOnOOPIFInPrerendering) {
  content::test::ScopedPrerenderWebContentsDelegate web_contents_delegate(
      *web_contents());
  TestConsumer consumer;
  observer()->AddConsumer(&consumer);

  consumer.PopulateRequest(
      /*max_size=*/1024,
      /*events=*/{mojom::TextDumpEvent::kFirstLayout},
      /*request_amp=*/true);

  NavigateAndCommit(GURL("http://www.test.com"));

  consumer.Reset();

  // Add a prerender page.
  const GURL prerender_url = GURL("http://www.test.com/?prerender");
  content::RenderFrameHost* prerender_frame = AddPrerender(prerender_url);

  FakePageTextService fake_renderer_service;
  fake_renderer_service.SetRemoteResponsesForEvent(
      mojom::TextDumpEvent::kFirstLayout, {
                                              u"abc",
                                              u"def",
                                              std::nullopt,
                                          });
  blink::AssociatedInterfaceProvider* remote_interfaces =
      prerender_frame->GetRemoteAssociatedInterfaces();
  remote_interfaces->OverrideBinderForTesting(
      mojom::PageTextService::Name_,
      base::BindRepeating(&FakePageTextService::BindPendingReceiver,
                          base::Unretained(&fake_renderer_service)));
  EXPECT_FALSE(consumer.was_called());

  // Add an OOPIF subframe.
  content::RenderFrameHost* oopif_subframe =
      content::RenderFrameHostTester::For(prerender_frame)
          ->AppendChild("subframe");
  observer()->SetIsOOPIF(oopif_subframe, true);

  FakePageTextService subframe_fake_renderer_service;
  blink::AssociatedInterfaceProvider* subframe_remote_interfaces =
      oopif_subframe->GetRemoteAssociatedInterfaces();
  subframe_remote_interfaces->OverrideBinderForTesting(
      mojom::PageTextService::Name_,
      base::BindRepeating(&FakePageTextService::BindPendingReceiver,
                          base::Unretained(&subframe_fake_renderer_service)));
  subframe_fake_renderer_service.SetRemoteResponsesForEvent(
      mojom::TextDumpEvent::kFinishedLoad, {
                                               u"amp",
                                               std::nullopt,
                                           });

  observer()->RenderFrameCreated(oopif_subframe);
  observer()->CallDidFinishLoad();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(consumer.was_called());
  EXPECT_FALSE(consumer.result());
}

class PageTextObserverFencedFramesTest : public PageTextObserverTest {
 public:
  PageTextObserverFencedFramesTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        blink::features::kFencedFrames, {{"implementation_type", "mparch"}});
  }
  ~PageTextObserverFencedFramesTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(PageTextObserverFencedFramesTest, AMPRequestedOnOOPIFInFencedFrame) {
  TestConsumer consumer;
  observer()->AddConsumer(&consumer);

  consumer.PopulateRequest(
      /*max_size=*/1024,
      /*events=*/{mojom::TextDumpEvent::kFirstLayout},
      /*request_amp=*/true);

  FakePageTextService fake_renderer_service;
  fake_renderer_service.SetRemoteResponsesForEvent(
      mojom::TextDumpEvent::kFirstLayout, {
                                              u"abc",
                                              u"def",
                                              std::nullopt,
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

  content::RenderFrameHost* fenced_frame_rfh =
      content::RenderFrameHostTester::For(main_rfh())->AppendFencedFrame();
  GURL kFencedFrameUrl("http://fencedframe.com");
  std::unique_ptr<content::NavigationSimulator> navigation_simulator =
      content::NavigationSimulator::CreateRendererInitiated(kFencedFrameUrl,
                                                            fenced_frame_rfh);
  navigation_simulator->Commit();
  fenced_frame_rfh = navigation_simulator->GetFinalRenderFrameHost();

  // Add an OOPIF subframe.
  content::RenderFrameHost* oopif_subframe =
      content::RenderFrameHostTester::For(fenced_frame_rfh)
          ->AppendChild("subframe");
  observer()->SetIsOOPIF(oopif_subframe, true);

  FakePageTextService subframe_fake_renderer_service;
  blink::AssociatedInterfaceProvider* subframe_remote_interfaces =
      oopif_subframe->GetRemoteAssociatedInterfaces();
  subframe_remote_interfaces->OverrideBinderForTesting(
      mojom::PageTextService::Name_,
      base::BindRepeating(&FakePageTextService::BindPendingReceiver,
                          base::Unretained(&subframe_fake_renderer_service)));
  subframe_fake_renderer_service.SetRemoteResponsesForEvent(
      mojom::TextDumpEvent::kFinishedLoad, {
                                               u"amp",
                                               std::nullopt,
                                           });

  observer()->RenderFrameCreated(oopif_subframe);
  observer()->CallDidFinishLoad();
  consumer.WaitForPageText();

  EXPECT_THAT(
      fake_renderer_service.requests(),
      ::testing::UnorderedElementsAreArray({
          mojom::PageTextDumpRequest(1024U, mojom::TextDumpEvent::kFirstLayout),
      }));
  EXPECT_TRUE(subframe_fake_renderer_service.requests().empty());

  ASSERT_TRUE(consumer.result());
  EXPECT_THAT(
      consumer.result()->frame_results(),
      ::testing::UnorderedElementsAreArray({
          MakeFrameDump(
              mojom::TextDumpEvent::kFirstLayout, main_rfh()->GetGlobalId(),
              /*amp_frame=*/false,
              web_contents()->GetController().GetVisibleEntry()->GetUniqueID(),
              u"abcdef"),
      }));
}

}  // namespace optimization_guide
