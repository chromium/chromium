// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Internal data structures used by V8ContextTracker. These are only exposed in
// a header for testing. Everything in this header lives in an "internal"
// namespace so as not to pollute the "v8_memory", which houses the actual
// consumer API.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_V8_MEMORY_V8_CONTEXT_TRACKER_INTERNAL_H_
#define COMPONENTS_PERFORMANCE_MANAGER_V8_MEMORY_V8_CONTEXT_TRACKER_INTERNAL_H_

#include <memory>
#include <set>

#include "base/containers/linked_list.h"
#include "base/memory/raw_ptr.h"
#include "base/types/pass_key.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/public/graph/node_attached_data.h"
#include "components/performance_manager/public/mojom/v8_contexts.mojom.h"
#include "components/performance_manager/v8_memory/v8_context_tracker.h"
#include "components/performance_manager/v8_memory/v8_context_tracker_helpers.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace performance_manager {
namespace v8_memory {
namespace internal {

using ExecutionContextState = V8ContextTracker::ExecutionContextState;
using V8ContextState = V8ContextTracker::V8ContextState;

// Forward declarations.
class ExecutionContextData;
class ProcessData;
class RemoteFrameData;
class V8ContextData;
class V8ContextTrackerDataStore;

// A comparator for "Data" objects that compares by token.
template <typename DataType, typename TokenType>
struct TokenComparator {
  using is_transparent = int;
  static const TokenType& GetToken(const TokenType& token) { return token; }
  static const TokenType& GetToken(const std::unique_ptr<DataType>& data) {
    return data->GetToken();
  }
  template <typename Type1, typename Type2>
  bool operator()(const Type1& obj1, const Type2& obj2) const {
    return GetToken(obj1) < GetToken(obj2);
  }
};

////////////////////////////////////////////////////////////////////////////////
// ExecutionContextData declaration:

// Internal wrapper of ExecutionContextState. Augments with additional data
// needed for the implementation. Since these objects also need to be tracked
// per-process, they are kept in a process-associated doubly-linked list.
class ExecutionContextData : public base::LinkNode<ExecutionContextData>,
                             public ExecutionContextState {
 public:
  using Comparator =
      TokenComparator<ExecutionContextData, blink::ExecutionContextToken>;

  ExecutionContextData() = delete;
  ExecutionContextData(const ExecutionContextData&) = delete;
  ExecutionContextData(ProcessData* process_data,
                       const blink::ExecutionContextToken& token,
                       mojom::IframeAttributionDataPtr iframe_attribution_data);
  ExecutionContextData& operator=(const ExecutionContextData&) = delete;
  ~ExecutionContextData() override;

  // Simple accessors.
  ProcessData* process_data() const { return process_data_; }
  RemoteFrameData* remote_frame_data() { return remote_frame_data_; }
  size_t v8_context_count() const { return v8_context_count_; }
  size_t main_nondetached_v8_context_count() const {
    return main_nondetached_v8_context_count_;
  }

  // For consistency, all Data objects have a GetToken() function.
  const blink::ExecutionContextToken& GetToken() const { return token; }

  // Returns true if this object is currently being tracked (it is in
  // ProcessData::execution_context_datas_, and
  // V8ContextTrackerDataStore::global_execution_context_datas_).
  [[nodiscard]] bool IsTracked() const;

  // Returns true if this object *should* be destroyed (there are no references
  // to it keeping it alive).
  [[nodiscard]] bool ShouldDestroy() const;

  // Manages remote frame data associated with this ExecutionContextData.
  void SetRemoteFrameData(base::PassKey<RemoteFrameData>,
                          RemoteFrameData* remote_frame_data);
  [[nodiscard]] bool ClearRemoteFrameData(base::PassKey<RemoteFrameData>);

  // Increments |v8_context_count_|.
  void IncrementV8ContextCount(base::PassKey<V8ContextData>);

  // Decrements |v8_context_count_|, and returns true if the object has
  // transitioned to "ShouldDestroy".
  [[nodiscard]] bool DecrementV8ContextCount(base::PassKey<V8ContextData>);

  // Marks this context as destroyed. Returns true if the state changed, false
  // if it was already destroyed.
  [[nodiscard]] bool MarkDestroyed(base::PassKey<ProcessData>);

  // Used for tracking the total number of non-detached "main" V8Contexts
  // associated with this ExecutionContext. This should always be no more than
  // 1. A new context may become the main context during a same-document
  // navigation of a frame.
  [[nodiscard]] bool MarkMainV8ContextCreated(
      base::PassKey<V8ContextTrackerDataStore>);
  void MarkMainV8ContextDetached(base::PassKey<V8ContextData>);

 private:
  const raw_ptr<ProcessData> process_data_;

  raw_ptr<RemoteFrameData> remote_frame_data_ = nullptr;

  // The count of V8ContextDatas keeping this object alive.
  size_t v8_context_count_ = 0;

  // The number of non-detached main worlds that are currently associated with
  // this ExecutionContext. There can be no more than 1 of these. Once
  // document and frame lifetime semantics have been cleaned up, there will only
  // be a single main context per ExecutionContext over its lifetime; right now
  // there can be multiple due to same-document navigations.
  size_t main_nondetached_v8_context_count_ = 0;
};

////////////////////////////////////////////////////////////////////////////////
// RemoteFrameData declaration:

// Represents data about an ExecutionCOntext from the point of view of the
// parent frame that owns it.
class RemoteFrameData : public base::LinkNode<RemoteFrameData> {
 public:
  using Comparator = TokenComparator<RemoteFrameData, blink::RemoteFrameToken>;
  using PassKey = base::PassKey<RemoteFrameData>;

