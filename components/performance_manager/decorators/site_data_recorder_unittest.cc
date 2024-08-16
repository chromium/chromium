// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/decorators/site_data_recorder.h"

#include <memory>

#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/threading/sequence_bound.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/persistence/site_data/site_data_cache.h"
#include "components/performance_manager/persistence/site_data/site_data_cache_factory.h"
#include "components/performance_manager/persistence/site_data/site_data_impl.h"
#include "components/performance_manager/persistence/site_data/site_data_writer.h"
#include "components/performance_manager/persistence/site_data/tab_visibility.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/test_support/performance_manager_test_harness.h"
#include "components/performance_manager/test_support/persistence/test_site_data_reader.h"
#include "components/performance_manager/test_support/persistence/unittest_utils.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace performance_manager {

constexpr base::TimeDelta kTitleOrFaviconChangePostLoadGracePeriod =
    base::Seconds(20);
constexpr base::TimeDelta kFeatureUsagePostBackgroundGracePeriod =
    base::Seconds(10);

// A mock implementation of a SiteDataWriter.
class LenientMockDataWriter : public SiteDataWriter {
 public:
  LenientMockDataWriter(const url::Origin& origin,
                        scoped_refptr<internal::SiteDataImpl> impl)
      : SiteDataWriter(impl), origin_(origin) {}
  ~LenientMockDataWriter() override {
    if (on_destroy_indicator_)
      *on_destroy_indicator_ = true;
  }
  LenientMockDataWriter(const LenientMockDataWriter& other) = delete;
  LenientMockDataWriter& operator=(const LenientMockDataWriter&) = delete;

  MOCK_METHOD(void, NotifySiteLoaded, (TabVisibility), (override));
  MOCK_METHOD(void, NotifySiteUnloaded, (TabVisibility), (override));
  MOCK_METHOD(void, NotifySiteForegrounded, (bool), (override));
  MOCK_METHOD(void, NotifySiteBackgrounded, (bool), (override));
  MOCK_METHOD(void, NotifyUpdatesFaviconInBackground, (), (override));
  MOCK_METHOD(void, NotifyUpdatesTitleInBackground, (), (override));
  MOCK_METHOD(void, NotifyUsesAudioInBackground, (), (override));
  MOCK_METHOD(void,
              NotifyLoadTimePerformanceMeasurement,
              (base::TimeDelta, base::TimeDelta, uint64_t),
              (override));

  // Used to record the destruction of this object.
  void SetOnDestroyIndicator(bool* on_destroy_indicator) {
    if (on_destroy_indicator)
      EXPECT_FALSE(*on_destroy_indicator);
    on_destroy_indicator_ = on_destroy_indicator;
  }

  const url::Origin& Origin() const override { return origin_; }

 private:
  raw_ptr<bool> on_destroy_indicator_ = nullptr;
  url::Origin origin_;
};
using MockDataWriter = ::testing::StrictMock<LenientMockDataWriter>;

// A data cache that serves MockDataWriter objects.
class MockDataCache : public SiteDataCache {
 public:
  MockDataCache() = default;
  ~MockDataCache() override = default;
  MockDataCache(const MockDataCache& other) = delete;
  MockDataCache& operator=(const MockDataCache&) = delete;

  // SiteDataCache:
  std::unique_ptr<SiteDataReader> GetReaderForOrigin(
      const url::Origin& origin) override {
    return std::make_unique<testing::SimpleTestSiteDataReader>();
  }
  std::unique_ptr<SiteDataWriter> GetWriterForOrigin(
      const url::Origin& origin) override {
    scoped_refptr<internal::SiteDataImpl> fake_impl =
        base::WrapRefCounted(new internal::SiteDataImpl(
            origin, delegate_.GetWeakPtr(), &data_store_));

    return std::make_unique<MockDataWriter>(origin, fake_impl);
  }
  bool IsRecording() const override { return true; }
  int Size() const override { return 0; }

 private:
  testing::NoopSiteDataStore data_store_;

  // The mock delegate used by the SiteDataImpl objects
  // created by this class, NiceMock is used to avoid having to set
  // expectations in test cases that don't care about this.
  ::testing::NiceMock<testing::MockSiteDataImplOnDestroyDelegate> delegate_;
};

void NavigatePageNodeOnUIThread(content::WebContents* contents,
                                const GURL& url) {
  EXPECT_TRUE(contents);
  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(contents);
  EXPECT_TRUE(web_contents_tester);
  web_contents_tester->NavigateAndCommit(url);
}

