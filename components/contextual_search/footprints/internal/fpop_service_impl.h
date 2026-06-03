// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTEXTUAL_SEARCH_FOOTPRINTS_INTERNAL_FPOP_SERVICE_IMPL_H_
#define COMPONENTS_CONTEXTUAL_SEARCH_FOOTPRINTS_INTERNAL_FPOP_SERVICE_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/contextual_search/footprints/public/fpop_service.h"
#include "components/contextual_search/footprints/public/proto/footprints_oneplatform.pb.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace contextual_search {

class FpopServiceImpl : public FpopService {
 public:
  FpopServiceImpl(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~FpopServiceImpl() override;

  FpopServiceImpl(const FpopServiceImpl&) = delete;
  FpopServiceImpl& operator=(const FpopServiceImpl&) = delete;

  // FpopService:
  // Fetches the user's Footprints activity control settings.
  void GetFacs(const footprints::oneplatform::GetFacsRequest& request,
               base::OnceCallback<void(
                   bool success,
                   const footprints::oneplatform::GetFacsResponse& response)>
                   callback) override;
  // Updates the user's Footprints activity control settings.
  void UpdateActivityControlsSettings(
      const footprints::oneplatform::UpdateActivityControlsSettingsRequest&
          request,
      base::OnceCallback<void(
          bool success,
          const footprints::oneplatform::UpdateActivityControlsSettingsResponse&
              response)> callback) override;

 private:
  // Parses the response body from the GetFacs network request.
  void OnGetFacsResponse(
      base::OnceCallback<
          void(bool, const footprints::oneplatform::GetFacsResponse&)> callback,
      bool success,
      const std::string& response_body);

  // Parses the response body from the UpdateActivityControlsSettings network
  // request.
  void OnUpdateActivityControlsSettingsResponse(
      base::OnceCallback<void(bool,
                              const footprints::oneplatform::
                                  UpdateActivityControlsSettingsResponse&)>
          callback,
      bool success,
      const std::string& response_body);

  // Requests an OAuth access token for the `fpop_service` consumer. If a token
  // request is already in progress, the callback is queued. When the token
  // fetch completes, all queued callbacks are executed with the result.
  void RequestAccessToken(
      base::OnceCallback<void(const std::string& access_token)> callback);

  // Handles the result of an access token fetch and flushes queued callbacks.
  void OnAccessTokenFetched(
      base::OnceCallback<void(const std::string& access_token)> callback,
      GoogleServiceAuthError error,
      signin::AccessTokenInfo access_token_info);

  // Sends the authenticated network request to the specified API endpoint.
  void SendRequest(
      const std::string& url,
      const std::string& request_body,
      base::OnceCallback<void(bool success, const std::string& response_body)>
          callback,
      const std::string& access_token);

  raw_ptr<signin::IdentityManager> identity_manager_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<signin::AccessTokenFetcher> access_token_fetcher_;
  std::vector<base::OnceCallback<void(const std::string&)>>
      queued_token_callbacks_;

  base::WeakPtrFactory<FpopServiceImpl> weak_ptr_factory_{this};
};

}  // namespace contextual_search

#endif  // COMPONENTS_CONTEXTUAL_SEARCH_FOOTPRINTS_INTERNAL_FPOP_SERVICE_IMPL_H_