  RemoteFrameData() = delete;
  RemoteFrameData(ProcessData* process_data,
                  const blink::RemoteFrameToken& token,
                  ExecutionContextData* execution_context_data);
  RemoteFrameData(const RemoteFrameData&) = delete;
  RemoteFrameData& operator=(const RemoteFrameData&) = delete;
  ~RemoteFrameData();

  // Simple accessors.
  ProcessData* process_data() const { return process_data_; }
  ExecutionContextData* execution_context_data() const {
    return execution_context_data_;
  }

  // For consistency, all Data objects have a GetToken() function.
  const blink::RemoteFrameToken& GetToken() const { return token_; }

  // Returns true if this object is currently being tracked (it is in
  // ProcessData::remote_frame_datas_, and
  // V8ContextTrackerDataStore::global_remote_frame_datas_).
  [[nodiscard]] bool IsTracked() const;

 private:
  const raw_ptr<ProcessData> process_data_;
  const blink::RemoteFrameToken token_;
  raw_ptr<ExecutionContextData> execution_context_data_;
};

////////////////////////////////////////////////////////////////////////////////
// V8ContextData declaration:

// Internal wrapper of V8ContextState. Augments with additional data needed for
// the implementation.
class V8ContextData : public base::LinkNode<V8ContextData>,
                      public V8ContextState {
 public:
  using Comparator = TokenComparator<V8ContextData, blink::V8ContextToken>;
  using PassKey = base::PassKey<V8ContextData>;

  V8ContextData() = delete;
  V8ContextData(ProcessData* process_data,
                const mojom::V8ContextDescription& description,
                ExecutionContextData* execution_context_data);
  V8ContextData(const V8ContextData&) = delete;
  V8ContextData& operator=(const V8ContextData&) = delete;
  ~V8ContextData() override;

  // Simple accessors.
  ProcessData* process_data() const { return process_data_; }

  // For consistency, all Data objects have a GetToken() function.
  const blink::V8ContextToken& GetToken() const { return description.token; }

  // Returns true if this object is currently being tracked (its in
  // ProcessData::v8_context_datas_, and
  // V8ContextTrackerDataStore::global_v8_context_datas_).
  [[nodiscard]] bool IsTracked() const;

  // Returns the ExecutionContextData associated with this V8ContextData.
  ExecutionContextData* GetExecutionContextData() const;

  // Marks this context as having been successfully passed into the data store.
  void SetWasTracked(base::PassKey<V8ContextTrackerDataStore>);

  // Marks this context as detached. Returns true if the state changed, false
  // if it was already detached.
  [[nodiscard]] bool MarkDetached(base::PassKey<ProcessData>);

  // Returns true if this is the "main" V8Context for an ExecutionContext.
  // This will return true if |GetExecutionContextData()| is a frame and
  // |description.world_type| is kMain, or if |GetExecutionContextData()| is a
  // worker and |description.world_type| is a kWorkerOrWorklet.
  bool IsMainV8Context() const;

 private:
  bool MarkDetachedImpl();

  const raw_ptr<ProcessData> process_data_;
  bool was_tracked_ = false;
};

////////////////////////////////////////////////////////////////////////////////
// ProcessData declaration:

class ProcessData : public ExternalNodeAttachedDataImpl<ProcessData> {
 public:
  using PassKey = base::PassKey<ProcessData>;

  explicit ProcessData(const ProcessNodeImpl* process_node);
  ~ProcessData() override;

  // Simple accessors.
  V8ContextTrackerDataStore* data_store() const { return data_store_; }

  // Tears down this ProcessData by ensuring that all associated
  // ExecutionContextDatas and V8ContextDatas are cleaned up. This must be
  // called *prior* to the destructor being invoked.
  void TearDown();

  // Adds the provided object to the list of process-associated objects. The
  // object must not be part of a list, its process data must match this one,
  // and it must return false for "ShouldDestroy" (if applicable). For removal,
  // the object must be part of a list, the process data must match this one and
  // "ShouldDestroy" must return false.
  void Add(base::PassKey<V8ContextTrackerDataStore>,
           ExecutionContextData* ec_data);
  void Add(base::PassKey<V8ContextTrackerDataStore>, RemoteFrameData* rf_data);
  void Add(base::PassKey<V8ContextTrackerDataStore>, V8ContextData* v8_data);
  void Remove(base::PassKey<V8ContextTrackerDataStore>,
              ExecutionContextData* ec_data);
  void Remove(base::PassKey<V8ContextTrackerDataStore>,
              RemoteFrameData* rf_data);
  void Remove(base::PassKey<V8ContextTrackerDataStore>, V8ContextData* v8_data);

