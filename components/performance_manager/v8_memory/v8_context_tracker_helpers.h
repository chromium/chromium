// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_V8_MEMORY_V8_CONTEXT_TRACKER_HELPERS_H_
#define COMPONENTS_PERFORMANCE_MANAGER_V8_MEMORY_V8_CONTEXT_TRACKER_HELPERS_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/optional.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace performance_manager {

class FrameNode;
class Graph;

namespace execution_context {
class ExecutionContext;
}  // namespace execution_context

namespace v8_memory {

struct V8ContextDescription;

// Helper function to convert a WorkerToken to an ExecutionContext token.
// TODO(crbug.com/1126285): There should be automatic type conversion for this
// added to MultiToken<>.
blink::ExecutionContextToken ToExecutionContextToken(
    const blink::WorkerToken& token) WARN_UNUSED_RESULT;

// Determines if the provided frame has a cross-process parent frame.
bool HasCrossProcessParent(const FrameNode* frame_node) WARN_UNUSED_RESULT;

// Determines if a string is a valid extension ID.
// TODO(crbug.com/1096617): The extension ID should be strongly typed, with
// built-in validation, mojo type-mapping, etc. Ideally this would be done
// directly in extensions/common/extension_id.h.
bool IsValidExtensionId(const std::string& s) WARN_UNUSED_RESULT;

// Returns true if an ExecutionContextToken corresponds to a worklet.
bool IsWorkletToken(const blink::ExecutionContextToken& token)
    WARN_UNUSED_RESULT;

// Looks up the execution context corresponding to the given token. Note that
// the ExecutionContextRegistry must be installed on the graph.
const execution_context::ExecutionContext* GetExecutionContext(
    const blink::ExecutionContextToken& token,
    Graph* graph) WARN_UNUSED_RESULT;

// Return type for V8ContextDescription validation.
enum class V8ContextDescriptionStatus {
  kValid,

  // World name errors.
  kMissingWorldName,
  kUnexpectedWorldName,
  kInvalidExtensionWorldName,

  // ExecutionContextToken errors.
  kMissingExecutionContextToken,
  kUnexpectedExecutionContextToken,
  kMissingLocalFrameToken,
  kUnexpectedLocalFrameToken,
  kUnexpectedWorkletToken,
};

// Validates the given V8ContextDescription.
V8ContextDescriptionStatus ValidateV8ContextDescription(
    const V8ContextDescription& description) WARN_UNUSED_RESULT;

// Determines whether or not IframeAttributionData is expected to accompany the
// provided V8ContextDescription. This is not always able to be determined, in
// which case base::nullopt will be returned. It is assumed that the
// |description| has previously been validated.
base::Optional<bool> ExpectIframeAttributionDataForV8ContextDescription(
    const V8ContextDescription& description,
    Graph* graph) WARN_UNUSED_RESULT;

}  // namespace v8_memory
}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_V8_MEMORY_V8_CONTEXT_TRACKER_HELPERS_H_
