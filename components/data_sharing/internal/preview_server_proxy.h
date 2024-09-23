// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_SHARING_INTERNAL_PREVIEW_SERVER_PROXY_H_
#define COMPONENTS_DATA_SHARING_INTERNAL_PREVIEW_SERVER_PROXY_H_

#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/data_sharing/public/data_sharing_service.h"
#include "components/data_sharing/public/group_data.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace signin {
class IdentityManager;
}  // namespace signin

namespace data_sharing {

// Class for retrieving a preview of shared data before user can join a group.
class PreviewServerProxy {
 public:
  PreviewServerProxy(
      signin::IdentityManager* identity_manager,
      const scoped_refptr<network::SharedURLLoaderFactory>& url_loader_factory);
  virtual ~PreviewServerProxy();
  PreviewServerProxy(const PreviewServerProxy& other) = delete;
  PreviewServerProxy& operator=(const PreviewServerProxy& other) = delete;

  // Called to get a preview of shared data.
  virtual void GetSharedDataPreview(
      const GroupToken& group_token,
      base::OnceCallback<
          void(const DataSharingService::SharedDataPreviewOrFailureOutcome&)>
          callback);

 protected:
  virtual std::unique_ptr<EndpointFetcher> CreateEndpointFetcher(
      const GURL& url);

 private:
  void HandleServerResponse(
      base::OnceCallback<
          void(const DataSharingService::SharedDataPreviewOrFailureOutcome&)>
          callback,
      std::unique_ptr<EndpointFetcher> endpoint_fetcher,
      std::unique_ptr<EndpointResponse> response);

  void OnResponseJsonParsed(
      base::OnceCallback<
          void(const DataSharingService::SharedDataPreviewOrFailureOutcome&)>
          callback,
      data_decoder::DataDecoder::ValueOrError result);

  raw_ptr<signin::IdentityManager> identity_manager_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  base::WeakPtrFactory<PreviewServerProxy> weak_ptr_factory_{this};
};

}  // namespace data_sharing

#endif  // COMPONENTS_DATA_SHARING_INTERNAL_PREVIEW_SERVER_PROXY_H_
