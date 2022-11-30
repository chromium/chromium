// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/v8_memory/web_memory_stress_tester.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/check.h"
#include "base/dcheck_is_on.h"
#include "base/debug/activity_tracker.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/rand_util.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/mojom/web_memory.mojom.h"
#include "components/performance_manager/public/v8_memory/web_memory.h"
#include "mojo/public/cpp/bindings/message.h"
#include "url/origin.h"

namespace performance_manager {

namespace v8_memory {

namespace {

#if DCHECK_IS_ON()
// Give the feature a different name on the Albatross build so it can get
// different parameters.
BASE_FEATURE(kStressTestFeature,
             "StressTestWebMeasureMemoryDcheck",
             base::FEATURE_DISABLED_BY_DEFAULT);
#else
BASE_FEATURE(kStressTestFeature,
             "StressTestWebMeasureMemory",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

constexpr base::FeatureParam<double> kStressTestProbabilityParam{
    &kStressTestFeature, "probability", 0.0};

class StressTestSecurityChecker : public WebMeasureMemorySecurityChecker {
 public:
  StressTestSecurityChecker() = default;
  ~StressTestSecurityChecker() override = default;

  void CheckMeasureMemoryIsAllowed(
      const FrameNode* frame_node,
      MeasureMemoryCallback measure_memory_callback,
      mojo::ReportBadMessageCallback bad_message_callback) const override {
    // No need for a security check since we are not reporting results to the
    // renderer.
    DCHECK(frame_node);
    DCHECK_ON_GRAPH_SEQUENCE(frame_node->GetGraph());
    std::move(measure_memory_callback)
        .Run(FrameNodeImpl::FromNode(frame_node)->GetWeakPtr());
  }
};

// When the production implementation would kill a renderer, instead upload a
// crash report with the message in a breadcrumb. This should only be done once
// per browser session to avoid spamming crashes.
void ReportBadMessageInCrashOnce(base::StringPiece message) {
  static bool have_crashed = false;
  if (have_crashed)
    return;
  have_crashed = true;
  base::debug::ScopedActivity scoped_activity;
  auto& user_data = scoped_activity.user_data();
  user_data.SetString("web_measure_memory_bad_mojo_message", message);
  // Crashes here should be assigned to https://crbug.com/1085129 for
  // investigation.
  base::debug::DumpWithoutCrashing();
}

}  // namespace

// static
bool WebMeasureMemoryStressTester::FeatureIsEnabled() {
  return base::FeatureList::IsEnabled(kStressTestFeature);
}

void WebMeasureMemoryStressTester::OnLoadingStateChanged(
    const PageNode* page_node,
    PageNode::LoadingState previous_state) {
  DCHECK(page_node);
  DCHECK_ON_GRAPH_SEQUENCE(page_node->GetGraph());
  if (page_node->GetLoadingState() != PageNode::LoadingState::kLoadedIdle)
    return;
  const FrameNode* main_frame = page_node->GetMainFrameNode();
  if (!main_frame)
    return;
  if (url::Origin::Create(main_frame->GetURL()).opaque())
    return;
  if (base::RandDouble() > kStressTestProbabilityParam.Get())
    return;
  WebMeasureMemory(main_frame, mojom::WebMemoryMeasurement::Mode::kDefault,
                   std::make_unique<StressTestSecurityChecker>(),
                   /*result_callback=*/base::DoNothing(),
                   base::BindOnce(&ReportBadMessageInCrashOnce));
}

void WebMeasureMemoryStressTester::OnPassedToGraph(Graph* graph) {
  graph->AddPageNodeObserver(this);
}

void WebMeasureMemoryStressTester::OnTakenFromGraph(Graph* graph) {
  graph->RemovePageNodeObserver(this);
}

}  // namespace v8_memory

}  // namespace performance_manager
