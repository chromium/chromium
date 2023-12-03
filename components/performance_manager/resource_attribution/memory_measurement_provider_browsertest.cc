// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/resource_attribution/memory_measurement_provider.h"

#include <map>
#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/resource_attribution/query_results.h"
#include "components/performance_manager/public/resource_attribution/resource_contexts.h"
#include "components/performance_manager/test_support/performance_manager_browsertest_harness.h"
#include "components/performance_manager/test_support/resource_attribution/gtest_util.h"
#include "components/performance_manager/test_support/run_in_graph.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/gurl.h"

namespace performance_manager::resource_attribution {

namespace {

using ::testing::_;
using ::testing::AllOf;
using ::testing::Eq;
using ::testing::Field;
using ::testing::Gt;
using ::testing::IsEmpty;
using ::testing::IsSupersetOf;
using ::testing::Lt;
using ::testing::Pair;
using ::testing::VariantWith;
using ResultMap = std::map<ResourceContext, QueryResult>;

class ResourceAttrMemoryMeasurementProviderBrowserTest
    : public PerformanceManagerBrowserTestHarness {
 protected:
  using Super = PerformanceManagerBrowserTestHarness;

  void OnGraphCreated(Graph* graph) override {
    memory_provider_ = std::make_unique<MemoryMeasurementProvider>(graph);
    Super::OnGraphCreated(graph);
  }

  void TearDownOnMainThread() override {
    // Delete MemoryMeasurementProvider before tearing down the graph to avoid
    // dangling pointers.
    RunInGraph([&] { memory_provider_.reset(); });
    Super::TearDownOnMainThread();
  }

  // Calls MemoryMeasurementProvider::RequestMemorySummary, waits for its result
  // callback to fire, and returns the result map passed to the callback.
  ResultMap WaitForMemorySummary() {
    ResultMap return_results;
    RunInGraph([&](base::OnceClosure quit_closure) {
      memory_provider_->RequestMemorySummary(
          base::BindOnce(base::BindLambdaForTesting([&](ResultMap results) {
            return_results = std::move(results);
          })).Then(std::move(quit_closure)));
    });
    return return_results;
  }

 private:
  std::unique_ptr<MemoryMeasurementProvider> memory_provider_;
};

// GMock matcher expecting that a given MemorySummaryResult has the metadata
// filled in and all memory measurements >0.
auto MemorySummaryResultIsPositive() {
  return VariantWith<MemorySummaryResult>(AllOf(
      Field("metadata", &MemorySummaryResult::metadata,
            Field("measurement_time", &ResultMetadata::measurement_time,
                  AllOf(Gt(base::TimeTicks()), Lt(base::TimeTicks::Now())))),
#if BUILDFLAG(IS_IOS)
      // TODO(crbug.com/1506552): iOS doesn't support private_memory_footprint,
      // so it's always 0.
      Field("private_footprint_kb", &MemorySummaryResult::private_footprint_kb,
            Eq(0u)),
#else
      Field("private_footprint_kb", &MemorySummaryResult::private_footprint_kb,
            Gt(0u)),
#endif
      Field("resident_set_size_kb", &MemorySummaryResult::resident_set_size_kb,
            Gt(0u))));
}

IN_PROC_BROWSER_TEST_F(ResourceAttrMemoryMeasurementProviderBrowserTest,
                       TwoFramesOneProcess) {
  // Navigate to an initial URL that will load 2 frames in the same process.
  ASSERT_TRUE(NavigateAndWaitForConsoleMessage(
      shell()->web_contents(),
      embedded_test_server()->GetURL("a.com", "/a_embeds_a.html"),
      "a.html loaded"));

  content::RenderFrameHost* main_rfh =
      shell()->web_contents()->GetPrimaryMainFrame();
  ASSERT_TRUE(main_rfh);
  content::RenderFrameHost* child_rfh = ChildFrameAt(main_rfh, 0);
  ASSERT_TRUE(child_rfh);
  ASSERT_TRUE(main_rfh->GetProcess());
  ASSERT_EQ(main_rfh->GetProcess(), child_rfh->GetProcess());

  const ResourceContext main_frame_context =
      FrameContext::FromRenderFrameHost(main_rfh).value();
  const ResourceContext child_frame_context =
      FrameContext::FromRenderFrameHost(child_rfh).value();
  const ResourceContext process_context =
      ProcessContext::FromRenderProcessHost(main_rfh->GetProcess()).value();

  // Measure the memory of all processes. Results will include the browser and
  // utility processes.
  ResultMap results = WaitForMemorySummary();
  EXPECT_THAT(
      results,
      IsSupersetOf(
          {Pair(process_context, MemorySummaryResultIsPositive()),
           Pair(main_frame_context, MemorySummaryResultIsPositive()),
           Pair(child_frame_context, MemorySummaryResultIsPositive())}));

  // The process memory should be split between frames in the
  // process.
  const auto main_frame_result =
      absl::get<MemorySummaryResult>(results.at(main_frame_context));
  const auto child_frame_result =
      absl::get<MemorySummaryResult>(results.at(child_frame_context));
  const auto process_result =
      absl::get<MemorySummaryResult>(results.at(process_context));
  EXPECT_LE(main_frame_result.resident_set_size_kb,
            process_result.resident_set_size_kb);
  EXPECT_LE(main_frame_result.private_footprint_kb,
            process_result.private_footprint_kb);
  EXPECT_LE(child_frame_result.resident_set_size_kb,
            process_result.resident_set_size_kb);
  EXPECT_LE(child_frame_result.private_footprint_kb,
            process_result.private_footprint_kb);
}

IN_PROC_BROWSER_TEST_F(ResourceAttrMemoryMeasurementProviderBrowserTest,
                       TwoFramesSeparateProcesses) {
  // Navigate to an initial URL that will load 2 frames in separate processes.
  content::IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());
  ASSERT_TRUE(NavigateAndWaitForConsoleMessage(
      shell()->web_contents(),
      embedded_test_server()->GetURL("a.com", "/a_embeds_b.html"),
      "b.html loaded"));

