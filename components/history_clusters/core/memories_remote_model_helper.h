// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_MEMORIES_REMOTE_MODEL_HELPER_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_MEMORIES_REMOTE_MODEL_HELPER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "components/history/core/browser/history_types.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace history_clusters {

using DebugLoggerCallback = base::RepeatingCallback<void(const std::string&)>;

// A helper class to communicate with the remote model. Forms requests from
// `history::AnnotatedVisit`s and parses the response into
// `history::Cluster`s.
class MemoriesRemoteModelHelper {
 public:
  // Pass in a defined `debug_logger` to enable debug logging from this class.
  MemoriesRemoteModelHelper(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      absl::optional<DebugLoggerCallback> debug_logger);
  ~MemoriesRemoteModelHelper();

  // POSTs `visits` to the remote endpoint and invokes `callback` with the
  // retrieved `Cluster`s.
  using MemoriesCallback =
      base::OnceCallback<void(std::vector<history::Cluster>)>;
  void GetMemories(MemoriesCallback callback,
                   const std::vector<history::AnnotatedVisit>& visits);

 private:
  // Helpers for making requests used by `GetMemories()`.
  static std::unique_ptr<network::ResourceRequest> CreateRequest(
      const GURL& endpoint);
  static std::unique_ptr<network::SimpleURLLoader> CreateLoader(
      std::unique_ptr<network::ResourceRequest> request,
      const std::string& request_body);

  // Used to make requests.
  const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // This should be set to absl::nullopt if debug logging is disabled.
  // This is absl::optional, so we can skip the expense of constructing the log
  // messages if the logger is disabled.
  absl::optional<DebugLoggerCallback> debug_logger_;
};

}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_MEMORIES_REMOTE_MODEL_HELPER_H_
