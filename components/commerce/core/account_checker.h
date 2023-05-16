// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_ACCOUNT_CHECKER_H_
#define COMPONENTS_COMMERCE_CORE_ACCOUNT_CHECKER_H_

#include "base/memory/scoped_refptr.h"
#include "base/scoped_observation.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_change_event.h"
#include "components/sync/service/sync_service.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

class GURL;
class PrefService;
class PrefChangeRegistrar;

namespace commerce {

extern const char kOAuthScope[];
extern const char kOAuthName[];
extern const char kGetHttpMethod[];
extern const char kPostHttpMethod[];
extern const char kContentType[];
extern const char kEmptyPostData[];
extern const char kNotificationsPrefUrl[];

// Used to check user account status.
class AccountChecker : public signin::IdentityManager::Observer {
 public:
  AccountChecker(const AccountChecker&) = delete;
  ~AccountChecker() override;

  virtual bool IsSignedIn();

  // Returns whether bookmarks is currently syncing. This will return true in
  // cases where sync is still initializing, but the sync feature itself is
  // enabled.
  virtual bool IsSyncingBookmarks();

  virtual bool IsAnonymizedUrlDataCollectionEnabled();

  virtual bool IsWebAndAppActivityEnabled();

  virtual bool IsSubjectToParentalControls();

 protected:
  friend class ShoppingService;
  friend class MockAccountChecker;

  // This class should only be initialized in ShoppingService.
  AccountChecker(
      PrefService* pref_service,
      signin::IdentityManager* identity_manager,
      syncer::SyncService* sync_service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  // Fetch users' pref from server on whether to receive price tracking emails.
  void FetchPriceEmailPref();

  // This method could be overridden in tests.
  virtual std::unique_ptr<EndpointFetcher> CreateEndpointFetcher(
      const std::string& oauth_consumer_name,
      const GURL& url,
      const std::string& http_method,
      const std::string& content_type,
      const std::vector<std::string>& scopes,
      int64_t timeout_ms,
      const std::string& post_data,
      const net::NetworkTrafficAnnotationTag& annotation_tag);

 private:
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;

  // Fetch users' consent status on web and app activity.
  void FetchWaaStatus();

  // Handle the responses for fetching users' web and app activity consent
  // status.
  void HandleFetchWaaResponse(
      // Passing the endpoint_fetcher ensures the endpoint_fetcher's
      // lifetime extends to the callback and is not destroyed
      // prematurely (which would result in cancellation of the request).
      // TODO(crbug.com/1362026): Avoid passing this fetcher.
      std::unique_ptr<EndpointFetcher> endpoint_fetcher,
      std::unique_ptr<EndpointResponse> responses);

  void OnFetchWaaJsonParsed(data_decoder::DataDecoder::ValueOrError result);

  // Called when the pref value on whether to receive price tracking emails
  // changes. We need to send the new value to server unless the change is
  // triggered by aligning with the server fetched value.
  void OnPriceEmailPrefChanged();

  void HandleSendPriceEmailPrefResponse(
      // Passing the endpoint_fetcher ensures the endpoint_fetcher's
      // lifetime extends to the callback and is not destroyed
      // prematurely (which would result in cancellation of the request).
      // TODO(crbug.com/1362026): Avoid passing this fetcher.
      std::unique_ptr<EndpointFetcher> endpoint_fetcher,
      std::unique_ptr<EndpointResponse> responses);

  void OnSendPriceEmailPrefJsonParsed(
      data_decoder::DataDecoder::ValueOrError result);

  void HandleFetchPriceEmailPrefResponse(
      // Passing the endpoint_fetcher ensures the endpoint_fetcher's
      // lifetime extends to the callback and is not destroyed
      // prematurely (which would result in cancellation of the request).
      // TODO(crbug.com/1362026): Avoid passing this fetcher.
      std::unique_ptr<EndpointFetcher> endpoint_fetcher,
      std::unique_ptr<EndpointResponse> responses);

  void OnFetchPriceEmailPrefJsonParsed(
      data_decoder::DataDecoder::ValueOrError result);

  raw_ptr<PrefService> pref_service_;

  raw_ptr<signin::IdentityManager> identity_manager_;

  raw_ptr<syncer::SyncService> sync_service_;

  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      scoped_identity_manager_observation_{this};

  const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  bool is_waiting_for_pref_fetch_completion_;

  // Indicate whether we should ignore the next email pref change. This is true
  // only if the change is triggered by aligning with the server fetched value,
  // in which case we don't need to send the new value to the server again.
  bool ignore_next_email_pref_change_{false};

  base::WeakPtrFactory<AccountChecker> weak_ptr_factory_;
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_ACCOUNT_CHECKER_H_
