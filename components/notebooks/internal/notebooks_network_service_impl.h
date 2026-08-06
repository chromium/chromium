// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NOTEBOOKS_INTERNAL_NOTEBOOKS_NETWORK_SERVICE_IMPL_H_
#define COMPONENTS_NOTEBOOKS_INTERNAL_NOTEBOOKS_NETWORK_SERVICE_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/notebooks/public/notebooks_network_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

class GURL;

namespace endpoint_fetcher {
class EndpointFetcher;
struct EndpointResponse;
}  // namespace endpoint_fetcher

namespace signin {
class IdentityManager;
}  // namespace signin

namespace net {
struct NetworkTrafficAnnotationTag;
}  // namespace net

namespace notebooks {

// Internal implementation of `NotebooksNetworkService`.
class NotebooksNetworkServiceImpl : public NotebooksNetworkService {
 public:
  NotebooksNetworkServiceImpl(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager* identity_manager);
  ~NotebooksNetworkServiceImpl() override;

  // Disallow copy/assign.
  NotebooksNetworkServiceImpl(const NotebooksNetworkServiceImpl&) = delete;
  NotebooksNetworkServiceImpl& operator=(const NotebooksNetworkServiceImpl&) =
      delete;

  // NotebooksNetworkService Impl.
  void CreateNotebook(std::string_view notebook_display_name,
                      NetworkLoaderCallback callback) override;
  void CreateNotebookSource(std::string_view notebook_id,
                            std::string_view source_id,
                            NetworkLoaderCallback callback) override;

 protected:
  // This method could be overridden in tests.
  virtual std::unique_ptr<endpoint_fetcher::EndpointFetcher>
  CreateEndpointFetcher(const GURL& url,
                        const std::string& post_data,
                        const net::NetworkTrafficAnnotationTag& annotation_tag);

  virtual GURL ConstructServiceURL(std::string_view path);

 private:
  void FetchInternal(const GURL& url,
                     const std::string& post_data,
                     const net::NetworkTrafficAnnotationTag& annotation_tag,
                     NetworkLoaderCallback callback);

  // Called when response is received
  void OnDownloadComplete(
      NetworkLoaderCallback callback,
      std::unique_ptr<endpoint_fetcher::EndpointFetcher> fetcher,
      std::unique_ptr<endpoint_fetcher::EndpointResponse> response);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  raw_ptr<signin::IdentityManager> identity_manager_;

  base::WeakPtrFactory<NotebooksNetworkServiceImpl> weak_ptr_factory_{this};
};

}  // namespace notebooks

#endif  // COMPONENTS_NOTEBOOKS_INTERNAL_NOTEBOOKS_NETWORK_SERVICE_IMPL_H_
