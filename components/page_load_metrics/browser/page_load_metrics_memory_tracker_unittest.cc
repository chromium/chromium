// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/page_load_metrics_memory_tracker.h"

#include "base/memory/raw_ptr.h"
#include "base/memory/singleton.h"
#include "base/test/scoped_feature_list.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_embedder_base.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_content_browser_client.h"
#include "components/performance_manager/public/render_process_host_id.h"
#include "components/performance_manager/public/v8_memory/v8_detailed_memory.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_client.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"

using V8DetailedMemoryExecutionContextData =
    performance_manager::v8_memory::V8DetailedMemoryExecutionContextData;
using FrameDataMap = base::flat_map<content::GlobalRenderFrameHostId,
                                    V8DetailedMemoryExecutionContextData>;

const char kMainUrl[] = "https://main.com/";
const char kSubUrl[] = "https://foo.com/";
const char kOtherSubUrl[] = "https://bar.com/";

namespace page_load_metrics {

namespace {

class TestPageLoadMetricsEmbedder
    : public page_load_metrics::PageLoadMetricsEmbedderBase {
 public:
  explicit TestPageLoadMetricsEmbedder(content::WebContents* web_contents)
      : PageLoadMetricsEmbedderBase(web_contents) {}
  TestPageLoadMetricsEmbedder(const TestPageLoadMetricsEmbedder&) = delete;
  TestPageLoadMetricsEmbedder& operator=(const TestPageLoadMetricsEmbedder&) =
      delete;
  ~TestPageLoadMetricsEmbedder() override = default;

  // page_load_metrics::PageLoadMetricsEmbedderBase:
  bool IsNewTabPageUrl(const GURL& url) override { return false; }
  bool IsNoStatePrefetch(content::WebContents* web_contents) override {
    return false;
  }
  bool IsExtensionUrl(const GURL& url) override { return false; }
  bool IsSidePanel(content::WebContents* web_contents) override {
    return false;
  }
  bool IsNonTabWebUI() override { return false; }

  page_load_metrics::PageLoadMetricsMemoryTracker*
  GetMemoryTrackerForBrowserContext(
      content::BrowserContext* browser_context) override {
    if (!base::FeatureList::IsEnabled(features::kV8PerFrameMemoryMonitoring))
      return nullptr;

    return &memory_tracker_;
  }

 private:
  page_load_metrics::PageLoadMetricsMemoryTracker memory_tracker_;
};

class TestMestricsWebContentsObserver : public MetricsWebContentsObserver {
 public:
  TestMestricsWebContentsObserver(
      content::WebContents* web_contents,
      std::unique_ptr<PageLoadMetricsEmbedderInterface> embedder_interface)
      : MetricsWebContentsObserver(web_contents,
                                   std::move(embedder_interface)) {}

  int num_updates_received() const { return num_updates_received_; }

  const base::flat_map<int, int64_t>& last_memory_deltas_received() const {
    return last_memory_deltas_received_;
  }

  void OnV8MemoryChanged(
      const std::vector<MemoryUpdate>& memory_updates) override {
    for (const auto& update : memory_updates) {
      num_updates_received_++;

      int routing_id = update.routing_id.frame_routing_id;
      auto it = last_memory_deltas_received_.find(routing_id);
      if (it == last_memory_deltas_received_.end())
        last_memory_deltas_received_[routing_id] = update.delta_bytes;
      else
        it->second = update.delta_bytes;
    }
  }

 private:
  base::flat_map<int, int64_t> last_memory_deltas_received_;
  int num_updates_received_ = 0;
};

class PageLoadMetricsMemoryTrackerTest
    : public content::RenderViewHostTestHarness {
 public:
  PageLoadMetricsMemoryTrackerTest() = default;
  ~PageLoadMetricsMemoryTrackerTest() override = default;
  PageLoadMetricsMemoryTrackerTest(const PageLoadMetricsMemoryTrackerTest&) =
      delete;
  PageLoadMetricsMemoryTrackerTest& operator=(
      const PageLoadMetricsMemoryTrackerTest&) = delete;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kV8PerFrameMemoryMonitoring);

    content::RenderViewHostTestHarness::SetUp();
    original_browser_client_ =
        content::SetBrowserClientForTesting(&browser_client_);

