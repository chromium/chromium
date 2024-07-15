// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_capture/browser/content_capture_receiver.h"

#include "base/json/json_reader.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_mock_time_task_runner.h"
#include "build/build_config.h"
#include "components/content_capture/browser/content_capture_test_helper.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/common/content_features.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "third_party/blink/public/mojom/favicon/favicon_url.mojom.h"
#include "ui/gfx/geometry/size.h"

namespace content_capture {
namespace {

static constexpr char16_t kMainFrameUrl[] = u"http://foo.com/main.html";
static constexpr char16_t kMainFrameUrl2[] = u"http://foo.com/2.html";
static constexpr char16_t kChildFrameUrl[] = u"http://foo.org/child.html";
static constexpr char16_t kMainFrameSameDocument[] =
    u"http://foo.com/main.html#1";

}  // namespace

class ContentCaptureReceiverTest : public content::RenderViewHostTestHarness,
                                   public ::testing::WithParamInterface<bool> {
 public:
  void SetUp() override {
    if (GetParam()) {
      scoped_feature_list_.InitWithFeaturesAndParameters(
          content::GetBasicBackForwardCacheFeatureForTesting(),
          content::GetDefaultDisabledBackForwardCacheFeaturesForTesting());
    }
    content::RenderViewHostTestHarness::SetUp();
    helper_.CreateProviderAndConsumer(web_contents(),
                                      &session_removed_test_helper_);

    // This needed to keep the WebContentsObserverConsistencyChecker checks
    // happy for when AppendChild is called.
    NavigateAndCommit(GURL(kMainFrameUrl));
    main_frame_ = web_contents()->GetPrimaryMainFrame();
    EXPECT_TRUE(main_frame_);

    main_frame_sender_ = std::make_unique<FakeContentCaptureSender>();
    main_frame_sender_->Bind(main_frame_);

    helper_.InitTestData(kMainFrameUrl, kChildFrameUrl);
  }

  void NavigateMainFrame(const GURL& url) {
    consumer()->Reset();
    NavigateAndCommit(url);
    main_frame_ = web_contents()->GetPrimaryMainFrame();
  }

  void NavigateMainFrameSameDocument() {
    consumer()->Reset();
    NavigateAndCommit(GURL(kMainFrameSameDocument));
  }

  void SetupChildFrame() {
    child_frame_ = content::RenderFrameHostTester::For(main_frame_.get())
                       ->AppendChild("child");
    EXPECT_TRUE(child_frame_);

    child_frame_sender_ = std::make_unique<FakeContentCaptureSender>();
    child_frame_sender_->Bind(child_frame_);
  }

  void BuildChildSession(const ContentCaptureSession& parent,
                         const ContentCaptureFrame& data,
                         ContentCaptureSession* child) {
    ContentCaptureFrame child_frame = data;
    child_frame.children.clear();
    child->clear();
    child->push_back(child_frame);
    DCHECK(parent.size() == 1);
    child->push_back(parent.front());
  }

  int64_t GetFrameId(bool main_frame) {
    return ContentCaptureReceiver::GetIdFrom(main_frame ? main_frame_.get()
                                                        : child_frame_.get());
  }

  const std::vector<int64_t>& expected_removed_ids() const {
    return expected_removed_ids_;
  }

  SessionRemovedTestHelper* session_removed_test_helper() {
    return &session_removed_test_helper_;
  }

  OnscreenContentProvider* provider() const {
    return helper_.onscreen_content_provider();
  }

  ContentCaptureConsumerHelper* consumer() const {
    return helper_.content_capture_consumer();
  }

  FakeContentCaptureSender* main_frame_sender() const {
    return main_frame_sender_.get();
  }
  FakeContentCaptureSender* child_frame_sender() const {
    return child_frame_sender_.get();
  }

  const ContentCaptureTestHelper* helper() const { return &helper_; }

 private:
  ContentCaptureTestHelper helper_;

  // The sender for main frame.
  std::unique_ptr<FakeContentCaptureSender> main_frame_sender_;
  // The sender for child frame.
  std::unique_ptr<FakeContentCaptureSender> child_frame_sender_;

  raw_ptr<content::RenderFrameHost, DanglingUntriaged> main_frame_ = nullptr;
  raw_ptr<content::RenderFrameHost, DanglingUntriaged> child_frame_ = nullptr;

  // Expected removed Ids.
  std::vector<int64_t> expected_removed_ids_{2};
  SessionRemovedTestHelper session_removed_test_helper_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(,
                         ContentCaptureReceiverTest,
                         testing::Values(true, false));

TEST_P(ContentCaptureReceiverTest, DidCaptureContent) {
  main_frame_sender()->DidCaptureContent(helper()->test_data(),
                                         true /* first_data */);
  EXPECT_TRUE(consumer()->parent_session().empty());
  EXPECT_TRUE(consumer()->removed_sessions().empty());
  EXPECT_EQ(GetExpectedTestData(helper()->test_data(),
                                GetFrameId(true /* main_frame */)),
            consumer()->captured_data());
}

TEST_P(ContentCaptureReceiverTest, MultipleConsumers) {
  std::unique_ptr<ContentCaptureConsumerHelper> consumer2 =
      std::make_unique<ContentCaptureConsumerHelper>(nullptr);

  provider()->AddConsumer(*(consumer2.get()));
  main_frame_sender()->DidCaptureContent(helper()->test_data(),
                                         true /* first_data */);
  EXPECT_TRUE(consumer()->parent_session().empty());
  EXPECT_TRUE(consumer()->removed_sessions().empty());
  EXPECT_EQ(GetExpectedTestData(helper()->test_data(),
                                GetFrameId(true /* main_frame */)),
            consumer()->captured_data());

  EXPECT_TRUE(consumer2->parent_session().empty());
  EXPECT_TRUE(consumer2->removed_sessions().empty());
  EXPECT_EQ(GetExpectedTestData(helper()->test_data(),
                                GetFrameId(true /* main_frame */)),
            consumer2->captured_data());

  // Verifies to get the remove session callback in RemoveConsumer.
  provider()->RemoveConsumer(*(consumer2.get()));
  EXPECT_TRUE(consumer()->removed_sessions().empty());
  EXPECT_EQ(1u, consumer2->removed_sessions().size());
  std::vector<ContentCaptureFrame> expected{GetExpectedTestData(
      helper()->test_data(), GetFrameId(true /* main_frame */))};
  VerifySession(expected, consumer2->removed_sessions().front());
  EXPECT_EQ(1u, provider()->GetConsumersForTesting().size());
  EXPECT_EQ(consumer(), provider()->GetConsumersForTesting()[0]);
}

TEST_P(ContentCaptureReceiverTest, DidCaptureContentWithUpdate) {
  main_frame_sender()->DidCaptureContent(helper()->test_data(),
                                         true /* first_data */);
  // Verifies to get test_data() with correct frame content id.
  EXPECT_TRUE(consumer()->parent_session().empty());
  EXPECT_TRUE(consumer()->removed_sessions().empty());
  EXPECT_EQ(GetExpectedTestData(helper()->test_data(),
                                GetFrameId(true /* main_frame */)),
            consumer()->captured_data());
  // Simulates to update the content within the same document.
  main_frame_sender()->DidCaptureContent(helper()->test_data_update(),
                                         false /* first_data */);
  // Verifies to get test_data2() with correct frame content id.
  EXPECT_TRUE(consumer()->parent_session().empty());
  // Verifies that the session isn't removed.
  EXPECT_TRUE(consumer()->removed_sessions().empty());
  EXPECT_EQ(GetExpectedTestData(helper()->test_data_update(),
                                GetFrameId(true /* main_frame */)),
            consumer()->captured_data());
}

TEST_P(ContentCaptureReceiverTest, DidUpdateContent) {
  main_frame_sender()->DidCaptureContent(helper()->test_data(),
                                         true /* first_data */);
  EXPECT_TRUE(consumer()->parent_session().empty());
  EXPECT_TRUE(consumer()->removed_sessions().empty());
  ContentCaptureFrame expected_data = GetExpectedTestData(
      helper()->test_data(), GetFrameId(true /* main_frame */));
  EXPECT_EQ(expected_data, consumer()->captured_data());

  // Simulate content change.
  main_frame_sender()->DidUpdateContent(helper()->test_data_change());
  EXPECT_TRUE(consumer()->updated_parent_session().empty());
  EXPECT_TRUE(consumer()->removed_sessions().empty());
  EXPECT_EQ(GetExpectedTestData(helper()->test_data_change(), expected_data.id),
            consumer()->updated_data());
}

TEST_P(ContentCaptureReceiverTest, DidRemoveSession) {
  main_frame_sender()->DidCaptureContent(helper()->test_data(),
                                         true /* first_data */);
  // Verifies to get test_data() with correct frame content id.
  EXPECT_TRUE(consumer()->parent_session().empty());
  EXPECT_TRUE(consumer()->removed_sessions().empty());
  EXPECT_EQ(GetExpectedTestData(helper()->test_data(),
                                GetFrameId(true /* main_frame */)),
            consumer()->captured_data());
  // Simulates to navigate other document.
  main_frame_sender()->DidCaptureContent(helper()->test_data2(),
                                         true /* first_data */);
  EXPECT_TRUE(consumer()->parent_session().empty());
  // Verifies that the previous session was removed.
  EXPECT_EQ(1u, consumer()->removed_sessions().size());
  std::vector<ContentCaptureFrame> expected{GetExpectedTestData(
      helper()->test_data(), GetFrameId(true /* main_frame */))};
  VerifySession(expected, consumer()->removed_sessions().front());
  // Verifies that we get the test_data2() from the new document.
  EXPECT_EQ(GetExpectedTestData(helper()->test_data2(),
                                GetFrameId(true /* main_frame */)),
            consumer()->captured_data());
}

TEST_P(ContentCaptureReceiverTest, DidRemoveContent) {
  main_frame_sender()->DidCaptureContent(helper()->test_data(),
                                         true /* first_data */);
  // Verifies to get test_data() with correct frame content id.
  EXPECT_TRUE(consumer()->parent_session().empty());
  EXPECT_TRUE(consumer()->removed_sessions().empty());
  EXPECT_EQ(GetExpectedTestData(helper()->test_data(),
                                GetFrameId(true /* main_frame */)),
            consumer()->captured_data());
  // Simulates to remove the content.
  main_frame_sender()->DidRemoveContent(expected_removed_ids());
  EXPECT_TRUE(consumer()->parent_session().empty());
  EXPECT_TRUE(consumer()->removed_sessions().empty());
  // Verifies that the removed_ids() was removed from the correct session.
  EXPECT_EQ(expected_removed_ids(), consumer()->removed_ids());
  std::vector<ContentCaptureFrame> expected{GetExpectedTestData(
      helper()->test_data(), GetFrameId(true /* main_frame */))};
  VerifySession(expected, consumer()->session());
}

TEST_P(ContentCaptureReceiverTest, ChildFrameDidCaptureContent) {
  // Simulate add child frame.
  SetupChildFrame();
  // Simulate to capture the content from main frame.
  main_frame_sender()->DidCaptureContent(helper()->test_data(),
                                         true /* first_data */);
  // Verifies to get test_data() with correct frame content id.
  EXPECT_TRUE(consumer()->parent_session().empty());
  EXPECT_TRUE(consumer()->removed_sessions().empty());
  EXPECT_EQ(GetExpectedTestData(helper()->test_data(),
                                GetFrameId(true /* main_frame */)),
            consumer()->captured_data());
  // Simulate to capture the content from child frame.
  child_frame_sender()->DidCaptureContent(helper()->test_data2(),
                                          true /* first_data */);
  // Verifies that the parent_session was set correctly.
  EXPECT_FALSE(consumer()->parent_session().empty());
  std::vector<ContentCaptureFrame> expected{GetExpectedTestData(
      helper()->test_data(), GetFrameId(true /* main_frame */))};
  VerifySession(expected, consumer()->parent_session());
  EXPECT_TRUE(consumer()->removed_sessions().empty());
  // Verifies that we receive the correct content from child frame.
  EXPECT_EQ(GetExpectedTestData(helper()->test_data2(),
                                GetFrameId(false /* main_frame */)),
            consumer()->captured_data());
}

// This test is for issue crbug.com/995121 .
TEST_P(ContentCaptureReceiverTest, RenderFrameHostGone) {
  auto* receiver = provider()->ContentCaptureReceiverForFrameForTesting(
      web_contents()->GetPrimaryMainFrame());
  // No good way to simulate crbug.com/995121, just set rfh_ to nullptr in
  // ContentCaptureReceiver, so content::WebContents::FromRenderFrameHost()
  // won't return WebContents.
  receiver->rfh_ = nullptr;
  // Ensure no crash.
  main_frame_sender()->DidCaptureContent(helper()->test_data(),
                                         true /* first_data */);
  main_frame_sender()->DidUpdateContent(helper()->test_data());
  main_frame_sender()->DidRemoveContent(expected_removed_ids());
}

TEST_P(ContentCaptureReceiverTest, TitleUpdateTaskDelay) {
  auto* receiver = provider()->ContentCaptureReceiverForFrameForTesting(
      web_contents()->GetPrimaryMainFrame());
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  // Uses TestMockTimeTaskRunner to check the task state.
  receiver->title_update_task_runner_ = task_runner;

  receiver->SetTitle(u"title 1");
  // No task scheduled because no content has been captured.
  EXPECT_FALSE(receiver->notify_title_update_callback_);
  EXPECT_FALSE(task_runner->HasPendingTask());

  // Capture content, then update the title.
  main_frame_sender()->DidCaptureContent(helper()->test_data(),
                                         /*first_data=*/true);
  std::u16string title2 = u"title 2";
  receiver->SetTitle(title2);
  // A task should be scheduled.
  EXPECT_TRUE(receiver->notify_title_update_callback_);
  EXPECT_TRUE(task_runner->HasPendingTask());
  EXPECT_EQ(2u, receiver->exponential_delay_);
  // Run the pending task.
  task_runner->FastForwardBy(base::Seconds(receiver->exponential_delay_ / 2));
  task_runner->RunUntilIdle();
  // Verify the title is updated and the task is reset.
  EXPECT_EQ(title2, consumer()->updated_title());
  EXPECT_FALSE(receiver->notify_title_update_callback_);
  EXPECT_FALSE(task_runner->HasPendingTask());

  // Set the task_runner again since it is reset after task runs.
  receiver->title_update_task_runner_ = task_runner;

  // Change title again and verify the result.
  receiver->SetTitle(u"title 3");
  EXPECT_TRUE(receiver->notify_title_update_callback_);
  EXPECT_TRUE(task_runner->HasPendingTask());
  EXPECT_EQ(4u, receiver->exponential_delay_);

  // Remove the session to verify the pending task cancelled.
  receiver->RemoveSession();
  EXPECT_FALSE(receiver->notify_title_update_callback_);
  // The cancelled task isn't removed from the TaskRunner, prune it.
  task_runner->TakePendingTasks();
  EXPECT_FALSE(task_runner->HasPendingTask());
  // The delay time is reset after session is removed.
  EXPECT_EQ(1u, receiver->exponential_delay_);
  // Verify the latest task isn't run.
  EXPECT_EQ(title2, consumer()->updated_title());
}

TEST_P(ContentCaptureReceiverTest, ChildFrameCaptureContentFirst) {
  // This test performs navigations, expecting the frames to be destroyed.
  content::DisableBackForwardCacheForTesting(
      web_contents(), content::BackForwardCache::TEST_REQUIRES_NO_CACHING);
  // Simulate add child frame.
  SetupChildFrame();
  // Simulate to capture the content from child frame.
  child_frame_sender()->DidCaptureContent(helper()->test_data2(),
                                          true /* first_data */);
  // Verifies that the parent_session was set correctly.
  EXPECT_FALSE(consumer()->parent_session().empty());

  ContentCaptureFrame data = GetExpectedTestData(
      helper()->test_data(), GetFrameId(true /* main_frame */));
  // Currently, there is no way to fake frame size, set it to 0.
  data.bounds = gfx::Rect();
  ContentCaptureSession expected{data};

  VerifySession(expected, consumer()->parent_session());
  EXPECT_TRUE(consumer()->removed_sessions().empty());
  // Verifies that we receive the correct content from child frame.
  EXPECT_EQ(GetExpectedTestData(helper()->test_data2(),
                                GetFrameId(false /* main_frame */)),
            consumer()->captured_data());

  // Get the child session, so we can verify that it has been removed in next
  // navigation
  ContentCaptureFrame child_frame = GetExpectedTestData(
      helper()->test_data2(), GetFrameId(false /* main_frame */));
  // child_frame.children.clear();
  ContentCaptureSession removed_child_session;
  BuildChildSession(expected, consumer()->captured_data(),
                    &removed_child_session);
  ContentCaptureSession removed_main_session = expected;
  // When main frame navigates to same url, the parent session will not change.
  bool rfh_should_change =
      web_contents()
          ->GetPrimaryMainFrame()
          ->ShouldChangeRenderFrameHostOnSameSiteNavigation();
  NavigateMainFrame(GURL(kMainFrameUrl));
  SetupChildFrame();
  child_frame_sender()->DidCaptureContent(helper()->test_data2(),
                                          true /* first_data */);

  // Intentionally reuse the data.id from previous result, so we know navigating
  // to same domain didn't create new ContentCaptureReceiver when call
  // VerifySession(), otherwise, we can't test the code to handle the navigation
  // in ContentCaptureReceiver - except when RenderDocument is enabled, where we
  // will get new RenderFrameHosts after the navigation to |kMainFrameUrl|.
  if (rfh_should_change) {
    data = GetExpectedTestData(helper()->test_data(),
                               GetFrameId(true /* main_frame */));
  }
  data.url = kMainFrameUrl;
  // Currently, there is no way to fake frame size, set it to 0.
  data.bounds = gfx::Rect();
  expected.clear();
  expected.push_back(data);
  VerifySession(expected, consumer()->parent_session());

  EXPECT_EQ(2u, consumer()->removed_sessions().size());
  VerifySession(removed_child_session, consumer()->removed_sessions().back());
  VerifySession(removed_main_session, consumer()->removed_sessions().front());

  // Get main and child session to verify that they are removed in next
  // navigateion.
  removed_main_session = expected;
  BuildChildSession(expected, consumer()->captured_data(),
                    &removed_child_session);

  // When main frame navigates to same domain, the parent session will change.
  NavigateMainFrame(GURL(kMainFrameUrl2));
  SetupChildFrame();
  child_frame_sender()->DidCaptureContent(helper()->test_data2(),
                                          true /* first_data */);

  // Intentionally reuse the data.id from previous result, so we know navigating
  // to same domain didn't create new ContentCaptureReceiver when call
  // VerifySession(), otherwise, we can't test the code to handle the navigation
  // in ContentCaptureReceiver.  - except when ProactivelySwapBrowsingInstance
  // or RenderDocument is enabled on same-site main frame navigation, where we
  // will get new RenderFrameHosts after the navigation to |kMainFrameUrl2|.
  if (content::CanSameSiteMainFrameNavigationsChangeRenderFrameHosts())
    data = GetExpectedTestData(helper()->test_data(),
                               GetFrameId(true /* main_frame */));

  data.url = kMainFrameUrl2;
  // Currently, there is no way to fake frame size, set it to 0.
  data.bounds = gfx::Rect();
  expected.clear();
  expected.push_back(data);
  VerifySession(expected, consumer()->parent_session());
  // There are two sessions removed, one the main frame because we navigate to
  // different URL (though the domain is same), another one is child frame
  // because of the main frame change.
  EXPECT_EQ(2u, consumer()->removed_sessions().size());

  VerifySession(removed_child_session, consumer()->removed_sessions().back());
  VerifySession(removed_main_session, consumer()->removed_sessions().front());

  // Keep current sessions to verify removed sessions later.
  removed_main_session = expected;
  BuildChildSession(expected, consumer()->captured_data(),
                    &removed_child_session);

  // When main frame navigates to different domain, the parent session will
  // change.
  NavigateMainFrame(GURL(kChildFrameUrl));
  SetupChildFrame();
  child_frame_sender()->DidCaptureContent(helper()->test_data2(),
                                          true /* first_data */);

  data = GetExpectedTestData(helper()->test_data2(),
                             GetFrameId(true /* main_frame */));
  // Currently, there is no way to fake frame size, set it to 0.
  data.bounds = gfx::Rect();
  expected.clear();
  expected.push_back(data);
  VerifySession(expected, consumer()->parent_session());
  EXPECT_EQ(2u, consumer()->removed_sessions().size());
  VerifySession(removed_child_session, consumer()->removed_sessions().back());
  VerifySession(removed_main_session, consumer()->removed_sessions().front());

  // Keep current sessions to verify removed sessions later.
  removed_main_session = expected;
  BuildChildSession(expected, consumer()->captured_data(),
                    &removed_child_session);

  session_removed_test_helper()->Reset();
  DeleteContents();
  EXPECT_EQ(2u, session_removed_test_helper()->removed_sessions().size());
  VerifySession(removed_child_session,
                session_removed_test_helper()->removed_sessions().front());
  VerifySession(removed_main_session,
                session_removed_test_helper()->removed_sessions().back());
}

TEST_P(ContentCaptureReceiverTest, SameDocumentSameSession) {
  main_frame_sender()->DidCaptureContent(helper()->test_data(),
                                         true /* first_data */);
  // Verifies to get test_data() with correct frame content id.
  EXPECT_TRUE(consumer()->parent_session().empty());
  EXPECT_TRUE(consumer()->removed_sessions().empty());
  EXPECT_EQ(GetExpectedTestData(helper()->test_data(),
                                GetFrameId(true /* main_frame */)),
            consumer()->captured_data());
  NavigateMainFrameSameDocument();
  // Verifies the session wasn't removed for the same document navigation.
  EXPECT_TRUE(consumer()->removed_sessions().empty());
}

TEST_P(ContentCaptureReceiverTest, ConvertFaviconURLToJSON) {
  std::vector<blink::mojom::FaviconURLPtr> favicon_urls;
  EXPECT_TRUE(ContentCaptureReceiver::ToJSON(favicon_urls).empty());
  favicon_urls.push_back(blink::mojom::FaviconURL::New(
      GURL{"https://a.com"}, blink::mojom::FaviconIconType::kFavicon,
      std::vector<gfx::Size>{gfx::Size(10, 10)}, /*is_default_icon=*/false));
  favicon_urls.push_back(blink::mojom::FaviconURL::New(
      GURL{"https://b.com"}, blink::mojom::FaviconIconType::kTouchIcon,
      std::vector<gfx::Size>{gfx::Size(100, 100), gfx::Size(20, 20)},
      /*is_default_icon=*/false));
  favicon_urls.push_back(blink::mojom::FaviconURL::New(
      GURL{"https://c.com"},
      blink::mojom::FaviconIconType::kTouchPrecomposedIcon,
      std::vector<gfx::Size>{}, /*is_default_icon=*/false));
  std::string actual_json = ContentCaptureReceiver::ToJSON(favicon_urls);
  std::optional<base::Value> actual = base::JSONReader::Read(actual_json);
  std::string expected_json =
      R"JSON(
      [
        {
          "sizes":[{"height":10,"width":10}],
          "type":"favicon",
          "url":"https://a.com/"
        },
        {
          "sizes":[{"height":100,"width":100},
                     {"height":20,"width":20}],
          "type":"touch icon",
          "url":"https://b.com/"
        },
        {
          "type":"touch precomposed icon",
          "url":"https://c.com/"
        }
      ]
      )JSON";
  std::optional<base::Value> expected = base::JSONReader::Read(expected_json);
  EXPECT_TRUE(actual);
  EXPECT_EQ(expected, actual);
}

class ContentCaptureReceiverMultipleFrameTest
    : public content::RenderViewHostTestHarness {
 public:
  void SetUp() override {
    // Setup multiple frames before creates OnscreenContentProvider.
    content::RenderViewHostTestHarness::SetUp();
    // This needed to keep the WebContentsObserverConsistencyChecker checks
    // happy for when AppendChild is called.
    NavigateAndCommit(GURL("about:blank"));
    content::RenderFrameHostTester::For(web_contents()->GetPrimaryMainFrame())
        ->AppendChild("child");

    helper_.CreateProviderAndConsumer(web_contents());
  }

  OnscreenContentProvider* provider() const {
    return helper_.onscreen_content_provider();
  }

 private:
  ContentCaptureTestHelper helper_;
};

TEST_F(ContentCaptureReceiverMultipleFrameTest,
       ReceiverCreatedForExistingFrame) {
  EXPECT_EQ(2u, provider()->GetFrameMapSizeForTesting());
}

}  // namespace content_capture
