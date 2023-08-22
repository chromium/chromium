// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_RESOURCE_FETCHER_H_
#define COMPONENTS_FEED_CORE_V2_RESOURCE_FETCHER_H_

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/feed/core/v2/public/types.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace feed {

struct NetworkResponse;

// Fetcher object to retrieve a resource from a URL.
class ResourceFetcher {
 public:
  using ResourceCallback = base::OnceCallback<void(NetworkResponse)>;
  explicit ResourceFetcher(
      scoped_refptr<::network::SharedURLLoaderFactory> url_loader_factory);
  ~ResourceFetcher();
  ResourceFetcher(const ResourceFetcher&) = delete;
  ResourceFetcher& operator=(const ResourceFetcher&) = delete;

  void Fetch(const GURL& url,
             const std::string& method,
             const std::vector<std::string>& header_names_and_values,
             const std::string& post_data,
             ResourceCallback callback);

 private:
  // Called when fetch request completes.
  void OnFetchComplete(std::unique_ptr<network::SimpleURLLoader> url_loader,
                       ResourceCallback callback,
                       std::unique_ptr<std::string> response_data);

  const scoped_refptr<::network::SharedURLLoaderFactory> url_loader_factory_;
  base::WeakPtrFactory<ResourceFetcher> weak_factory_{this};
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_RESOURCE_FETCHER_H_