  content::RenderFrameHost* main_rfh =
      shell()->web_contents()->GetPrimaryMainFrame();
  ASSERT_TRUE(main_rfh);
  content::RenderFrameHost* child_rfh = ChildFrameAt(main_rfh, 0);
  ASSERT_TRUE(child_rfh);
  ASSERT_TRUE(main_rfh->GetProcess());
  ASSERT_NE(main_rfh->GetProcess(), child_rfh->GetProcess());

  const ResourceContext main_frame_context =
      FrameContext::FromRenderFrameHost(main_rfh).value();
  const ResourceContext child_frame_context =
      FrameContext::FromRenderFrameHost(child_rfh).value();
  const ResourceContext process_a_context =
      ProcessContext::FromRenderProcessHost(main_rfh->GetProcess()).value();
  const ResourceContext process_b_context =
      ProcessContext::FromRenderProcessHost(child_rfh->GetProcess()).value();

  // Measure the memory of all processes. Results will include the browser and
  // utility processes.
  ResultMap results = WaitForMemorySummary();
  EXPECT_THAT(
      results,
      IsSupersetOf(
          {Pair(process_a_context, MemorySummaryResultIsPositive()),
           Pair(process_b_context, MemorySummaryResultIsPositive()),
           Pair(main_frame_context, MemorySummaryResultIsPositive()),
           Pair(child_frame_context, MemorySummaryResultIsPositive())}));

  // Each process memory should be assigned entirely to the frame in the
  // process.
  const auto main_frame_result =
      absl::get<MemorySummaryResult>(results.at(main_frame_context));
  const auto child_frame_result =
      absl::get<MemorySummaryResult>(results.at(child_frame_context));
  const auto process_a_result =
      absl::get<MemorySummaryResult>(results.at(process_a_context));
  const auto process_b_result =
      absl::get<MemorySummaryResult>(results.at(process_b_context));
  EXPECT_EQ(main_frame_result.resident_set_size_kb,
            process_a_result.resident_set_size_kb);
  EXPECT_EQ(main_frame_result.private_footprint_kb,
            process_a_result.private_footprint_kb);
  EXPECT_EQ(child_frame_result.resident_set_size_kb,
            process_b_result.resident_set_size_kb);
  EXPECT_EQ(child_frame_result.private_footprint_kb,
            process_b_result.private_footprint_kb);
}

}  // namespace

}  // namespace performance_manager::resource_attribution
