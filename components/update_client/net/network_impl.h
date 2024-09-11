// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_NET_NETWORK_IMPL_H_
#define COMPONENTS_UPDATE_CLIENT_NET_NETWORK_IMPL_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "components/update_client/net/network_chromium.h"
#include "components/update_client/network.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace update_client {

class NetworkFetcherImpl : public NetworkFetcher {
 public:
  explicit NetworkFetcherImpl(
      scoped_refptr<network::SharedURLLoaderFactory> shared_url_network_factory,
      SendCookiesPredicate cookie_predicate);

  NetworkFetcherImpl(const NetworkFetcherImpl&) = delete;
  NetworkFetcherImpl& operator=(const NetworkFetcherImpl&) = delete;

  ~NetworkFetcherImpl() override;

  // NetworkFetcher overrides.
  void PostRequest(
      const GURL& url,
      const std::string& post_data,
      const std::string& content_type,
      const base::flat_map<std::string, std::string>& post_additional_headers,
      ResponseStartedCallback response_started_callback,
      ProgressCallback progress_callback,
      PostRequestCompleteCallback post_request_complete_callback) override;
  base::OnceClosure DownloadToFile(
      const GURL& url,
      const base::FilePath& file_path,
      ResponseStartedCallback response_started_callback,
      ProgressCallback progress_callback,
      DownloadToFileCompleteCallback download_to_file_complete_callback)
      override;

 private:
  void OnResponseStartedCallback(
      ResponseStartedCallback response_started_callback,
      const GURL& final_url,
      const network::mojom::URLResponseHead& response_head);

  void OnProgressCallback(ProgressCallback response_started_callback,
                          uint64_t current);

  static constexpr int kMaxRetriesOnNetworkChange = 3;

  scoped_refptr<network::SharedURLLoaderFactory> shared_url_network_factory_;
  SendCookiesPredicate cookie_predicate_;
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_NET_NETWORK_IMPL_H_
