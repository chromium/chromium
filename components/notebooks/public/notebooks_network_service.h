// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NOTEBOOKS_PUBLIC_NOTEBOOKS_NETWORK_SERVICE_H_
#define COMPONENTS_NOTEBOOKS_PUBLIC_NOTEBOOKS_NETWORK_SERVICE_H_

#include <memory>
#include <string>
#include <string_view>

#include "base/functional/callback_forward.h"

namespace notebooks {

// Class for making notebook-related network requests.
class NotebooksNetworkService {
 public:
  enum class NetworkLoaderStatus {
    kUnknown = 0,
    kSuccess = 1,
    kTransientFailure = 2,
    kPersistentFailure = 3
  };

  struct LoadResult {
    LoadResult() = default;
    LoadResult(std::string result_bytes,
               NetworkLoaderStatus status,
               int network_error_code);
    ~LoadResult() = default;

    std::string result_bytes;
    NetworkLoaderStatus status = NetworkLoaderStatus::kUnknown;
    int network_error_code = 0;
  };

  // Callback to return the network response to the caller.
  using NetworkLoaderCallback =
      base::OnceCallback<void(std::unique_ptr<LoadResult>)>;

  NotebooksNetworkService() = default;
  virtual ~NotebooksNetworkService() = default;

  // Disallow copy/assign.
  NotebooksNetworkService(const NotebooksNetworkService&) = delete;
  NotebooksNetworkService& operator=(const NotebooksNetworkService&) = delete;

  // Called to create a notebook. Callback will be invoked once the
  // operation completes. If a non-network-related error occurs, e.g. because
  // the feature was not enabled, a nullptr will be passed to the callback.
  // For network errors, `LoadResult` will be populated with the error info.
  virtual void CreateNotebook(std::string_view notebook_display_name,
                              NetworkLoaderCallback callback) = 0;

  // Called to create a notebook source. Callback will be invoked once the
  // operation completes. If a non-network-related error occurs, e.g. because
  // the feature was not enabled, a nullptr will be passed to the callback.
  // For network errors, `LoadResult` will be populated with the error info.
  virtual void CreateNotebookSource(std::string_view notebook_id,
                                    std::string_view source_id,
                                    NetworkLoaderCallback callback) = 0;
};

}  // namespace notebooks

#endif  // COMPONENTS_NOTEBOOKS_PUBLIC_NOTEBOOKS_NETWORK_SERVICE_H_