  // For marking objects detached/destroyed. Returns true if the state
  // actually changed, false otherwise.
  [[nodiscard]] bool MarkDestroyed(base::PassKey<V8ContextTrackerDataStore>,
                                   ExecutionContextData* ec_data);
  [[nodiscard]] bool MarkDetached(base::PassKey<V8ContextTrackerDataStore>,
                                  V8ContextData* v8_data);

  size_t GetExecutionContextDataCount() const {
    return counts_.GetExecutionContextDataCount();
  }
  size_t GetDestroyedExecutionContextDataCount() const {
    return counts_.GetDestroyedExecutionContextDataCount();
  }
  size_t GetV8ContextDataCount() const {
    return counts_.GetV8ContextDataCount();
  }
  size_t GetDetachedV8ContextDataCount() const {
    return counts_.GetDetachedV8ContextDataCount();
  }

 private:
  // Used to initialize |data_store_| at construction.
  static V8ContextTrackerDataStore* GetDataStore(
      const ProcessNodeImpl* process_node) {
    return V8ContextTracker::GetFromGraph(process_node->graph())->data_store();
  }

  // Pointer to the DataStore that implicitly owns us.
  const raw_ptr<V8ContextTrackerDataStore> data_store_;

  // Counts the number of ExecutionContexts and V8Contexts.
  ContextCounts counts_;

  // List of ExecutionContextDatas associated with this process.
  base::LinkedList<ExecutionContextData> execution_context_datas_;

  // List of RemoteFrameDatas associated with this process.
  base::LinkedList<RemoteFrameData> remote_frame_datas_;

  // List of V8ContextDatas associated with this process.
  base::LinkedList<V8ContextData> v8_context_datas_;
};

////////////////////////////////////////////////////////////////////////////////
// V8ContextTrackerDataStore declaration:

// This class acts as the owner of all tracked objects. Objects are created
// in isolation, and ownership passed to this object. Management of all
// per-process lists is centralized through this object.
class V8ContextTrackerDataStore {
 public:
  using PassKey = base::PassKey<V8ContextTrackerDataStore>;

  V8ContextTrackerDataStore();
  ~V8ContextTrackerDataStore();

  // Passes ownership of an object. An object with the same token must not
  // already exist ("Get" should return nullptr). Note that when passing an
  // |ec_data| to the impl that "ShouldDestroy" should return false.
  void Pass(std::unique_ptr<ExecutionContextData> ec_data);
  void Pass(std::unique_ptr<RemoteFrameData> rf_data);
  [[nodiscard]] bool Pass(std::unique_ptr<V8ContextData> v8_data);

  // Looks up owned objects by token.
  ExecutionContextData* Get(const blink::ExecutionContextToken& token);
  RemoteFrameData* Get(const blink::RemoteFrameToken& token);
  V8ContextData* Get(const blink::V8ContextToken& token);

  // For marking objects as detached/destroyed. "MarkDetached" returns true if
  // the object was not previously detached, false otherwise.
  void MarkDestroyed(ExecutionContextData* ec_data);
  [[nodiscard]] bool MarkDetached(V8ContextData* v8_data);

  // Destroys objects by token. They must exist ("Get" should return non
  // nullptr).
  void Destroy(const blink::ExecutionContextToken& token);
  void Destroy(const blink::RemoteFrameToken& token);
  void Destroy(const blink::V8ContextToken& token);

  size_t GetDestroyedExecutionContextDataCount() const {
    return destroyed_execution_context_count_;
  }
  size_t GetDetachedV8ContextDataCount() const {
    return detached_v8_context_count_;
  }

  size_t GetExecutionContextDataCount() const {
    return global_execution_context_datas_.size();
  }
  size_t GetRemoteFrameDataCount() const {
    return global_remote_frame_datas_.size();
  }
  size_t GetV8ContextDataCount() const {
    return global_v8_context_datas_.size();
  }

 private:
  size_t destroyed_execution_context_count_ = 0;
  size_t detached_v8_context_count_ = 0;

  // Browser wide registry of ExecutionContextData objects.
  std::set<std::unique_ptr<ExecutionContextData>,
           ExecutionContextData::Comparator>
      global_execution_context_datas_;

  // Browser-wide registry of RemoteFrameData objects.
  std::set<std::unique_ptr<RemoteFrameData>, RemoteFrameData::Comparator>
      global_remote_frame_datas_;

  // Browser wide registry of V8ContextData objects.
  std::set<std::unique_ptr<V8ContextData>, V8ContextData::Comparator>
      global_v8_context_datas_;
};

}  // namespace internal
}  // namespace v8_memory
}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_V8_MEMORY_V8_CONTEXT_TRACKER_INTERNAL_H_
