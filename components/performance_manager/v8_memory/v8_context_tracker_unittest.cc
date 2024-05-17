// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/v8_memory/v8_context_tracker.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/test/gtest_util.h"
#include "base/types/optional_util.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/public/execution_context/execution_context_registry.h"
#include "components/performance_manager/public/mojom/v8_contexts.mojom.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/mock_graphs.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace performance_manager {
namespace v8_memory {

namespace {

using ::testing::AllOf;
using ::testing::Eq;
using ::testing::Property;

// Fake iframe attributes.
const std::string kIframeId("iframe1");
const std::string kIframeSrc("http://www.fakesite.com/");

// Some tokens for identifying frames and contexts.
const blink::V8ContextToken kFrameMainWorld;
const blink::V8ContextToken kChildFrameMainWorld;
const blink::V8ContextToken kFrameIsolatedWorld;
const blink::V8ContextToken kChildFrameIsolatedWorld;
const blink::RemoteFrameToken kChildFrameRemoteToken;

// A fake extension ID.
const char kExtensionId[] = "hickenlcldoffnfidnljacmfeielknka";

// Helper function for creating an IframeAttributionData.
mojom::IframeAttributionDataPtr GetFakeIframeAttributionDataPtr() {
  static const mojom::IframeAttributionData kData(kIframeId, kIframeSrc);
  mojom::IframeAttributionDataPtr iad = mojom::IframeAttributionData::New();
  *iad = kData;
  return iad;
}

class V8ContextTrackerTest : public GraphTestHarness {
 public:
  V8ContextTrackerTest() = default;

  ~V8ContextTrackerTest() override = default;

  void OnGraphCreated(GraphImpl* graph_impl) override {
    registry_ =
        execution_context::ExecutionContextRegistry::GetFromGraph(graph());
    tracker_ = graph_impl->PassToGraph(std::make_unique<V8ContextTracker>());
    mock_graph_ =
        std::make_unique<MockSinglePageWithMultipleProcessesGraph>(graph());
  }

