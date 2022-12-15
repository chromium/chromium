// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SELECTION_REQUEST_DISPATCHER_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SELECTION_REQUEST_DISPATCHER_H_

#include <map>
#include <string>
#include <utility>

#include "base/callback_helpers.h"
#include "base/containers/circular_deque.h"
#include "base/memory/scoped_refptr.h"
#include "components/segmentation_platform/internal/selection/request_handler.h"
#include "components/segmentation_platform/public/input_context.h"
#include "components/segmentation_platform/public/result.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace segmentation_platform {
struct Config;
struct PredictionOptions;
class SegmentResultProvider;

// RequestDispatcher is the topmost layer in serving API requests for all
// clients. It's responsible for
// 1. Queuing API requests until the platform isn't fully initialized.
// 2. Dispatching requests to client specific request handlers.
class RequestDispatcher {
 public:
  RequestDispatcher(
      const std::vector<std::unique_ptr<Config>>& configs,
      std::map<std::string, std::unique_ptr<SegmentResultProvider>>
          result_providers);
  ~RequestDispatcher();

  // Disallow copy/assign.
  RequestDispatcher(RequestDispatcher&) = delete;
  RequestDispatcher& operator=(RequestDispatcher&) = delete;

  // Called when platform and database initializations are completed.
  void OnPlatformInitialized(bool success);

  // Client API. See `SegmentationPlatformService::GetClassificationResult`.
  void GetClassificationResult(const std::string& segmentation_key,
                               const PredictionOptions& options,
                               scoped_refptr<InputContext> input_context,
                               ClassificationResultCallback callback);

  // For testing only.
  int get_pending_actions_size_for_testing() { return pending_actions_.size(); }
  void set_request_handler_for_testing(
      const std::string& segmentation_key,
      std::unique_ptr<RequestHandler> request_handler) {
    request_handlers_[segmentation_key] = std::move(request_handler);
  }

 private:
  // Configs for all registered clients.
  const std::vector<std::unique_ptr<Config>>& configs_;

  // Request handlers associated with the clients.
  std::map<std::string, std::unique_ptr<RequestHandler>> request_handlers_;

  // Storage initialization status.
  absl::optional<bool> storage_init_status_;

  // For caching any method calls that were received before initialization.
  base::circular_deque<base::OnceClosure> pending_actions_;

  base::WeakPtrFactory<RequestDispatcher> weak_ptr_factory_{this};
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SELECTION_REQUEST_DISPATCHER_H_