void RunTaskOnPMSequence(base::OnceClosure task) {
  base::RunLoop run_loop;
  PerformanceManager::CallOnGraph(FROM_HERE, std::move(task));
  PerformanceManager::CallOnGraph(FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

MockDataWriter* GetMockWriterForPageNode(const PageNode* page_node) {
  return static_cast<MockDataWriter*>(
      SiteDataRecorder::Data::GetForTesting(page_node).writer());
}

class SiteDataRecorderTest : public PerformanceManagerTestHarness {
 public:
  SiteDataRecorderTest()
      : PerformanceManagerTestHarness(
            content::BrowserTaskEnvironment::TimeSource::MOCK_TIME) {}
  ~SiteDataRecorderTest() override = default;
  explicit SiteDataRecorderTest(const SiteDataRecorderTest& other) = delete;
  SiteDataRecorderTest& operator=(const SiteDataRecorderTest&) = delete;

  void SetUp() override {
    PerformanceManagerTestHarness::SetUp();
    cache_factory_ = base::SequenceBound<SiteDataCacheFactory>(
        PerformanceManager::GetTaskRunner());
    auto recorder = std::make_unique<SiteDataRecorder>();
    recorder_ = recorder.get();
    PerformanceManager::PassToGraph(FROM_HERE, std::move(recorder));

    auto browser_context_id = GetBrowserContext()->UniqueId();
    RunTaskOnPMSequence(base::BindLambdaForTesting([&]() {
      auto* factory = SiteDataCacheFactory::GetInstance();
      ASSERT_TRUE(factory);
      factory->SetCacheForTesting(browser_context_id,
                                  std::make_unique<MockDataCache>());
    }));

    SetContents(CreateTestWebContents());
    base::WeakPtr<PageNode> page_node =
        PerformanceManager::GetPrimaryPageNodeForWebContents(web_contents());
    RunTaskOnPMSequence(base::BindLambdaForTesting([&]() {
      auto* page_node_impl = PageNodeImpl::FromNode(page_node.get());
      page_node_impl->SetIsAudible(false);
      page_node_impl->SetIsVisible(false);
      page_node_impl->SetLoadingState(PageNode::LoadingState::kLoading);
    }));
  }

  void TearDown() override {
    DeleteContents();
    recorder_ = nullptr;
    cache_factory_.SynchronouslyResetForTest();
    PerformanceManagerTestHarness::TearDown();
  }

  const GURL kTestUrl1 = GURL("http://foo.com");
  const GURL kTestUrl2 = GURL("http://bar.com");

 private:
  raw_ptr<SiteDataRecorder> recorder_ = nullptr;
  base::SequenceBound<SiteDataCacheFactory> cache_factory_;
};

TEST_F(SiteDataRecorderTest, NavigationEventsBasicTests) {
  base::WeakPtr<PageNode> page_node =
      PerformanceManager::GetPrimaryPageNodeForWebContents(web_contents());

  RunTaskOnPMSequence(base::BindLambdaForTesting([&]() {
    EXPECT_TRUE(page_node);
    EXPECT_FALSE(
        SiteDataRecorder::Data::GetForTesting(page_node.get()).writer());
  }));

  // Send a navigation event with the |committed| bit set and make sure that a
  // writer has been created for this origin.
  NavigatePageNodeOnUIThread(web_contents(), kTestUrl1);

  MockDataWriter* mock_writer = nullptr;
  RunTaskOnPMSequence(base::BindLambdaForTesting([&]() {
    mock_writer = GetMockWriterForPageNode(page_node.get());
    ASSERT_TRUE(mock_writer);
    EXPECT_EQ(url::Origin::Create(kTestUrl1), mock_writer->Origin());
  }));

  {
    // A navigation to the same origin shouldn't cause caused this writer to get
    // destroyed.
    bool writer_has_been_destroyed = false;

    RunTaskOnPMSequence(base::BindLambdaForTesting([&]() {
      mock_writer->SetOnDestroyIndicator(&writer_has_been_destroyed);
    }));

    NavigatePageNodeOnUIThread(web_contents(), kTestUrl1);
    RunTaskOnPMSequence(base::BindLambdaForTesting(
        [&]() { EXPECT_FALSE(writer_has_been_destroyed); }));

    // Navigate to a different origin and make sure that this causes the
    // destruction of the writer.
    NavigatePageNodeOnUIThread(web_contents(), kTestUrl2);

    RunTaskOnPMSequence(base::BindLambdaForTesting([&]() {
      EXPECT_TRUE(writer_has_been_destroyed);
      mock_writer = GetMockWriterForPageNode(page_node.get());
      EXPECT_EQ(url::Origin::Create(kTestUrl2), mock_writer->Origin());
      mock_writer->SetOnDestroyIndicator(nullptr);
    }));
  }
}

// Test that the feature usage events get forwarded to the writer when the tab
// is in background.
TEST_F(SiteDataRecorderTest, FeatureEventsGetForwardedWhenInBackground) {
  base::WeakPtr<PageNode> page_node =
      PerformanceManager::GetPrimaryPageNodeForWebContents(web_contents());

  NavigatePageNodeOnUIThread(web_contents(), kTestUrl1);

  MockDataWriter* mock_writer = nullptr;
  PageNodeImpl* node_impl = nullptr;
  RunTaskOnPMSequence(base::BindLambdaForTesting([&]() {
    mock_writer = GetMockWriterForPageNode(page_node.get());
    ASSERT_TRUE(mock_writer);
    EXPECT_EQ(url::Origin::Create(kTestUrl1), mock_writer->Origin());

    node_impl = PageNodeImpl::FromNode(page_node.get());
    EXPECT_CALL(*mock_writer, NotifySiteLoaded(TabVisibility::kBackground));
    node_impl->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
    ::testing::Mock::VerifyAndClear(mock_writer);

    EXPECT_CALL(*mock_writer, NotifySiteForegrounded(true));
  }));

  web_contents()->WasShown();

  RunTaskOnPMSequence(base::BindLambdaForTesting([&]() {
    ::testing::Mock::VerifyAndClear(mock_writer);

    // Ensure that no event gets forwarded if the tab is not in background.
    node_impl->OnFaviconUpdated();
    ::testing::Mock::VerifyAndClear(mock_writer);
    node_impl->OnTitleUpdated();
    ::testing::Mock::VerifyAndClear(mock_writer);
    node_impl->SetIsAudible(true);
    ::testing::Mock::VerifyAndClear(mock_writer);

    EXPECT_CALL(*mock_writer, NotifySiteBackgrounded(true));
  }));
  web_contents()->WasHidden();

  RunTaskOnPMSequence(base::BindLambdaForTesting([&]() {
    ::testing::Mock::VerifyAndClear(mock_writer);

    // Title and Favicon should be ignored during the post-loading grace period.
    node_impl->OnFaviconUpdated();
    node_impl->OnTitleUpdated();
    ::testing::Mock::VerifyAndClear(mock_writer);
  }));

  task_environment()->FastForwardBy(kTitleOrFaviconChangePostLoadGracePeriod);

  RunTaskOnPMSequence(base::BindLambdaForTesting([&]() {
    EXPECT_CALL(*mock_writer, NotifyUpdatesFaviconInBackground());
    node_impl->OnFaviconUpdated();
    ::testing::Mock::VerifyAndClear(mock_writer);
    EXPECT_CALL(*mock_writer, NotifyUpdatesTitleInBackground());
    node_impl->OnTitleUpdated();
    ::testing::Mock::VerifyAndClear(mock_writer);

    // Brievly switch the tab to foreground to reset the last backgrounded time.
    EXPECT_CALL(*mock_writer, NotifySiteForegrounded(true));
    EXPECT_CALL(*mock_writer, NotifySiteBackgrounded(true));
  }));
  web_contents()->WasShown();
  web_contents()->WasHidden();

  RunTaskOnPMSequence(base::BindLambdaForTesting([&]() {
    ::testing::Mock::VerifyAndClear(mock_writer);

    // These events should be ignored during the post-background grace period.
    node_impl->SetIsAudible(true);
    node_impl->SetIsAudible(false);
    node_impl->OnFaviconUpdated();
    node_impl->OnTitleUpdated();
    ::testing::Mock::VerifyAndClear(mock_writer);
  }));

  task_environment()->FastForwardBy(kFeatureUsagePostBackgroundGracePeriod);

  RunTaskOnPMSequence(base::BindLambdaForTesting([&]() {
    EXPECT_CALL(*mock_writer, NotifyUsesAudioInBackground());
    EXPECT_CALL(*mock_writer, NotifyUpdatesFaviconInBackground());
    EXPECT_CALL(*mock_writer, NotifyUpdatesTitleInBackground());
    node_impl->SetIsAudible(true);
    node_impl->OnFaviconUpdated();
    node_impl->OnTitleUpdated();
    ::testing::Mock::VerifyAndClear(mock_writer);

    EXPECT_CALL(*mock_writer, NotifySiteUnloaded(TabVisibility::kBackground));
  }));

  NavigatePageNodeOnUIThread(web_contents(), GURL("about:blank"));
}

TEST_F(SiteDataRecorderTest, FeatureEventsIgnoredWhenLoadingInBackground) {
  base::WeakPtr<PageNode> page_node =
      PerformanceManager::GetPrimaryPageNodeForWebContents(web_contents());
  NavigatePageNodeOnUIThread(web_contents(), kTestUrl1);

  RunTaskOnPMSequence(base::BindLambdaForTesting([&]() {
    MockDataWriter* mock_writer = GetMockWriterForPageNode(page_node.get());
    ASSERT_TRUE(mock_writer);
    EXPECT_EQ(url::Origin::Create(kTestUrl1), mock_writer->Origin());

    PageNodeImpl* node_impl = PageNodeImpl::FromNode(page_node.get());
    ::testing::Mock::VerifyAndClear(mock_writer);
    node_impl->OnFaviconUpdated();
    ::testing::Mock::VerifyAndClear(mock_writer);
    node_impl->OnTitleUpdated();
    ::testing::Mock::VerifyAndClear(mock_writer);
    node_impl->SetIsAudible(true);
    ::testing::Mock::VerifyAndClear(mock_writer);
  }));
}

TEST_F(SiteDataRecorderTest, VisibilityEvent) {
  base::WeakPtr<PageNode> page_node =
      PerformanceManager::GetPrimaryPageNodeForWebContents(web_contents());
  NavigatePageNodeOnUIThread(web_contents(), kTestUrl1);

  RunTaskOnPMSequence(base::BindLambdaForTesting([&]() {
    MockDataWriter* mock_writer = GetMockWriterForPageNode(page_node.get());
    PageNodeImpl* node_impl = PageNodeImpl::FromNode(page_node.get());

    // Test that the visibility events get forwarded to the writer.

    EXPECT_CALL(*mock_writer, NotifySiteForegrounded(false));
    node_impl->SetIsVisible(true);
    ::testing::Mock::VerifyAndClear(mock_writer);

    EXPECT_CALL(*mock_writer, NotifySiteBackgrounded(false));
    node_impl->SetIsVisible(false);
    ::testing::Mock::VerifyAndClear(mock_writer);
  }));
}

TEST_F(SiteDataRecorderTest, LoadEvent) {
  base::WeakPtr<PageNode> page_node =
      PerformanceManager::GetPrimaryPageNodeForWebContents(web_contents());
  NavigatePageNodeOnUIThread(web_contents(), kTestUrl1);

  RunTaskOnPMSequence(base::BindLambdaForTesting([&]() {
    MockDataWriter* mock_writer = GetMockWriterForPageNode(page_node.get());
    PageNodeImpl* node_impl = PageNodeImpl::FromNode(page_node.get());

    // Test that the load/unload events get forwarded to the writer.

    EXPECT_CALL(*mock_writer, NotifySiteLoaded(TabVisibility::kBackground));
    node_impl->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
    ::testing::Mock::VerifyAndClear(mock_writer);

    EXPECT_CALL(*mock_writer, NotifySiteUnloaded(TabVisibility::kBackground));
    node_impl->SetLoadingState(PageNode::LoadingState::kLoading);
    ::testing::Mock::VerifyAndClear(mock_writer);
  }));
}

TEST_F(SiteDataRecorderTest, NodeDataAccessors) {
  // SiteDataRecorder::Data objects should exist for all page nodes.
  // Reader and writer objects aren't created until the page navigates to an
  // origin.
  base::WeakPtr<PageNode> page_node =
      PerformanceManager::GetPrimaryPageNodeForWebContents(web_contents());
  RunTaskOnPMSequence(base::BindLambdaForTesting([&]() {
    ASSERT_TRUE(page_node);
    auto& data = SiteDataRecorder::Data::FromPageNode(page_node.get());
    EXPECT_FALSE(data.reader());
    EXPECT_FALSE(data.writer());
    EXPECT_FALSE(SiteDataRecorder::Data::GetReaderForPageNode(page_node.get()));
  }));

  NavigatePageNodeOnUIThread(web_contents(), kTestUrl1);

  RunTaskOnPMSequence(base::BindLambdaForTesting([&]() {
    ASSERT_TRUE(page_node);
    auto& data = SiteDataRecorder::Data::FromPageNode(page_node.get());
    EXPECT_TRUE(data.reader());
    EXPECT_TRUE(data.writer());
    EXPECT_EQ(SiteDataRecorder::Data::GetReaderForPageNode(page_node.get()),
              data.reader());
  }));
}

}  // namespace performance_manager