  raw_ptr<execution_context::ExecutionContextRegistry> registry_ = nullptr;
  // This field is not a raw_ptr<> because of incompatibilities with tracing
  // (TRACE_EVENT*), perfetto::TracedDictionary::Add and gmock/EXPECT_THAT.
  RAW_PTR_EXCLUSION V8ContextTracker* tracker_ = nullptr;
  std::unique_ptr<MockSinglePageWithMultipleProcessesGraph> mock_graph_;
};

using V8ContextTrackerDeathTest = V8ContextTrackerTest;

auto CountsMatch(size_t v8_context_count, size_t execution_context_count) {
  return AllOf(Property(&V8ContextTracker::GetV8ContextCountForTesting,
                        Eq(v8_context_count)),
               Property(&V8ContextTracker::GetExecutionContextCountForTesting,
                        Eq(execution_context_count)));
}

auto DetachedCountsMatch(size_t detached_v8_context_count,
                         size_t destroyed_execution_context_count) {
  return AllOf(
      Property(&V8ContextTracker::GetDetachedV8ContextCountForTesting,
               Eq(detached_v8_context_count)),
      Property(&V8ContextTracker::GetDestroyedExecutionContextCountForTesting,
               Eq(destroyed_execution_context_count)));
}

}  // namespace

TEST_F(V8ContextTrackerDeathTest, MissingExecutionContextForMainFrameExplodes) {
  // A main-frame should not have iframe data, and it must have an execution
  // context token. So this should fail.
  EXPECT_DCHECK_DEATH(tracker_->OnV8ContextCreated(
      ProcessNodeImpl::CreatePassKeyForTesting(), mock_graph_->process.get(),
      mojom::V8ContextDescription(
          /* token */ kFrameMainWorld,
          /* world_type */ mojom::V8ContextWorldType::kMain,
          /* world_name */ std::nullopt,
          /* execution_context_token */ std::nullopt),
      /* iframe_attribution_data */ nullptr));
}

TEST_F(V8ContextTrackerDeathTest, DoubleCreationExplodes) {
  auto v8_desc = mojom::V8ContextDescription(
      /* token */ kFrameMainWorld,
      /* world_type */ mojom::V8ContextWorldType::kMain,
      /* world_name */ std::nullopt,
      /* execution_context_token */ mock_graph_->frame->GetFrameToken());

  tracker_->OnV8ContextCreated(ProcessNodeImpl::CreatePassKeyForTesting(),
                               mock_graph_->process.get(), v8_desc, nullptr);

  // Trying to create the context a second time should explode.
  EXPECT_DCHECK_DEATH(tracker_->OnV8ContextCreated(
      ProcessNodeImpl::CreatePassKeyForTesting(), mock_graph_->process.get(),
      v8_desc, nullptr));
}

TEST_F(V8ContextTrackerDeathTest, MissingContextExplodes) {
  // There was no OnV8ContextCreated called first, so this should explode.
  EXPECT_DCHECK_DEATH(tracker_->OnV8ContextDetached(
      ProcessNodeImpl::CreatePassKeyForTesting(), mock_graph_->process.get(),
      kFrameMainWorld));

  // Similarly with a destroyed notification.
  EXPECT_DCHECK_DEATH(tracker_->OnV8ContextDestroyed(
      ProcessNodeImpl::CreatePassKeyForTesting(), mock_graph_->process.get(),
      kFrameMainWorld));
}

TEST_F(V8ContextTrackerDeathTest, DoubleRemoteFrameCreatedExplodes) {
  tracker_->OnRemoteIframeAttachedForTesting(
      mock_graph_->child_frame.get(), mock_graph_->frame.get(),
      kChildFrameRemoteToken, GetFakeIframeAttributionDataPtr());

  EXPECT_DCHECK_DEATH(tracker_->OnRemoteIframeAttachedForTesting(
      mock_graph_->child_frame.get(), mock_graph_->frame.get(),
      kChildFrameRemoteToken, GetFakeIframeAttributionDataPtr()));
}

TEST_F(V8ContextTrackerDeathTest, RemoteFrameWithBadParentExplodes) {
  // child_frame's real parent is frame.
  EXPECT_DCHECK_DEATH(tracker_->OnRemoteIframeAttachedForTesting(
      mock_graph_->child_frame.get(), mock_graph_->child_frame.get(),
      kChildFrameRemoteToken, GetFakeIframeAttributionDataPtr()));
}

TEST_F(V8ContextTrackerDeathTest, IframeAttributionDataForMainFrameExplodes) {
  EXPECT_DCHECK_DEATH(tracker_->OnV8ContextCreated(
      ProcessNodeImpl::CreatePassKeyForTesting(), mock_graph_->process.get(),
      mojom::V8ContextDescription(
          /* token */ kFrameMainWorld,
          /* world_type */ mojom::V8ContextWorldType::kMain,
          /* world_name */ std::nullopt,
          /* execution_context_token */ mock_graph_->frame->GetFrameToken()),
      GetFakeIframeAttributionDataPtr()));
}

TEST_F(V8ContextTrackerDeathTest,
       NoIframeAttributionDataForInProcessChildFrameExplodes) {
  // Create a child of mock_graph_->frame that is in the same process.
  TestNodeWrapper<FrameNodeImpl> child2_frame(graph()->CreateFrameNodeAutoId(
      mock_graph_->process.get(), mock_graph_->page.get(),
      mock_graph_->frame.get()));
  // This should explode because synchronous iframe data is expected, but not
  // provided.
  EXPECT_DCHECK_DEATH(tracker_->OnV8ContextCreated(
      ProcessNodeImpl::CreatePassKeyForTesting(), mock_graph_->process.get(),
      mojom::V8ContextDescription(
          /* token */ kChildFrameMainWorld,
          /* world_type */ mojom::V8ContextWorldType::kMain,
          /* world_name */ std::nullopt,
          /* execution_context_token */ child2_frame->GetFrameToken()),
      /* iframe_attribution_data */ nullptr));
}

TEST_F(V8ContextTrackerDeathTest, MultipleMainContextsForExecutionContext) {
  // Create a main-frame with two main worlds.
  tracker_->OnV8ContextCreated(
      ProcessNodeImpl::CreatePassKeyForTesting(), mock_graph_->process.get(),
      mojom::V8ContextDescription(
          /* token */ kFrameMainWorld,
          /* world_type */ mojom::V8ContextWorldType::kMain,
          /* world_name */ std::nullopt,
          /* execution_context_token */ mock_graph_->frame->GetFrameToken()),
      /* iframe_attribution_data */ nullptr);

  EXPECT_DCHECK_DEATH(tracker_->OnV8ContextCreated(
      ProcessNodeImpl::CreatePassKeyForTesting(), mock_graph_->process.get(),
      mojom::V8ContextDescription(
          /* token */ blink::V8ContextToken(),
          /* world_type */ mojom::V8ContextWorldType::kMain,
          /* world_name */ std::nullopt,
          /* execution_context_token */ mock_graph_->frame->GetFrameToken()),
      /* iframe_attribution_data */ nullptr));
}

TEST_F(V8ContextTrackerTest, NormalV8ContextLifecycleWithExecutionContext) {
  EXPECT_THAT(tracker_, CountsMatch(0, 0));
  EXPECT_THAT(tracker_, DetachedCountsMatch(0, 0));

  tracker_->OnV8ContextCreated(
      ProcessNodeImpl::CreatePassKeyForTesting(), mock_graph_->process.get(),
      mojom::V8ContextDescription(
          /* token */ kFrameMainWorld,
          /* world_type */ mojom::V8ContextWorldType::kMain,
          /* world_name */ std::nullopt,
          /* execution_context_token */ mock_graph_->frame->GetFrameToken()),
      /* iframe_attribution_data */ nullptr);
  EXPECT_THAT(tracker_, CountsMatch(1, 1));
  EXPECT_THAT(tracker_, DetachedCountsMatch(0, 0));

  tracker_->OnV8ContextDetached(ProcessNodeImpl::CreatePassKeyForTesting(),
                                mock_graph_->process.get(), kFrameMainWorld);
  EXPECT_THAT(tracker_, CountsMatch(1, 1));
  EXPECT_THAT(tracker_, DetachedCountsMatch(1, 0));

  tracker_->OnV8ContextDestroyed(ProcessNodeImpl::CreatePassKeyForTesting(),
                                 mock_graph_->process.get(), kFrameMainWorld);
  EXPECT_THAT(tracker_, CountsMatch(0, 0));
  EXPECT_THAT(tracker_, DetachedCountsMatch(0, 0));
}

TEST_F(V8ContextTrackerTest, NormalV8ContextLifecycleNoExecutionContext) {
  EXPECT_THAT(tracker_, CountsMatch(0, 0));
  EXPECT_THAT(tracker_, DetachedCountsMatch(0, 0));

  tracker_->OnV8ContextCreated(
      ProcessNodeImpl::CreatePassKeyForTesting(), mock_graph_->process.get(),
      mojom::V8ContextDescription(
          /* token */ kFrameMainWorld,
          /* world_type */ mojom::V8ContextWorldType::kRegExp,
          /* world_name */ std::nullopt,
          /* execution_context_token */ std::nullopt),
      /* iframe_attribution_data */ nullptr);
  EXPECT_THAT(tracker_, CountsMatch(1, 0));
  EXPECT_THAT(tracker_, DetachedCountsMatch(0, 0));

  tracker_->OnV8ContextDetached(ProcessNodeImpl::CreatePassKeyForTesting(),
                                mock_graph_->process.get(), kFrameMainWorld);
  EXPECT_THAT(tracker_, CountsMatch(1, 0));
  EXPECT_THAT(tracker_, DetachedCountsMatch(1, 0));

  tracker_->OnV8ContextDestroyed(ProcessNodeImpl::CreatePassKeyForTesting(),
                                 mock_graph_->process.get(), kFrameMainWorld);
  EXPECT_THAT(tracker_, CountsMatch(0, 0));
  EXPECT_THAT(tracker_, DetachedCountsMatch(0, 0));
}

TEST_F(V8ContextTrackerTest, MultipleV8ContextsForExecutionContext) {
  EXPECT_THAT(tracker_, CountsMatch(0, 0));
  EXPECT_THAT(tracker_, DetachedCountsMatch(0, 0));

  // Create a main-frame main world context, and an isolated world.
  {
    SCOPED_TRACE("");
    tracker_->OnV8ContextCreated(
        ProcessNodeImpl::CreatePassKeyForTesting(), mock_graph_->process.get(),
        mojom::V8ContextDescription(
            /* token */ kFrameMainWorld,
            /* world_type */ mojom::V8ContextWorldType::kMain,
            /* world_name */ std::nullopt,
            /* execution_context_token */ mock_graph_->frame->GetFrameToken()),
        /* iframe_attribution_data */ nullptr);
    EXPECT_THAT(tracker_, CountsMatch(1, 1));
    EXPECT_THAT(tracker_, DetachedCountsMatch(0, 0));
  }
  {
    SCOPED_TRACE("");
    tracker_->OnV8ContextCreated(
        ProcessNodeImpl::CreatePassKeyForTesting(), mock_graph_->process.get(),
        mojom::V8ContextDescription(
            /* token */ kFrameIsolatedWorld,
            /* world_type */ mojom::V8ContextWorldType::kExtension,
            /* world_name */ kExtensionId,
            /* execution_context_token */ mock_graph_->frame->GetFrameToken()),
        /* iframe_attribution_data */ nullptr);
    EXPECT_THAT(tracker_, CountsMatch(2, 1));
    EXPECT_THAT(tracker_, DetachedCountsMatch(0, 0));
  }

  // Create a child-frame main world context, and an isolated world. This child
  // is cross-process so expects no iframe data at creation.
  {
    SCOPED_TRACE("");
    tracker_->OnV8ContextCreated(
        ProcessNodeImpl::CreatePassKeyForTesting(),
        mock_graph_->other_process.get(),
        mojom::V8ContextDescription(
            /* token */ kChildFrameMainWorld,
            /* world_type */ mojom::V8ContextWorldType::kMain,
            /* world_name */ std::nullopt,
            /* execution_context_token */
            mock_graph_->child_frame->GetFrameToken()),
        /* iframe_attribution_data */ nullptr);
    EXPECT_THAT(tracker_, CountsMatch(3, 2));
    EXPECT_THAT(tracker_, DetachedCountsMatch(0, 0));
  }
  {
    SCOPED_TRACE("");
    tracker_->OnV8ContextCreated(
        ProcessNodeImpl::CreatePassKeyForTesting(),
        mock_graph_->other_process.get(),
        mojom::V8ContextDescription(
            /* token */ kChildFrameIsolatedWorld,
            /* world_type */ mojom::V8ContextWorldType::kExtension,
            /* world_name */ kExtensionId,
            /* execution_context_token */
            mock_graph_->child_frame->GetFrameToken()),
        /* iframe_attribution_data */ nullptr);
    EXPECT_THAT(tracker_, CountsMatch(4, 2));
    EXPECT_THAT(tracker_, DetachedCountsMatch(0, 0));
  }

  // Provide iframe data for the child frame.
  {
    SCOPED_TRACE("");
    tracker_->OnRemoteIframeAttachedForTesting(
        mock_graph_->child_frame.get(), mock_graph_->frame.get(),
        kChildFrameRemoteToken, GetFakeIframeAttributionDataPtr());
    EXPECT_THAT(tracker_, CountsMatch(4, 2));
    EXPECT_THAT(tracker_, DetachedCountsMatch(0, 0));
  }

  // Detach the child frame contexts.
  {
    SCOPED_TRACE("");
    tracker_->OnV8ContextDetached(ProcessNodeImpl::CreatePassKeyForTesting(),
                                  mock_graph_->other_process.get(),
                                  kChildFrameIsolatedWorld);
    EXPECT_THAT(tracker_, CountsMatch(4, 2));
    EXPECT_THAT(tracker_, DetachedCountsMatch(1, 0));
  }
  {
    SCOPED_TRACE("");
    tracker_->OnV8ContextDetached(ProcessNodeImpl::CreatePassKeyForTesting(),
                                  mock_graph_->other_process.get(),
                                  kChildFrameMainWorld);
    EXPECT_THAT(tracker_, CountsMatch(4, 2));
    EXPECT_THAT(tracker_, DetachedCountsMatch(2, 0));
  }

  // Destroy the child frame main world context.
  {
    SCOPED_TRACE("");
    tracker_->OnV8ContextDestroyed(ProcessNodeImpl::CreatePassKeyForTesting(),
                                   mock_graph_->other_process.get(),
                                   kChildFrameMainWorld);
    EXPECT_THAT(tracker_, CountsMatch(3, 2));
    EXPECT_THAT(tracker_, DetachedCountsMatch(1, 0));
  }

  // Detach the main frame contexts, main and isolated.
  {
    SCOPED_TRACE("");
    tracker_->OnV8ContextDetached(ProcessNodeImpl::CreatePassKeyForTesting(),
                                  mock_graph_->process.get(), kFrameMainWorld);
    EXPECT_THAT(tracker_, CountsMatch(3, 2));
    EXPECT_THAT(tracker_, DetachedCountsMatch(2, 0));
  }
  {
    SCOPED_TRACE("");
    tracker_->OnV8ContextDetached(ProcessNodeImpl::CreatePassKeyForTesting(),
                                  mock_graph_->process.get(),
                                  kFrameIsolatedWorld);
    EXPECT_THAT(tracker_, CountsMatch(3, 2));
    EXPECT_THAT(tracker_, DetachedCountsMatch(3, 0));
  }

  // Destroy the main frame isolated world.
  {
    SCOPED_TRACE("");
    tracker_->OnV8ContextDestroyed(ProcessNodeImpl::CreatePassKeyForTesting(),
                                   mock_graph_->process.get(),
                                   kFrameIsolatedWorld);
    EXPECT_THAT(tracker_, CountsMatch(2, 2));
    EXPECT_THAT(tracker_, DetachedCountsMatch(2, 0));
  }

  // Destroy the child frame isolated world..
  {
    SCOPED_TRACE("");
    tracker_->OnV8ContextDestroyed(ProcessNodeImpl::CreatePassKeyForTesting(),
                                   mock_graph_->other_process.get(),
                                   kChildFrameIsolatedWorld);
    EXPECT_THAT(tracker_, CountsMatch(1, 2));
    EXPECT_THAT(tracker_, DetachedCountsMatch(1, 0));
  }

  // Destroy the remote iframe reference to the child frame, which should
  // finally tear down the ExecutionContext as well.
  {
    SCOPED_TRACE("");
    tracker_->OnRemoteIframeDetachedForTesting(mock_graph_->frame.get(),
                                               kChildFrameRemoteToken);
    EXPECT_THAT(tracker_, CountsMatch(1, 1));
    EXPECT_THAT(tracker_, DetachedCountsMatch(1, 0));
  }
}

TEST_F(V8ContextTrackerTest, AllEventOrders) {
  EXPECT_THAT(tracker_, CountsMatch(0, 0));
  EXPECT_THAT(tracker_, DetachedCountsMatch(0, 0));

  // Create a main frame. This exists for the duration of the test, and we
  // repeatedly attach/detach child frames to it.
  tracker_->OnV8ContextCreated(
      ProcessNodeImpl::CreatePassKeyForTesting(), mock_graph_->process.get(),
      mojom::V8ContextDescription(
          /* token */ kFrameMainWorld,
          /* world_type */ mojom::V8ContextWorldType::kMain,
          /* world_name */ std::nullopt,
          /* execution_context_token */ mock_graph_->frame->GetFrameToken()),
      /* iframe_attribution_data */ nullptr);
  EXPECT_THAT(tracker_, CountsMatch(1, 1));
  EXPECT_THAT(tracker_, DetachedCountsMatch(0, 0));

  // Bind lambdas for all the events that can occur to a frame in its lifetime.
  // This test will explore all possible valid combinations of these events.

  // Creates a child frame V8Context.
  auto v8create = [self = this]() {
    SCOPED_TRACE("");
    self->tracker_->OnV8ContextCreated(
        ProcessNodeImpl::CreatePassKeyForTesting(),
        self->mock_graph_->other_process.get(),
        mojom::V8ContextDescription(
            /* token */ kChildFrameMainWorld,
            /* world_type */ mojom::V8ContextWorldType::kMain,
            /* world_name */ std::nullopt,
            /* execution_context_token */
            self->mock_graph_->child_frame->GetFrameToken()),
        /* iframe_attribution_data */ nullptr);
  };

  // Detaches a child frame V8Context.
  auto v8detach = [self = this]() {
    SCOPED_TRACE("");
    self->tracker_->OnV8ContextDetached(
        ProcessNodeImpl::CreatePassKeyForTesting(),
        self->mock_graph_->other_process.get(), kChildFrameMainWorld);
  };

  // Destroys a child frame V8Context.
  auto v8destroy = [self = this]() {
    SCOPED_TRACE("");
    self->tracker_->OnV8ContextDestroyed(
        ProcessNodeImpl::CreatePassKeyForTesting(),
        self->mock_graph_->other_process.get(), kChildFrameMainWorld);
  };

  // Attaches a child iframe. This is after frame resolution has occurred so it
  // is aimed at the frame that is represented by the remote frame token
  // (hence mock_graph_->child_frame). The actual "OnRemoteIframeAttached"
  // message originally arrives over the parent frame interface.
  auto iframeattach = [self = this]() {
    SCOPED_TRACE("");
    self->tracker_->OnRemoteIframeAttachedForTesting(
        self->mock_graph_->child_frame.get(), self->mock_graph_->frame.get(),
        kChildFrameRemoteToken, GetFakeIframeAttributionDataPtr());
  };

  // Detaches a child iframe. This message is sent over the interface associated
  // with the parent frame that hosts the child frame (hence
  // mock_graph_->frame).
  auto iframedetach = [self = this]() {
    SCOPED_TRACE("");
    self->tracker_->OnRemoteIframeDetachedForTesting(
        self->mock_graph_->frame.get(), kChildFrameRemoteToken);
  };

  // The following tests look at all 10 possible orderings of the 3 ordered
  // V8Context events interleaved with the 2 ordered Iframe events.

  {
    SCOPED_TRACE("Case 1");
    v8create();
    EXPECT_THAT(tracker_, CountsMatch(2, 2));
    EXPECT_THAT(tracker_, DetachedCountsMatch(0, 0));
    v8detach();
    EXPECT_THAT(tracker_, CountsMatch(2, 2));
    EXPECT_THAT(tracker_, DetachedCountsMatch(1, 0));
    v8destroy();
    EXPECT_THAT(tracker_, CountsMatch(1, 1));
    EXPECT_THAT(tracker_, DetachedCountsMatch(0, 0));
    iframeattach();
    EXPECT_THAT(tracker_, CountsMatch(1, 2));
    EXPECT_THAT(tracker_, DetachedCountsMatch(0, 0));
    iframedetach();
    EXPECT_THAT(tracker_, CountsMatch(1, 1));
    EXPECT_THAT(tracker_, DetachedCountsMatch(0, 0));
  }

  {
    SCOPED_TRACE("Case 2");
    v8create();
    EXPECT_THAT(tracker_, CountsMatch(2, 2));
    EXPECT_THAT(tracker_, DetachedCountsMatch(0, 0));
    v8detach();
    EXPECT_THAT(tracker_, CountsMatch(2, 2));
    EXPECT_THAT(tracker_, DetachedCountsMatch(1, 0));
    iframeattach();
    EXPECT_THAT(tracker_, CountsMatch(2, 2));
    EXPECT_THAT(tracker_, DetachedCountsMatch(1, 0));
    v8destroy();
    EXPECT_THAT(tracker_, CountsMatch(1, 2));
    EXPECT_THAT(tracker_, DetachedCountsMatch(0, 0));
    iframedetach();
    EXPECT_THAT(tracker_, CountsMatch(1, 1));
    EXPECT_THAT(tracker_, DetachedCountsMatch(0, 0));
  }

  {
    SCOPED_TRACE("Case 3");
    v8create();
    EXPECT_THAT(tracker_, CountsMatch(2, 2));
    EXPECT_THAT(tracker_, DetachedCountsMatch(0, 0));
    v8detach();
    EXPECT_THAT(tracker_, CountsMatch(2, 2));
    EXPECT_THAT(tracker_, DetachedCountsMatch(1, 0));
    iframeattach();
    EXPECT_THAT(tracker_, CountsMatch(2, 2));
    EXPECT_THAT(tracker_, DetachedCountsMatch(1, 0));
    iframedetach();
    EXPECT_THAT(tracker_, CountsMatch(2, 2));
    EXPECT_THAT(tracker_, DetachedCountsMatch(1, 0));
    v8destroy();
    EXPECT_THAT(tracker_, CountsMatch(1, 1));
    EXPECT_THAT(tracker_, DetachedCountsMatch(0, 0));
  }

  {
    SCOPED_TRACE("Case 4");
    v8create();
    EXPECT_THAT(tracker_, CountsMatch(2, 2));
    EXPECT_THAT(tracker_, DetachedCountsMatch(0, 0));
    iframeattach();
    EXPECT_THAT(tracker_, CountsMatch(2, 2));
    EXPECT_THAT(tracker_, DetachedCountsMatch(0, 0));
    v8detach();
    EXPECT_THAT(tracker_, CountsMatch(2, 2));
    EXPECT_THAT(tracker_, DetachedCountsMatch(1, 0));
    v8destroy();
    EXPECT_THAT(tracker_, CountsMatch(1, 2));
    EXPECT_THAT(tracker_, DetachedCountsMatch(0, 0));
    iframedetach();
    EXPECT_THAT(tracker_, CountsMatch(1, 1));
    EXPECT_THAT(tracker_, DetachedCountsMatch(0, 0));
  }

  {
    SCOPED_TRACE("Case 5");
    v8create();
    EXPECT_THAT(tracker_, CountsMatch(2, 2));
    EXPECT_THAT(tracker_, DetachedCountsMatch(0, 0));
    iframeattach();
    EXPECT_THAT(tracker_, CountsMatch(2, 2));
    EXPECT_THAT(tracker_, DetachedCountsMatch(0, 0));
    v8detach();
    EXPECT_THAT(tracker_, CountsMatch(2, 2));
    EXPECT_THAT(tracker_, DetachedCountsMatch(1, 0));
    iframedetach();
    EXPECT_THAT(tracker_, CountsMatch(2, 2));
    EXPECT_THAT(tracker_, DetachedCountsMatch(1, 0));
    v8destroy();
    EXPECT_THAT(tracker_, CountsMatch(1, 1));
    EXPECT_THAT(tracker_, DetachedCountsMatch(0, 0));
  }

  {
    SCOPED_TRACE("Case 6");
    v8create();
    EXPECT_THAT(tracker_, CountsMatch(2, 2));
    EXPECT_THAT(tracker_, DetachedCountsMatch(0, 0));
    iframeattach();
    EXPECT_THAT(tracker_, CountsMatch(2, 2));
    EXPECT_THAT(tracker_, DetachedCountsMatch(0, 0));
    iframedetach();
    EXPECT_THAT(tracker_, CountsMatch(2, 2));
    EXPECT_THAT(tracker_, DetachedCountsMatch(0, 0));
    v8detach();
    EXPECT_THAT(tracker_, CountsMatch(2, 2));
    EXPECT_THAT(tracker_, DetachedCountsMatch(1, 0));
    v8destroy();
    EXPECT_THAT(tracker_, CountsMatch(1, 1));
    EXPECT_THAT(tracker_, DetachedCountsMatch(0, 0));
  }

  {
    SCOPED_TRACE("Case 7");
    iframeattach();
    EXPECT_THAT(tracker_, CountsMatch(1, 2));
    EXPECT_THAT(tracker_, DetachedCountsMatch(0, 0));
    v8create();
    EXPECT_THAT(tracker_, CountsMatch(2, 2));
    EXPECT_THAT(tracker_, DetachedCountsMatch(0, 0));
    v8detach();
    EXPECT_THAT(tracker_, CountsMatch(2, 2));
    EXPECT_THAT(tracker_, DetachedCountsMatch(1, 0));
    v8destroy();
    EXPECT_THAT(tracker_, CountsMatch(1, 2));
    EXPECT_THAT(tracker_, DetachedCountsMatch(0, 0));
    iframedetach();
    EXPECT_THAT(tracker_, CountsMatch(1, 1));
    EXPECT_THAT(tracker_, DetachedCountsMatch(0, 0));
  }

  {
    SCOPED_TRACE("Case 8");
    iframeattach();
    EXPECT_THAT(tracker_, CountsMatch(1, 2));
    EXPECT_THAT(tracker_, DetachedCountsMatch(0, 0));
    v8create();
    EXPECT_THAT(tracker_, CountsMatch(2, 2));
    EXPECT_THAT(tracker_, DetachedCountsMatch(0, 0));
    v8detach();
    EXPECT_THAT(tracker_, CountsMatch(2, 2));
    EXPECT_THAT(tracker_, DetachedCountsMatch(1, 0));
    iframedetach();
    EXPECT_THAT(tracker_, CountsMatch(2, 2));
    EXPECT_THAT(tracker_, DetachedCountsMatch(1, 0));
    v8destroy();
    EXPECT_THAT(tracker_, CountsMatch(1, 1));
    EXPECT_THAT(tracker_, DetachedCountsMatch(0, 0));
  }

  {
    SCOPED_TRACE("Case 9");
    iframeattach();
    EXPECT_THAT(tracker_, CountsMatch(1, 2));
    EXPECT_THAT(tracker_, DetachedCountsMatch(0, 0));
    v8create();
    EXPECT_THAT(tracker_, CountsMatch(2, 2));
    EXPECT_THAT(tracker_, DetachedCountsMatch(0, 0));
    iframedetach();
    EXPECT_THAT(tracker_, CountsMatch(2, 2));
    EXPECT_THAT(tracker_, DetachedCountsMatch(0, 0));
    v8detach();
    EXPECT_THAT(tracker_, CountsMatch(2, 2));
    EXPECT_THAT(tracker_, DetachedCountsMatch(1, 0));
    v8destroy();
    EXPECT_THAT(tracker_, CountsMatch(1, 1));
    EXPECT_THAT(tracker_, DetachedCountsMatch(0, 0));
  }

  {
    SCOPED_TRACE("Case 10");
    iframeattach();
    EXPECT_THAT(tracker_, CountsMatch(1, 2));
    EXPECT_THAT(tracker_, DetachedCountsMatch(0, 0));
    iframedetach();
    EXPECT_THAT(tracker_, CountsMatch(1, 1));
    EXPECT_THAT(tracker_, DetachedCountsMatch(0, 0));
    v8create();
    EXPECT_THAT(tracker_, CountsMatch(2, 2));
    EXPECT_THAT(tracker_, DetachedCountsMatch(0, 0));
    v8detach();
    EXPECT_THAT(tracker_, CountsMatch(2, 2));
    EXPECT_THAT(tracker_, DetachedCountsMatch(1, 0));
    v8destroy();
    EXPECT_THAT(tracker_, CountsMatch(1, 1));
    EXPECT_THAT(tracker_, DetachedCountsMatch(0, 0));
  }
}

TEST_F(V8ContextTrackerTest, PublicApi) {
  EXPECT_THAT(tracker_, CountsMatch(0, 0));
  EXPECT_THAT(tracker_, DetachedCountsMatch(0, 0));

  // Create a main frame.

  EXPECT_FALSE(tracker_->GetV8ContextState(kFrameMainWorld));
  EXPECT_FALSE(
      tracker_->GetExecutionContextState(mock_graph_->frame->GetFrameToken()));

  tracker_->OnV8ContextCreated(
      ProcessNodeImpl::CreatePassKeyForTesting(), mock_graph_->process.get(),
      mojom::V8ContextDescription(
          /* token */ kFrameMainWorld,
          /* world_type */ mojom::V8ContextWorldType::kMain,
          /* world_name */ std::nullopt,
          /* execution_context_token */ mock_graph_->frame->GetFrameToken()),
      /* iframe_attribution_data */ nullptr);
  EXPECT_THAT(tracker_, CountsMatch(1, 1));
  EXPECT_THAT(tracker_, DetachedCountsMatch(0, 0));

  const auto* v8_state = tracker_->GetV8ContextState(kFrameMainWorld);
  ASSERT_TRUE(v8_state);
  EXPECT_EQ(kFrameMainWorld, v8_state->description.token);
  EXPECT_EQ(mojom::V8ContextWorldType::kMain, v8_state->description.world_type);
  EXPECT_FALSE(v8_state->description.world_name);
  ASSERT_TRUE(v8_state->description.execution_context_token);
  EXPECT_EQ(blink::ExecutionContextToken(mock_graph_->frame->GetFrameToken()),
            v8_state->description.execution_context_token.value());
  const auto* ec_state =
      tracker_->GetExecutionContextState(mock_graph_->frame->GetFrameToken());
  ASSERT_TRUE(ec_state);
  EXPECT_EQ(blink::ExecutionContextToken(mock_graph_->frame->GetFrameToken()),
            ec_state->token);

  // Create a child frame.

  ASSERT_FALSE(tracker_->GetV8ContextState(kChildFrameMainWorld));
  ASSERT_FALSE(tracker_->GetExecutionContextState(
      mock_graph_->child_frame->GetFrameToken()));

  tracker_->OnV8ContextCreated(
      ProcessNodeImpl::CreatePassKeyForTesting(),
      mock_graph_->other_process.get(),
      mojom::V8ContextDescription(
          /* token */ kChildFrameMainWorld,
          /* world_type */ mojom::V8ContextWorldType::kMain,
          /* world_name */ std::nullopt,
          /* execution_context_token */
          mock_graph_->child_frame->GetFrameToken()),
      /* iframe_attribution_data */ nullptr);
  v8_state = tracker_->GetV8ContextState(kChildFrameMainWorld);
  ASSERT_TRUE(v8_state);
  EXPECT_EQ(kChildFrameMainWorld, v8_state->description.token);
  EXPECT_EQ(mojom::V8ContextWorldType::kMain, v8_state->description.world_type);
  EXPECT_FALSE(v8_state->description.world_name);
  ASSERT_TRUE(v8_state->description.execution_context_token);
  EXPECT_EQ(
      blink::ExecutionContextToken(mock_graph_->child_frame->GetFrameToken()),
      v8_state->description.execution_context_token.value());
  ec_state = tracker_->GetExecutionContextState(
      mock_graph_->child_frame->GetFrameToken());
  ASSERT_TRUE(ec_state);
  EXPECT_EQ(
      blink::ExecutionContextToken(mock_graph_->child_frame->GetFrameToken()),
      ec_state->token);

  // Provide iframe data for the child frame.

  ASSERT_FALSE(ec_state->iframe_attribution_data);
  tracker_->OnRemoteIframeAttachedForTesting(
      mock_graph_->child_frame.get(), mock_graph_->frame.get(),
      kChildFrameRemoteToken, GetFakeIframeAttributionDataPtr());

  const auto& iad = ec_state->iframe_attribution_data;
  ASSERT_TRUE(iad);
  EXPECT_EQ(base::OptionalFromPtr(&kIframeId), iad->id);
  EXPECT_EQ(base::OptionalFromPtr(&kIframeSrc), iad->src);
}

}  // namespace v8_memory
}  // namespace performance_manager