    auto embedder_interface =
        std::make_unique<TestPageLoadMetricsEmbedder>(web_contents());
    embedder_interface_ = embedder_interface.get();
    observer_ = new TestMestricsWebContentsObserver(
        web_contents(), std::move(embedder_interface));
    web_contents()->SetUserData(TestMestricsWebContentsObserver::UserDataKey(),
                                base::WrapUnique(observer_.get()));

    tracker_ = embedder_interface_->GetMemoryTrackerForBrowserContext(
        browser_context());
  }

  void TearDown() override {
    content::SetBrowserClientForTesting(original_browser_client_);
    tracker_->Shutdown();
    content::RenderViewHostTestHarness::TearDown();
  }

  // Returns the final RenderFrameHost after navigation commits.
  content::RenderFrameHost* NavigateFrame(const std::string& url,
                                          content::RenderFrameHost* frame) {
    auto navigation_simulator =
        content::NavigationSimulator::CreateRendererInitiated(GURL(url), frame);
    navigation_simulator->Commit();
    return navigation_simulator->GetFinalRenderFrameHost();
  }

  // Returns the final RenderFrameHost after navigation commits.
  content::RenderFrameHost* NavigateMainFrame(const std::string& url) {
    return NavigateFrame(url, web_contents()->GetPrimaryMainFrame());
  }

  // Returns the final RenderFrameHost after navigation commits.
  content::RenderFrameHost* CreateAndNavigateSubFrame(
      const std::string& url,
      content::RenderFrameHost* parent) {
    content::RenderFrameHost* subframe =
        content::RenderFrameHostTester::For(parent)->AppendChild("frame_name");
    auto navigation_simulator =
        content::NavigationSimulator::CreateRendererInitiated(GURL(url),
                                                              subframe);
    navigation_simulator->Commit();

    return navigation_simulator->GetFinalRenderFrameHost();
  }

  void SimulateMemoryMeasurementUpdate(
      content::RenderFrameHost* render_frame_host,
      uint64_t bytes) {
    if (!render_frame_host || !render_frame_host->GetProcess())
      return;

    content::GlobalRenderFrameHostId global_routing_id =
        render_frame_host->GetGlobalId();
    int process_id = render_frame_host->GetProcess()->GetID();

    performance_manager::RenderProcessHostId pm_process_id =
        static_cast<performance_manager::RenderProcessHostId>(process_id);
    performance_manager::v8_memory::V8DetailedMemoryProcessData process_data;
    V8DetailedMemoryExecutionContextData frame_data;
    frame_data.set_v8_bytes_used(bytes);

    FrameDataMap frame_map;
    frame_map[global_routing_id] = frame_data;

    tracker_->OnV8MemoryMeasurementAvailable(pm_process_id, process_data,
                                             frame_map);
  }

  int num_updates_received() const { return observer_->num_updates_received(); }

  const base::flat_map<int, int64_t>& last_memory_deltas_received() const {
    return observer_->last_memory_deltas_received();
  }

 protected:
  raw_ptr<PageLoadMetricsMemoryTracker, DanglingUntriaged> tracker_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<TestMestricsWebContentsObserver, DanglingUntriaged> observer_;
  raw_ptr<TestPageLoadMetricsEmbedder, DanglingUntriaged> embedder_interface_;
  PageLoadMetricsTestContentBrowserClient browser_client_;
  raw_ptr<content::ContentBrowserClient> original_browser_client_ = nullptr;
};

}  // namespace

