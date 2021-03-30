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
#include "components/history_clusters/core/memories.mojom.h"
#include "components/history_clusters/core/visit_data.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace memories {

using Memories = std::vector<mojom::MemoryPtr>;
using MemoriesCallback = base::OnceCallback<void(Memories)>;

// A helper class to communicate with the remote model. Forms requests from
// |MemoriesVisit|s and parses the response into |mojom::MemoryPtr|s.
class MemoriesRemoteModelHelper {
 public:
  explicit MemoriesRemoteModelHelper(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~MemoriesRemoteModelHelper();

  // POSTs |visits| to |endpoint_| and invokes |callback| with the retrieved
  // |MemoryPtr|s.
  void GetMemories(const std::vector<MemoriesVisit>& visits,
                   MemoriesCallback callback);

 private:
  // Stops pending requests. Invoking |GetMemories| multiple times will stop
  // incomplete previous requests.
  void StopPendingRequests();

  // Helpers for making requests used by |GetMemories()|.
  static std::unique_ptr<network::ResourceRequest> CreateRequest(
      const GURL& endpoint);
  static std::unique_ptr<network::SimpleURLLoader> CreateLoader(
      std::unique_ptr<network::ResourceRequest> request,
      const std::string& request_body);

  // The most recent request.
  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  // Used to make requests.
  const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
};

}  // namespace memories

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_MEMORIES_REMOTE_MODEL_HELPER_H_
