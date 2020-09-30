// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/v8_memory/v8_context_tracker_helpers.h"

#include "base/check.h"
#include "base/notreached.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/public/execution_context/execution_context.h"
#include "components/performance_manager/public/execution_context/execution_context_registry.h"
#include "components/performance_manager/v8_memory/v8_context_tracker_types.h"

namespace performance_manager {
namespace v8_memory {

namespace {

bool IsSynchronousIframeAttributionDataExpected(
    const execution_context::ExecutionContext* ec) {
  DCHECK(ec);
  auto* frame = ec->GetFrameNode();
  if (!frame)
    return false;
  if (frame->IsMainFrame())
    return false;
  // Iframe data is expected if this node is in the same process as its
  // parent.
  return frame->GetProcessNode() ==
         frame->GetParentFrameNode()->GetProcessNode();
}

}  // namespace

blink::ExecutionContextToken ToExecutionContextToken(
    const blink::WorkerToken& token) {
  if (token.Is<blink::DedicatedWorkerToken>()) {
    return blink::ExecutionContextToken(
        token.GetAs<blink::DedicatedWorkerToken>());
  }
  if (token.Is<blink::ServiceWorkerToken>()) {
    return blink::ExecutionContextToken(
        token.GetAs<blink::ServiceWorkerToken>());
  }
  // This will DCHECK for us if the token isn't a SharedWorkerToken.
  return blink::ExecutionContextToken(token.GetAs<blink::SharedWorkerToken>());
}

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

const execution_context::ExecutionContext* GetExecutionContext(
    const blink::ExecutionContextToken& token,
    Graph* graph) {
  auto* registry =
      execution_context::ExecutionContextRegistry::GetFromGraph(graph);
  DCHECK(registry);
  return registry->GetExecutionContextByToken(token);
}

V8ContextDescriptionStatus ValidateV8ContextDescription(
    const V8ContextDescription& description) {
  switch (description.world_type) {
    case V8ContextWorldType::kMain: {
      if (description.world_name)
        return V8ContextDescriptionStatus::kUnexpectedWorldName;
      if (!description.execution_context_token)
        return V8ContextDescriptionStatus::kMissingExecutionContextToken;
      if (!description.execution_context_token->Is<blink::LocalFrameToken>())
        return V8ContextDescriptionStatus::kMissingLocalFrameToken;
    } break;

    case V8ContextWorldType::kWorkerOrWorklet: {
      if (description.world_name)
        return V8ContextDescriptionStatus::kUnexpectedWorldName;
      if (!description.execution_context_token)
        return V8ContextDescriptionStatus::kMissingExecutionContextToken;
      if (description.execution_context_token->Is<blink::LocalFrameToken>())
        return V8ContextDescriptionStatus::kUnexpectedLocalFrameToken;
    } break;

    case V8ContextWorldType::kExtension: {
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

    case V8ContextWorldType::kIsolated: {
      // World names are optional in isolated worlds.
      // Only frame and workers can have isolated worlds, *not* worklets.
      if (!description.execution_context_token)
        return V8ContextDescriptionStatus::kMissingExecutionContextToken;
      if (IsWorkletToken(description.execution_context_token.value()))
        return V8ContextDescriptionStatus::kUnexpectedWorkletToken;
    } break;

    case V8ContextWorldType::kInspector: {
      if (description.world_name)
        return V8ContextDescriptionStatus::kUnexpectedWorldName;
      // Devtools can only inject into frames and workers, *not* worklets.
      if (!description.execution_context_token)
        return V8ContextDescriptionStatus::kMissingExecutionContextToken;
      if (IsWorkletToken(description.execution_context_token.value()))
        return V8ContextDescriptionStatus::kUnexpectedWorkletToken;
    } break;

    case V8ContextWorldType::kRegExp: {
      // Regexp worlds have no additional data.
      if (description.world_name)
        return V8ContextDescriptionStatus::kUnexpectedWorldName;
      if (description.execution_context_token)
        return V8ContextDescriptionStatus::kUnexpectedExecutionContextToken;
    } break;
  }

  return V8ContextDescriptionStatus::kValid;
}

base::Optional<bool> ExpectIframeAttributionDataForV8ContextDescription(
    const V8ContextDescription& description,
    Graph* graph) {
  switch (description.world_type) {
    case V8ContextWorldType::kMain: {
      // There's no guarantee that the actual ExecutionContext has yet been
      // created from our POV as there's a race between V8Context creation
      // notifications and node creations. But if it does exist, we sanity check
      // that we should in fact be receiving iframe data for this frame.
      if (auto* ec = GetExecutionContext(*description.execution_context_token,
                                         graph)) {
        return IsSynchronousIframeAttributionDataExpected(ec);
      } else {
        // Unable to be determined.
        return base::nullopt;
      }
    } break;

    case V8ContextWorldType::kWorkerOrWorklet:
    case V8ContextWorldType::kExtension:
    case V8ContextWorldType::kIsolated:
    case V8ContextWorldType::kInspector:
    case V8ContextWorldType::kRegExp: {
    } break;
  }

  return false;
}

}  // namespace v8_memory
}  // namespace performance_manager
