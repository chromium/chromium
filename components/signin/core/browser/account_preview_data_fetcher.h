// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_PREVIEW_DATA_FETCHER_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_PREVIEW_DATA_FETCHER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/signin/core/browser/account_preview_data.h"
#include "components/signin/public/identity_manager/access_token_fetcher.h"
#include "google_apis/gaia/gaia_id.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace signin {

class IdentityManager;

inline constexpr char kAccountPreviewStatsUrl[] =
    "https://chromesyncpreview.pa.googleapis.com/v1/dataTypes/-/statistics";
inline constexpr char kAccountPreviewPreviewsUrl[] =
    "https://chromesyncpreview.pa.googleapis.com/v1/dataTypes/-/"
    "entitiesPreviews";

// Helper class to fetch account preview data from the Sync Preview API.
// Fetches both statistics and entities previews in parallel (after acquiring a
// SyncPreview OAuth token for the input account). Once both fetches complete,
// the provided callback is invoked with the account preview data.
// If the token acquisition or any network request fails or if the response
// format is unexpected, the callback is invoked with no data (std::nullopt).
// The fetching starts on construction.
class AccountPreviewDataFetcher {
 public:
  using FetchCompleteCallback =
      base::OnceCallback<void(const GaiaId&,
                              std::optional<AccountPreviewData>)>;

  AccountPreviewDataFetcher(
      const GaiaId& gaia_id,
      IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      FetchCompleteCallback callback);
  ~AccountPreviewDataFetcher();

 private:
  void OnAccessTokenReceived(GoogleServiceAuthError error,
                             AccessTokenInfo token_info);
  void StartNetworkRequests(const std::string& access_token);
  void OnStatsFetchCompleted(std::optional<std::string> response_body);
  void OnPreviewsFetchCompleted(std::optional<std::string> response_body);
  void OnFetchCompleted();

  const GaiaId gaia_id_;
  const raw_ptr<IdentityManager> identity_manager_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  FetchCompleteCallback callback_;

  std::unique_ptr<AccessTokenFetcher> token_fetcher_;
  std::unique_ptr<network::SimpleURLLoader> stats_url_loader_;
  std::unique_ptr<network::SimpleURLLoader> previews_url_loader_;

  // Starts as empty but valid structure, may be invalidated if the response
  // format of any of the API calls is unexpected.
  std::optional<AccountPreviewData> fetched_data_ = AccountPreviewData();
  base::RepeatingClosure barrier_closure_;

  base::WeakPtrFactory<AccountPreviewDataFetcher> weak_ptr_factory_{this};
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_PREVIEW_DATA_FETCHER_H_
