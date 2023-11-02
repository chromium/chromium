// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_EXECUTION_CONTEXT_EXECUTION_CONTEXT_ATTACHED_DATA_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_EXECUTION_CONTEXT_EXECUTION_CONTEXT_ATTACHED_DATA_H_

#include <type_traits>

#include "components/performance_manager/public/execution_context/execution_context.h"
#include "components/performance_manager/public/execution_context/execution_context_registry.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/node_attached_data.h"
#include "components/performance_manager/public/graph/worker_node.h"

namespace performance_manager {
namespace execution_context {

// An adapter that can be used to associate NodeAttachedData with all node
// types that represent execution contexts. Under the hood this wraps
// ExternalNodeAttachedDataImpl for each of the underlying node types.
// It is expected that UserDataType have a constructor with the signature
// "UserDataType(const ExecutionContext*)".
template <typename UserDataType>
class ExecutionContextAttachedData {
 public:
  ExecutionContextAttachedData() = default;
  ExecutionContextAttachedData(const ExecutionContextAttachedData&) = delete;
  ExecutionContextAttachedData& operator=(const ExecutionContextAttachedData&) =
      delete;
  virtual ~ExecutionContextAttachedData() = default;

  // Gets the user data for the given |object|, creating it if it doesn't yet
  // exist.
  template <typename NodeTypeOrExecutionContext>
  static UserDataType* GetOrCreate(const NodeTypeOrExecutionContext* object) {
    return Dispatch<GetOrCreateFunctor>(ExecutionContext::From(object));
  }

  // Gets the user data for the given |object|, returning nullptr if it doesn't
  // exist.
  template <typename NodeTypeOrExecutionContext>
  static UserDataType* Get(const NodeTypeOrExecutionContext* object) {
    return Dispatch<GetFunctor>(ExecutionContext::From(object));
  }

  // Destroys the user data associated with the given |object|, returning true
  // on success or false if the user data did not exist to begin with.
  template <typename NodeTypeOrExecutionContext>
  static bool Destroy(const NodeTypeOrExecutionContext* object) {
    return Dispatch<DestroyFunctor>(ExecutionContext::From(object));
  }

 private:
  // A small adapter for UserDataType that makes it compatible with
  // underlying node types.
  class UserDataWrapper : public ExternalNodeAttachedDataImpl<UserDataWrapper>,
                          public UserDataType {
   public:
    explicit UserDataWrapper(const FrameNode* frame_node)
        : UserDataType(
              ExecutionContextRegistry::GetFromGraph(frame_node->GetGraph())
                  ->GetExecutionContextForFrameNode(frame_node)) {}

    explicit UserDataWrapper(const WorkerNode* worker_node)
        : UserDataType(
              ExecutionContextRegistry::GetFromGraph(worker_node->GetGraph())
                  ->GetExecutionContextForWorkerNode(worker_node)) {}
  };

  // Helpers for dynamic dispatch to various functions based on the underlying
  // node type.
  struct GetOrCreateFunctor {
    using ReturnType = UserDataType*;
    template <typename NodeType>
    static UserDataType* Do(const NodeType* node) {
      return ExternalNodeAttachedDataImpl<UserDataWrapper>::GetOrCreate(node);
    }
  };
  struct GetFunctor {
    using ReturnType = UserDataType*;
    template <typename NodeType>
    static UserDataType* Do(const NodeType* node) {
      return ExternalNodeAttachedDataImpl<UserDataWrapper>::Get(node);
    }
  };
  struct DestroyFunctor {
    using ReturnType = bool;
    template <typename NodeType>
    static bool Do(const NodeType* node) {
      return ExternalNodeAttachedDataImpl<UserDataWrapper>::Destroy(node);
    }
  };

  // Helper for dynamic dispatching based on underlying node type.
  template <typename Functor>
  static typename Functor::ReturnType Dispatch(const ExecutionContext* ec) {
    switch (ec->GetType()) {
      case ExecutionContextType::kFrameNode:
        return Functor::Do(ec->GetFrameNode());
      case ExecutionContextType::kWorkerNode:
        return Functor::Do(ec->GetWorkerNode());
    }
  }
};

}  // namespace execution_context
}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_EXECUTION_CONTEXT_EXECUTION_CONTEXT_ATTACHED_DATA_H_