TEST_F(PageLoadMetricsMemoryTrackerTest,
       InitialUpdatesOnly_CorrectDeltasReceived) {
  content::RenderFrameHost* main_frame = NavigateMainFrame(kMainUrl);
  int main_id = main_frame->GetRoutingID();
  content::RenderFrameHost* sub_frame1 =
      CreateAndNavigateSubFrame(kSubUrl, main_frame);
  int sub_frame1_id = sub_frame1->GetRoutingID();

  // Create a nested subframe with the same origin as its parent.
  content::RenderFrameHost* sub_frame2 =
      CreateAndNavigateSubFrame(kOtherSubUrl, sub_frame1);
  int sub_frame2_id = sub_frame2->GetRoutingID();

  SimulateMemoryMeasurementUpdate(main_frame, 100 * 1024);
  SimulateMemoryMeasurementUpdate(sub_frame1, 200 * 1024);
  SimulateMemoryMeasurementUpdate(sub_frame2, 300 * 1024);

  auto deltas_received = last_memory_deltas_received();
  EXPECT_EQ(3, num_updates_received());
  ASSERT_EQ(3UL, deltas_received.size());

  EXPECT_TRUE(deltas_received.find(main_id) != deltas_received.end());
  EXPECT_EQ(100L, deltas_received[main_id] / 1024);
  EXPECT_TRUE(deltas_received.find(sub_frame1_id) != deltas_received.end());
  EXPECT_EQ(200L, deltas_received[sub_frame1_id] / 1024);
  EXPECT_TRUE(deltas_received.find(sub_frame2_id) != deltas_received.end());
  EXPECT_EQ(300L, deltas_received[sub_frame2_id] / 1024);
}

TEST_F(PageLoadMetricsMemoryTrackerTest, SecondUpdates_CorrectDeltasReceived) {
  content::RenderFrameHost* main_frame = NavigateMainFrame(kMainUrl);
  int main_id = main_frame->GetRoutingID();
  content::RenderFrameHost* sub_frame1 =
      CreateAndNavigateSubFrame(kSubUrl, main_frame);
  int sub_frame1_id = sub_frame1->GetRoutingID();

  // Create a nested subframe with the same origin as its parent.
  content::RenderFrameHost* sub_frame2 =
      CreateAndNavigateSubFrame(kOtherSubUrl, sub_frame1);
  int sub_frame2_id = sub_frame2->GetRoutingID();

  SimulateMemoryMeasurementUpdate(main_frame, 100 * 1024);
  SimulateMemoryMeasurementUpdate(sub_frame1, 200 * 1024);
  SimulateMemoryMeasurementUpdate(sub_frame2, 300 * 1024);

  // Simulate second round of updates.
  SimulateMemoryMeasurementUpdate(main_frame, 50 * 1024);
  SimulateMemoryMeasurementUpdate(sub_frame1, 300 * 1024);
  SimulateMemoryMeasurementUpdate(sub_frame2, 100 * 1024);

  auto deltas_received = last_memory_deltas_received();
  EXPECT_EQ(6, num_updates_received());
  ASSERT_EQ(3UL, deltas_received.size());

  EXPECT_TRUE(deltas_received.find(main_id) != deltas_received.end());
  EXPECT_EQ(-50L, deltas_received[main_id] / 1024);
  EXPECT_TRUE(deltas_received.find(sub_frame1_id) != deltas_received.end());
  EXPECT_EQ(100L, deltas_received[sub_frame1_id] / 1024);
  EXPECT_TRUE(deltas_received.find(sub_frame2_id) != deltas_received.end());
  EXPECT_EQ(-200L, deltas_received[sub_frame2_id] / 1024);
}

TEST_F(PageLoadMetricsMemoryTrackerTest, FrameDeleted_CorrectDeltasReceived) {
  content::RenderFrameHost* main_frame = NavigateMainFrame(kMainUrl);
  int main_id = main_frame->GetRoutingID();
  content::RenderFrameHost* sub_frame =
      CreateAndNavigateSubFrame(kSubUrl, main_frame);
  int sub_frame_id = sub_frame->GetRoutingID();

  SimulateMemoryMeasurementUpdate(main_frame, 100 * 1024);
  SimulateMemoryMeasurementUpdate(sub_frame, 200 * 1024);

  // Delete |sub_frame| and refresh the usage map. An update should have been
  // received that will make the usage corresponding to |sub_frame| zero.
  content::RenderFrameHostTester::For(sub_frame)->Detach();

  auto deltas_received = last_memory_deltas_received();
  EXPECT_EQ(3, num_updates_received());
  ASSERT_EQ(2UL, deltas_received.size());

  EXPECT_TRUE(deltas_received.find(main_id) != deltas_received.end());
  EXPECT_EQ(100L, deltas_received[main_id] / 1024);
  EXPECT_TRUE(deltas_received.find(sub_frame_id) != deltas_received.end());
  EXPECT_EQ(-200L, deltas_received[sub_frame_id] / 1024);
}

}  // namespace page_load_metrics
