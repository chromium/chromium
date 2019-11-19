// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_REQUEST_FETCHER_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_REQUEST_FETCHER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace offline_pages {

// Asynchronously fetches the offline prefetch related data from the server.
class PrefetchRequestFetcher {
 public:
  using FinishedCallback = base::OnceCallback<void(PrefetchRequestStatus status,
                                                   const std::string& data)>;

  // Creates a fetcher that will sends a GET request to the server.
  static std::unique_ptr<PrefetchRequestFetcher> CreateForGet(
      const GURL& url,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      FinishedCallback callback);

  // Creates a fetcher that will sends a POST request to the server.
  static std::unique_ptr<PrefetchRequestFetcher> CreateForPost(
      const GURL& url,
      const std::string& message,
      const std::string& testing_header_value,
      bool empty_request,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      FinishedCallback callback);

  ~PrefetchRequestFetcher();

  void OnURLLoadComplete(std::unique_ptr<std::string> response_body);

 private:
  // If |message| is empty, the GET request is sent. Otherwise, the POST request
  // is sent with |message| as post data.
  PrefetchRequestFetcher(
      const GURL& url,
      const std::string& message,
      const std::string& testing_header_value,
      bool empty_request,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      FinishedCallback callback);

  PrefetchRequestStatus ParseResponse(
      std::unique_ptr<std::string> response_body,
      std::string* data);

  // Whether this is a GeneratePageBundle request with no pages (used for the
  // server-enabled check).
  bool empty_request_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  FinishedCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(PrefetchRequestFetcher);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_REQUEST_FETCHER_H_
