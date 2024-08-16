// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/v8_memory/v8_context_tracker_helpers.h"

#include "base/check.h"
#include "base/notreached.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/public/execution_context/execution_context.h"
#include "components/performance_manager/public/execution_context/execution_context_registry.h"
#include "components/performance_manager/public/mojom/v8_contexts.mojom.h"

namespace performance_manager {
namespace v8_memory {

namespace {

bool IsSynchronousIframeAttributionDataExpected(
    const execution_context::ExecutionContext* ec) {
  DCHECK(ec);
  auto* frame = ec->GetFrameNode();
  // We only expect iframe data for frames...
  if (!frame)
    return false;
  // ... that aren't main frames (have a parent) ...
  if (frame->IsMainFrame())
    return false;
  auto* parent = frame->GetParentFrameNode();
  DCHECK(parent);
  // ... where the parent is hosted in the same process ...
  if (frame->GetProcessNode() != parent->GetProcessNode())
    return false;
  // ... and where they are both in the same SiteInstanceGroup (implying they
  // are both in the same frame-tree and know directly of each other's
  // LocalFrame rather then communicating via a RemoteFrame and a
  // RenderFrameProxy).
  return frame->GetSiteInstanceGroupId() == parent->GetSiteInstanceGroupId();
}

}  // namespace

bool HasCrossProcessParent(const FrameNode* frame_node) {
  DCHECK(frame_node);
  if (frame_node->IsMainFrame())
    return false;
  const ProcessNode* process = frame_node->GetProcessNode();
  const ProcessNode* parent_process =
      frame_node->GetParentFrameNode()->GetProcessNode();
  return process != parent_process;
}

bool IsValidExtensionId(const std::string& s) {
  // Must be a 32-character string with lowercase letters between a and p,
  // inclusive.
  if (s.size() != 32u)
    return false;
  for (char c : s) {
    if (c < 'a' || c > 'p')
      return false;
  }
  return true;
}

bool IsWorkletToken(const blink::ExecutionContextToken& token) {
  return token.Is<blink::AnimationWorkletToken>() ||
         token.Is<blink::AudioWorkletToken>() ||
         token.Is<blink::LayoutWorkletToken>() ||
         token.Is<blink::PaintWorkletToken>();
}

bool IsWorkerToken(const blink::ExecutionContextToken& token) {
  return token.Is<blink::DedicatedWorkerToken>() ||
         token.Is<blink::ServiceWorkerToken>() ||
         token.Is<blink::SharedWorkerToken>();
}

const execution_context::ExecutionContext* GetExecutionContext(
    const blink::ExecutionContextToken& token,
    Graph* graph) {
  auto* registry =
      execution_context::ExecutionContextRegistry::GetFromGraph(graph);
  DCHECK(registry);
  return registry->GetExecutionContextByToken(token);
}

V8ContextDescriptionStatus ValidateV8ContextDescription(
    const mojom::V8ContextDescription& description) {
  switch (description.world_type) {
    case mojom::V8ContextWorldType::kMain: {
      if (description.world_name)
        return V8ContextDescriptionStatus::kUnexpectedWorldName;
      if (!description.execution_context_token)
        return V8ContextDescriptionStatus::kMissingExecutionContextToken;
      if (!description.execution_context_token->Is<blink::LocalFrameToken>())
        return V8ContextDescriptionStatus::kMissingLocalFrameToken;
    } break;

    case mojom::V8ContextWorldType::kWorkerOrWorklet: {
      if (description.world_name)
        return V8ContextDescriptionStatus::kUnexpectedWorldName;
      if (!description.execution_context_token)
        return V8ContextDescriptionStatus::kMissingExecutionContextToken;
      if (description.execution_context_token->Is<blink::LocalFrameToken>())
        return V8ContextDescriptionStatus::kUnexpectedLocalFrameToken;
    } break;

    case mojom::V8ContextWorldType::kShadowRealm: {
      if (description.world_name)
        return V8ContextDescriptionStatus::kUnexpectedWorldName;
      if (!description.execution_context_token)
        return V8ContextDescriptionStatus::kMissingExecutionContextToken;
      if (!description.execution_context_token->Is<blink::ShadowRealmToken>())
        return V8ContextDescriptionStatus::kMissingShadowRealmToken;
    } break;

    case mojom::V8ContextWorldType::kExtension: {
      if (!description.world_name)
        return V8ContextDescriptionStatus::kMissingWorldName;
      if (!IsValidExtensionId(description.world_name.value()))
        return V8ContextDescriptionStatus::kInvalidExtensionWorldName;
      // Extensions can only inject into frames and workers, *not* worklets.
      if (!description.execution_context_token)
        return V8ContextDescriptionStatus::kMissingExecutionContextToken;
      if (IsWorkletToken(description.execution_context_token.value()))
        return V8ContextDescriptionStatus::kUnexpectedWorkletToken;
    } break;

    case mojom::V8ContextWorldType::kIsolated: {
      // World names are optional in isolated worlds.
      // Only frame and workers can have isolated worlds, *not* worklets.
      if (!description.execution_context_token)
        return V8ContextDescriptionStatus::kMissingExecutionContextToken;
      if (IsWorkletToken(description.execution_context_token.value()))
        return V8ContextDescriptionStatus::kUnexpectedWorkletToken;
    } break;

    case mojom::V8ContextWorldType::kInspector: {
      if (description.world_name)
        return V8ContextDescriptionStatus::kUnexpectedWorldName;
      // Devtools can only inject into frames and workers, *not* worklets.
      if (!description.execution_context_token)
        return V8ContextDescriptionStatus::kMissingExecutionContextToken;
      if (IsWorkletToken(description.execution_context_token.value()))
        return V8ContextDescriptionStatus::kUnexpectedWorkletToken;
    } break;

    case mojom::V8ContextWorldType::kRegExp: {
      // Regexp worlds have no additional data.
      if (description.world_name)
        return V8ContextDescriptionStatus::kUnexpectedWorldName;
      if (description.execution_context_token)
        return V8ContextDescriptionStatus::kUnexpectedExecutionContextToken;
    } break;
  }

  return V8ContextDescriptionStatus::kValid;
}

std::optional<bool> ExpectIframeAttributionDataForV8ContextDescription(
    const mojom::V8ContextDescription& description,
    Graph* graph) {
  switch (description.world_type) {
    case mojom::V8ContextWorldType::kMain: {
      // There's no guarantee that the actual ExecutionContext has yet been
      // created from our POV as there's a race between V8Context creation
      // notifications and node creations. But if it does exist, we sanity check
      // that we should in fact be receiving iframe data for this frame.
      if (auto* ec = GetExecutionContext(*description.execution_context_token,
                                         graph)) {
        return IsSynchronousIframeAttributionDataExpected(ec);
      }
      // Unable to be determined.
      return std::nullopt;
    }

    case mojom::V8ContextWorldType::kWorkerOrWorklet:
    case mojom::V8ContextWorldType::kShadowRealm:
    case mojom::V8ContextWorldType::kExtension:
    case mojom::V8ContextWorldType::kIsolated:
    case mojom::V8ContextWorldType::kInspector:
    case mojom::V8ContextWorldType::kRegExp:
      break;
  }

  return false;
}

void MarkedObjectCount::Mark() {
  DCHECK_LT(marked_count_, count_);
  ++marked_count_;
}

void MarkedObjectCount::Decrement(bool marked) {
  DCHECK_LT(0u, count_);
  if (marked) {
    DCHECK_LT(0u, marked_count_);
    --marked_count_;
  } else {
    DCHECK_LT(marked_count_, count_);
  }
  --count_;
}

}  // namespace v8_memory
}  // namespace performance_manager
