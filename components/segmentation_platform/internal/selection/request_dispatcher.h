// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SELECTION_REQUEST_DISPATCHER_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SELECTION_REQUEST_DISPATCHER_H_

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>

#include "base/containers/circular_deque.h"
#include "base/memory/scoped_refptr.h"
#include "components/segmentation_platform/internal/database/storage_service.h"
#include "components/segmentation_platform/internal/selection/request_handler.h"
#include "components/segmentation_platform/public/input_context.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"
#include "components/segmentation_platform/public/result.h"

namespace segmentation_platform {
struct PredictionOptions;
class SegmentResultProvider;

// RequestDispatcher is the topmost layer in serving API requests for all
// clients. It's responsible for
// 1. Queuing API requests until the platform isn't fully initialized.
// 2. Dispatching requests to client specific request handlers.
class RequestDispatcher {
 public:
  explicit RequestDispatcher(StorageService* storage_service);
  ~RequestDispatcher();

  // Disallow copy/assign.
  RequestDispatcher(RequestDispatcher&) = delete;
  RequestDispatcher& operator=(RequestDispatcher&) = delete;

  // Called when platform and database initializations are completed.
  void OnPlatformInitialized(
      bool success,
      ExecutionService* execution_service,
      std::map<std::string, std::unique_ptr<SegmentResultProvider>>
          result_providers);

  // Called when the model for |segment_id| has been initialized. Used to
  // execute any queued requests that depend on that model.
  void OnModelUpdated(proto::SegmentId segment_id);

  // Client API. See `SegmentationPlatformService::GetClassificationResult`.
  void GetClassificationResult(const std::string& segmentation_key,
                               const PredictionOptions& options,
                               scoped_refptr<InputContext> input_context,
                               ClassificationResultCallback callback);

  // Client API. See `SegmentationPlatformService::GetAnnotatedNumericResult`.
  void GetAnnotatedNumericResult(const std::string& segmentation_key,
                                 const PredictionOptions& options,
                                 scoped_refptr<InputContext> input_context,
                                 AnnotatedNumericResultCallback callback);

  // For testing only.
  int GetPendingActionCountForTesting();
  void set_request_handler_for_testing(
      const std::string& segmentation_key,
      std::unique_ptr<RequestHandler> request_handler) {
    request_handlers_[segmentation_key] = std::move(request_handler);
  }

 private:
  void OnModelInitializationTimeout();
  void ExecuteAllPendingActions();
  void ExecutePendingActionsForKey(const std::string& segmentation_key);

  using WrappedCallback = base::OnceCallback<void(bool, const RawResult&)>;
  void GetModelResult(const std::string& segmentation_key,
                      const PredictionOptions& options,
                      scoped_refptr<InputContext> input_context,
                      WrappedCallback callback);

  void ExecuteOnDemand(const std::string& segmentation_key,
                       const Config* config,
                       const PredictionOptions& options,
                       scoped_refptr<InputContext> input_context,
                       WrappedCallback callback);

  void OnFinishedOnDemandExecution(const std::string& segmentation_key,
                                   const Config* config,
                                   const PredictionOptions& options,
                                   scoped_refptr<InputContext> input_context,
                                   WrappedCallback callback,
                                   const RawResult& raw_result);

  void HandleCachedExecution(const std::string& segmentation_key,
                             const Config* config,
                             const PredictionOptions& options,
                             scoped_refptr<InputContext> input_context,
                             WrappedCallback callback);

  // Wrap the result callback for recording metrics and converting raw result to
  // necessary result type.
  template <typename ResultType>
  void CallbackWrapper(const std::string& segmentation_key,
                       base::Time start_time,
                       base::OnceCallback<void(const ResultType&)> callback,
                       bool is_cached_result,
                       const RawResult& raw_result);

  // Request handlers associated with the clients.
  std::map<std::string, std::unique_ptr<RequestHandler>> request_handlers_;

  // List of segmentation keys whose models haven't been initialized. Used to
  // enqueue requests that involve an uninitialized model. It gets populated
  // when the platform initializes and each element gets removed when
  // |OnModelUpdated| gets called with its corresponding segment ID. All
  // elements get cleared after a timeout to avoid waiting for too long.
  std::set<std::string> uninitialized_segmentation_keys_;

  const raw_ptr<StorageService> storage_service_;

  // Storage initialization status.
  std::optional<bool> storage_init_status_;

  // For caching any method calls that were received before initialization.
  // Key is a segmentation key, value is a queue of actions that use that model.
  std::map<std::string, base::circular_deque<base::OnceClosure>>
      pending_actions_;

  base::WeakPtrFactory<RequestDispatcher> weak_ptr_factory_{this};
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SELECTION_REQUEST_DISPATCHER_H_
