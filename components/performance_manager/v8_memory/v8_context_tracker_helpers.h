// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_V8_MEMORY_V8_CONTEXT_TRACKER_HELPERS_H_
#define COMPONENTS_PERFORMANCE_MANAGER_V8_MEMORY_V8_CONTEXT_TRACKER_HELPERS_H_

#include <optional>
#include <string>

#include "third_party/blink/public/common/tokens/tokens.h"

namespace performance_manager {

class FrameNode;
class Graph;

namespace execution_context {
class ExecutionContext;
}  // namespace execution_context

namespace mojom {
class V8ContextDescription;
}  // namespace mojom

namespace v8_memory {

// Determines if the provided frame has a cross-process parent frame.
[[nodiscard]] bool HasCrossProcessParent(const FrameNode* frame_node);

// Determines if a string is a valid extension ID.
// TODO(crbug.com/40136290): The extension ID should be strongly typed, with
// built-in validation, mojo type-mapping, etc. Ideally this would be done
// directly in extensions/common/extension_id.h.
[[nodiscard]] bool IsValidExtensionId(const std::string& s);

// Returns true if an ExecutionContextToken corresponds to a worklet.
[[nodiscard]] bool IsWorkletToken(const blink::ExecutionContextToken& token);

// Returns true if an ExecutionContextToken corresponds to a worker.
[[nodiscard]] bool IsWorkerToken(const blink::ExecutionContextToken& token);

// Looks up the execution context corresponding to the given token. Note that
// the ExecutionContextRegistry must be installed on the graph.
[[nodiscard]] const execution_context::ExecutionContext* GetExecutionContext(
    const blink::ExecutionContextToken& token,
    Graph* graph);

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
  kMissingShadowRealmToken,
};

// Validates the given V8ContextDescription.
[[nodiscard]] V8ContextDescriptionStatus ValidateV8ContextDescription(
    const mojom::V8ContextDescription& description);

// Determines whether or not IframeAttributionData is expected to accompany the
// provided V8ContextDescription. This is not always able to be determined, in
// which case std::nullopt will be returned. It is assumed that the
// |description| has previously been validated.
[[nodiscard]] std::optional<bool>
ExpectIframeAttributionDataForV8ContextDescription(
    const mojom::V8ContextDescription& description,
    Graph* graph);

// Small helper class for maintaining a count of objects that are optionally
// "marked".
class MarkedObjectCount {
 public:
  MarkedObjectCount() = default;
  MarkedObjectCount(const MarkedObjectCount&) = delete;
  MarkedObjectCount& operator=(const MarkedObjectCount&) = delete;
  ~MarkedObjectCount() = default;

  size_t count() const { return count_; }
  size_t marked_count() const { return marked_count_; }

  void Increment() { ++count_; }
  void Mark();
  void Decrement(bool marked);

 private:
  size_t marked_count_ = 0;
  size_t count_ = 0;
};

// Helper class for maintaining a pair of context counts for both
// ExecutionContexts and V8Contexts.
class ContextCounts {
 public:
  ContextCounts() = default;
  ContextCounts(const ContextCounts&) = delete;
  ContextCounts& operator=(const ContextCounts&) = delete;
  ~ContextCounts() = default;

  size_t GetExecutionContextDataCount() const { return ec_count_.count(); }
  size_t GetDestroyedExecutionContextDataCount() const {
    return ec_count_.marked_count();
  }
  void IncrementExecutionContextDataCount() { ec_count_.Increment(); }
  void MarkExecutionContextDataDestroyed() { ec_count_.Mark(); }
  void DecrementExecutionContextDataCount(bool destroyed) {
    ec_count_.Decrement(destroyed);
  }

  size_t GetV8ContextDataCount() const { return v8_count_.count(); }
  size_t GetDetachedV8ContextDataCount() const {
    return v8_count_.marked_count();
  }
  void IncrementV8ContextDataCount() { v8_count_.Increment(); }
  void MarkV8ContextDataDetached() { v8_count_.Mark(); }
  void DecrementV8ContextDataCount(bool detached) {
    v8_count_.Decrement(detached);
  }

 private:
  MarkedObjectCount ec_count_;
  MarkedObjectCount v8_count_;
};

}  // namespace v8_memory
}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_V8_MEMORY_V8_CONTEXT_TRACKER_HELPERS_H_
