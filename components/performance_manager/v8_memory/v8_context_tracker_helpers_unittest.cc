// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/v8_memory/v8_context_tracker_helpers.h"

#include "base/memory/raw_ptr.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/worker_node_impl.h"
#include "components/performance_manager/public/execution_context/execution_context.h"
#include "components/performance_manager/public/execution_context/execution_context_registry.h"
#include "components/performance_manager/public/mojom/v8_contexts.mojom.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/mock_graphs.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {
namespace v8_memory {

namespace {

constexpr char kValidExtensionWorldName[] = "abcdefghijklmnopabcdefghijklmnop";
constexpr char kInvalidExtensionWorldName[] = "BADEXTENSIONNAME";
constexpr char kWorldName[] = "worldname";

class V8ContextTrackerHelpersTest : public GraphTestHarness {
 public:
  V8ContextTrackerHelpersTest() = default;

  ~V8ContextTrackerHelpersTest() override = default;

  void OnGraphCreated(GraphImpl* graph_impl) override {
    registry =
        execution_context::ExecutionContextRegistry::GetFromGraph(graph());
    mock_graph =
        std::make_unique<MockSinglePageWithMultipleProcessesGraph>(graph());
  }

  raw_ptr<execution_context::ExecutionContextRegistry> registry = nullptr;
  std::unique_ptr<MockSinglePageWithMultipleProcessesGraph> mock_graph;
  mojom::IframeAttributionData fake_iframe_attribution_data;
};

}  // namespace

TEST_F(V8ContextTrackerHelpersTest, HasCrossProcessParent) {
  // Fails for a main-frame.
  EXPECT_FALSE(HasCrossProcessParent(mock_graph->frame.get()));

  // Returns true for an actual cross-process child frame.
  EXPECT_TRUE(HasCrossProcessParent(mock_graph->child_frame.get()));

  // Fails for a same-process child frame.
  TestNodeWrapper<FrameNodeImpl> child_frame(graph()->CreateFrameNodeAutoId(
      mock_graph->process.get(), mock_graph->page.get(),
      mock_graph->frame.get()));
  EXPECT_FALSE(HasCrossProcessParent(child_frame.get()));
}

TEST_F(V8ContextTrackerHelpersTest, IsValidExtensionId) {
  EXPECT_TRUE(IsValidExtensionId("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
  EXPECT_FALSE(IsValidExtensionId("Aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
  EXPECT_FALSE(IsValidExtensionId("qaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
  EXPECT_FALSE(IsValidExtensionId("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
  EXPECT_FALSE(IsValidExtensionId(""));
}

TEST_F(V8ContextTrackerHelpersTest, IsWorkletToken) {
  blink::LocalFrameToken local_frame;
  blink::DedicatedWorkerToken dedicated;
  blink::ServiceWorkerToken service;
  blink::SharedWorkerToken shared;
  blink::AnimationWorkletToken animation;
  blink::AudioWorkletToken audio;
  blink::LayoutWorkletToken layout;
  blink::PaintWorkletToken paint;
  EXPECT_FALSE(IsWorkletToken(blink::ExecutionContextToken(local_frame)));
  EXPECT_FALSE(IsWorkletToken(blink::ExecutionContextToken(dedicated)));
  EXPECT_FALSE(IsWorkletToken(blink::ExecutionContextToken(service)));
  EXPECT_FALSE(IsWorkletToken(blink::ExecutionContextToken(shared)));
  EXPECT_TRUE(IsWorkletToken(blink::ExecutionContextToken(animation)));
  EXPECT_TRUE(IsWorkletToken(blink::ExecutionContextToken(audio)));
  EXPECT_TRUE(IsWorkletToken(blink::ExecutionContextToken(layout)));
  EXPECT_TRUE(IsWorkletToken(blink::ExecutionContextToken(paint)));
}

TEST_F(V8ContextTrackerHelpersTest, GetExecutionContext) {
  FrameNode* frame_node = mock_graph->frame.get();
  auto* execution_context =
      GetExecutionContext(frame_node->GetFrameToken(), graph());
  ASSERT_TRUE(execution_context);
  EXPECT_EQ(frame_node, execution_context->GetFrameNode());
}

TEST_F(V8ContextTrackerHelpersTest, ValidateV8ContextDescriptionMainWorld) {
  TestNodeWrapper<FrameNodeImpl> child_frame(graph()->CreateFrameNodeAutoId(
      mock_graph->process.get(), mock_graph->page.get(),
      mock_graph->frame.get()));

  // A valid description of a main frame.
  auto desc = mojom::V8ContextDescription(
      blink::V8ContextToken(), mojom::V8ContextWorldType::kMain,
      /* world_name */ std::nullopt, mock_graph->frame->GetFrameToken());
  EXPECT_EQ(V8ContextDescriptionStatus::kValid,
            ValidateV8ContextDescription(desc));
  EXPECT_EQ(false,
            ExpectIframeAttributionDataForV8ContextDescription(desc, graph()));

  // A valid description of a cross-process child frame.
  desc = mojom::V8ContextDescription(
      blink::V8ContextToken(), mojom::V8ContextWorldType::kMain,
      /* world_name */ std::nullopt, mock_graph->child_frame->GetFrameToken());
  EXPECT_EQ(V8ContextDescriptionStatus::kValid,
            ValidateV8ContextDescription(desc));
  EXPECT_EQ(false,
            ExpectIframeAttributionDataForV8ContextDescription(desc, graph()));

  // A valid description of a same-process child frame.
  desc = mojom::V8ContextDescription(
      blink::V8ContextToken(), mojom::V8ContextWorldType::kMain,
      /* world_name */ std::nullopt, child_frame->GetFrameToken());
  EXPECT_EQ(V8ContextDescriptionStatus::kValid,
            ValidateV8ContextDescription(desc));
  EXPECT_EQ(true,
            ExpectIframeAttributionDataForV8ContextDescription(desc, graph()));

  // A valid description of a frame, but one that doesn't have a corresponding
  // entry in the graph. In this case its impossible to determine if
  // IframeAttributionData should accompany the V8ContextDescription.
  desc = mojom::V8ContextDescription(
      blink::V8ContextToken(), mojom::V8ContextWorldType::kMain,
      /* world_name */ std::nullopt, blink::LocalFrameToken());
  EXPECT_EQ(std::nullopt,
            ExpectIframeAttributionDataForV8ContextDescription(desc, graph()));

  // A main-world should not have a world name.
  EXPECT_EQ(V8ContextDescriptionStatus::kUnexpectedWorldName,
            ValidateV8ContextDescription(mojom::V8ContextDescription(
                blink::V8ContextToken(), mojom::V8ContextWorldType::kMain,
                kWorldName, mock_graph->frame->GetFrameToken())));

  // A main world must have an |execution_context_token|.
  EXPECT_EQ(V8ContextDescriptionStatus::kMissingExecutionContextToken,
            ValidateV8ContextDescription(mojom::V8ContextDescription(
                blink::V8ContextToken(), mojom::V8ContextWorldType::kMain,
                /* world_name */ std::nullopt,
                /* execution_context_token */ std::nullopt)));

  // A main world must have an blink::LocalFrameToken.
  blink::ExecutionContextToken worker_token((blink::SharedWorkerToken()));
  EXPECT_EQ(V8ContextDescriptionStatus::kMissingLocalFrameToken,
            ValidateV8ContextDescription(mojom::V8ContextDescription(
                blink::V8ContextToken(), mojom::V8ContextWorldType::kMain,
                /* world_name */ std::nullopt, worker_token)));
}

TEST_F(V8ContextTrackerHelpersTest, ValidateV8ContextDescriptionWorkerWorld) {
  blink::DedicatedWorkerToken worker_token;
  auto worker = TestNodeWrapper<WorkerNodeImpl>::Create(
      graph(), WorkerNode::WorkerType::kDedicated, mock_graph->process.get(),
      "browser_context", worker_token);

  // A valid worker description.
  auto desc = mojom::V8ContextDescription(
      blink::V8ContextToken(), mojom::V8ContextWorldType::kWorkerOrWorklet,
      /* world_name */ std::nullopt, worker_token);
  EXPECT_EQ(V8ContextDescriptionStatus::kValid,
            ValidateV8ContextDescription(desc));
  EXPECT_EQ(false,
            ExpectIframeAttributionDataForV8ContextDescription(desc, graph()));

  // A worker should not have a world name.
  EXPECT_EQ(
      V8ContextDescriptionStatus::kUnexpectedWorldName,
      ValidateV8ContextDescription(mojom::V8ContextDescription(
          blink::V8ContextToken(), mojom::V8ContextWorldType::kWorkerOrWorklet,
          kWorldName, worker_token)));

  // A worker must have an |execution_context_token|.
  EXPECT_EQ(
      V8ContextDescriptionStatus::kMissingExecutionContextToken,
      ValidateV8ContextDescription(mojom::V8ContextDescription(
          blink::V8ContextToken(), mojom::V8ContextWorldType::kWorkerOrWorklet,
          /* world_name */ std::nullopt,
          /* execution_context_token */ std::nullopt)));

  // A worker must have a valid worker token, not a LocalFrameToken.
  EXPECT_EQ(
      V8ContextDescriptionStatus::kUnexpectedLocalFrameToken,
      ValidateV8ContextDescription(mojom::V8ContextDescription(
          blink::V8ContextToken(), mojom::V8ContextWorldType::kWorkerOrWorklet,
          /* world_name */ std::nullopt, blink::LocalFrameToken())));
}

TEST_F(V8ContextTrackerHelpersTest,
       ValidateV8ContextDescriptionShadowRealmWorld) {
  // A valid shadow realm description.
  blink::ShadowRealmToken shadow_realm_token;
  auto desc = mojom::V8ContextDescription(
      blink::V8ContextToken(), mojom::V8ContextWorldType::kShadowRealm,
      /* world_name */ std::nullopt, shadow_realm_token);
  EXPECT_EQ(V8ContextDescriptionStatus::kValid,
            ValidateV8ContextDescription(desc));
  EXPECT_EQ(false,
            ExpectIframeAttributionDataForV8ContextDescription(desc, graph()));

  // A shadow realm should not have a world name.
  EXPECT_EQ(
      V8ContextDescriptionStatus::kUnexpectedWorldName,
      ValidateV8ContextDescription(mojom::V8ContextDescription(
          blink::V8ContextToken(), mojom::V8ContextWorldType::kShadowRealm,
          kWorldName, shadow_realm_token)));

  // A shadow realm must have an |execution_context_token|.
  EXPECT_EQ(
      V8ContextDescriptionStatus::kMissingExecutionContextToken,
      ValidateV8ContextDescription(mojom::V8ContextDescription(
          blink::V8ContextToken(), mojom::V8ContextWorldType::kShadowRealm,
          /* world_name */ std::nullopt,
          /* execution_context_token */ std::nullopt)));

  // A shadow realm must have a valid shadow realm token.
  EXPECT_EQ(
      V8ContextDescriptionStatus::kMissingShadowRealmToken,
      ValidateV8ContextDescription(mojom::V8ContextDescription(
          blink::V8ContextToken(), mojom::V8ContextWorldType::kShadowRealm,
          /* world_name */ std::nullopt, blink::LocalFrameToken())));
}

TEST_F(V8ContextTrackerHelpersTest,
       ValidateV8ContextDescriptionExtensionWorld) {
  // A valid extension description.
  auto desc = mojom::V8ContextDescription(
      blink::V8ContextToken(), mojom::V8ContextWorldType::kExtension,
      kValidExtensionWorldName, mock_graph->frame->GetFrameToken());
  EXPECT_EQ(V8ContextDescriptionStatus::kValid,
            ValidateV8ContextDescription(desc));
  EXPECT_EQ(false,
            ExpectIframeAttributionDataForV8ContextDescription(desc, graph()));

  // An extension must have a world name.
  EXPECT_EQ(
      V8ContextDescriptionStatus::kMissingWorldName,
      ValidateV8ContextDescription(mojom::V8ContextDescription(
          blink::V8ContextToken(), mojom::V8ContextWorldType::kExtension,
          /* world_name */ std::nullopt, mock_graph->frame->GetFrameToken())));

  // An invalid extension name should fail.
  EXPECT_EQ(
      V8ContextDescriptionStatus::kInvalidExtensionWorldName,
      ValidateV8ContextDescription(mojom::V8ContextDescription(
          blink::V8ContextToken(), mojom::V8ContextWorldType::kExtension,
          kInvalidExtensionWorldName, mock_graph->frame->GetFrameToken())));

  // An extension must have an |execution_context_token|.
  EXPECT_EQ(V8ContextDescriptionStatus::kMissingExecutionContextToken,
            ValidateV8ContextDescription(mojom::V8ContextDescription(
                blink::V8ContextToken(), mojom::V8ContextWorldType::kExtension,
                kValidExtensionWorldName,
                /* execution_context_token */ std::nullopt)));

  // An extension can't inject into a worklet.
  EXPECT_EQ(V8ContextDescriptionStatus::kUnexpectedWorkletToken,
            ValidateV8ContextDescription(mojom::V8ContextDescription(
                blink::V8ContextToken(), mojom::V8ContextWorldType::kExtension,
                kValidExtensionWorldName, blink::AudioWorkletToken())));
}

TEST_F(V8ContextTrackerHelpersTest, ValidateV8ContextDescriptionIsolatedWorld) {
  // An isolated world may or may not have a |world_name|.
  auto desc = mojom::V8ContextDescription(
      blink::V8ContextToken(), mojom::V8ContextWorldType::kIsolated,
      /* world_name */ std::nullopt, mock_graph->frame->GetFrameToken());
  EXPECT_EQ(V8ContextDescriptionStatus::kValid,
            ValidateV8ContextDescription(desc));
  EXPECT_EQ(false,
            ExpectIframeAttributionDataForV8ContextDescription(desc, graph()));

  desc = mojom::V8ContextDescription(
      blink::V8ContextToken(), mojom::V8ContextWorldType::kIsolated, kWorldName,
      mock_graph->frame->GetFrameToken());
  EXPECT_EQ(V8ContextDescriptionStatus::kValid,
            ValidateV8ContextDescription(desc));
  EXPECT_EQ(false,
            ExpectIframeAttributionDataForV8ContextDescription(desc, graph()));

  // An isolated world must have an |execution_context_token|
  EXPECT_EQ(V8ContextDescriptionStatus::kMissingExecutionContextToken,
            ValidateV8ContextDescription(mojom::V8ContextDescription(
                blink::V8ContextToken(), mojom::V8ContextWorldType::kIsolated,
                /* world_name */ std::nullopt,
                /* execution_context_token */ std::nullopt)));

  // An isolated world can not inject into a worklet.
  EXPECT_EQ(V8ContextDescriptionStatus::kUnexpectedWorkletToken,
            ValidateV8ContextDescription(mojom::V8ContextDescription(
                blink::V8ContextToken(), mojom::V8ContextWorldType::kIsolated,
                /* world_name */ std::nullopt, blink::AudioWorkletToken())));
}

TEST_F(V8ContextTrackerHelpersTest,
       ValidateV8ContextDescriptionInspectorWorld) {
  // A valid inspector world.
  auto desc = mojom::V8ContextDescription(
      blink::V8ContextToken(), mojom::V8ContextWorldType::kInspector,
      /* world_name */ std::nullopt, mock_graph->frame->GetFrameToken());
  EXPECT_EQ(V8ContextDescriptionStatus::kValid,
            ValidateV8ContextDescription(desc));
  EXPECT_EQ(false,
            ExpectIframeAttributionDataForV8ContextDescription(desc, graph()));

  // An inspector world must have an |execution_context_token|
  EXPECT_EQ(V8ContextDescriptionStatus::kMissingExecutionContextToken,
            ValidateV8ContextDescription(mojom::V8ContextDescription(
                blink::V8ContextToken(), mojom::V8ContextWorldType::kInspector,
                /* world_name */ std::nullopt,
                /* execution_context_token */ std::nullopt)));

  // An inspector world can not inject into a worklet.
  EXPECT_EQ(V8ContextDescriptionStatus::kUnexpectedWorkletToken,
            ValidateV8ContextDescription(mojom::V8ContextDescription(
                blink::V8ContextToken(), mojom::V8ContextWorldType::kInspector,
                /* world_name */ std::nullopt, blink::AudioWorkletToken())));
}

TEST_F(V8ContextTrackerHelpersTest, ValidateV8ContextDescriptionRegExpWorld) {
  // A valid regexp world.
  auto desc = mojom::V8ContextDescription(
      blink::V8ContextToken(), mojom::V8ContextWorldType::kRegExp,
      /* world_name */ std::nullopt,
      /* execution_context_token */ std::nullopt);
  EXPECT_EQ(V8ContextDescriptionStatus::kValid,
            ValidateV8ContextDescription(desc));
  EXPECT_EQ(false,
            ExpectIframeAttributionDataForV8ContextDescription(desc, graph()));

  // A regexp world must not have a |world_name|.
  EXPECT_EQ(V8ContextDescriptionStatus::kUnexpectedWorldName,
            ValidateV8ContextDescription(mojom::V8ContextDescription(
                blink::V8ContextToken(), mojom::V8ContextWorldType::kRegExp,
                kWorldName,
                /* execution_context_token */ std::nullopt)));

  // A regexp world must not have an |execution_context_token|.
  EXPECT_EQ(
      V8ContextDescriptionStatus::kUnexpectedExecutionContextToken,
      ValidateV8ContextDescription(mojom::V8ContextDescription(
          blink::V8ContextToken(), mojom::V8ContextWorldType::kRegExp,
          /* world_name */ std::nullopt, mock_graph->frame->GetFrameToken())));
}

}  // namespace v8_memory
}  // namespace performance_manager
