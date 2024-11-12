// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_ACCOUNT_CHECKER_H_
#define COMPONENTS_COMMERCE_CORE_ACCOUNT_CHECKER_H_

#include <string>

#include "base/memory/scoped_refptr.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

class GURL;
class PrefService;
class PrefChangeRegistrar;

namespace base {
class TimeDelta;
}  // namespace base

namespace commerce {

extern const char kNotificationsPrefUrl[];

// Used to check user account status.
class AccountChecker {
 public:
  AccountChecker(const AccountChecker&) = delete;
  virtual ~AccountChecker();

  virtual bool IsSignedIn();

  // Returns whether bookmarks is currently syncing. This will return true in
  // cases where sync is still initializing, but the sync feature itself is
  // enabled.
  virtual bool IsSyncingBookmarks();

  // Check whether a specific sync entity is enabled by the user. This means
  // the user has chosen to sync the provided model type and does not
  // necessarily mean sync is active.
  virtual bool IsSyncTypeEnabled(syncer::UserSelectableType type);

  virtual bool IsAnonymizedUrlDataCollectionEnabled();

  virtual bool IsSubjectToParentalControls();

  // Whether a user is allowed to use model execution features.
  virtual bool CanUseModelExecutionFeatures();

  // Gets the user's country as determined at startup.
  virtual std::string GetCountry();

  // Gets the user's locale as determine at startup.
  virtual std::string GetLocale();

  virtual PrefService* GetPrefs();

 protected:
  friend class ShoppingService;
  friend class MockAccountChecker;

  // This class should only be initialized in ShoppingService.
  AccountChecker(
      std::string country,
      std::string locale,
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
      const base::TimeDelta& timeout,
      const std::string& post_data,
      const net::NetworkTrafficAnnotationTag& annotation_tag);

 private:
  // Called when the pref value on whether to receive price tracking emails
  // changes. We need to send the new value to server unless the change is
  // triggered by aligning with the server fetched value.
  void OnPriceEmailPrefChanged();

  void HandleSendPriceEmailPrefResponse(
      // Passing the endpoint_fetcher ensures the endpoint_fetcher's
      // lifetime extends to the callback and is not destroyed
      // prematurely (which would result in cancellation of the request).
      // TODO(crbug.com/40238190): Avoid passing this fetcher.
      std::unique_ptr<EndpointFetcher> endpoint_fetcher,
      std::unique_ptr<EndpointResponse> responses);

  void OnSendPriceEmailPrefJsonParsed(
      data_decoder::DataDecoder::ValueOrError result);

  void HandleFetchPriceEmailPrefResponse(
      // Passing the endpoint_fetcher ensures the endpoint_fetcher's
      // lifetime extends to the callback and is not destroyed
      // prematurely (which would result in cancellation of the request).
      // TODO(crbug.com/40238190): Avoid passing this fetcher.
      std::unique_ptr<EndpointFetcher> endpoint_fetcher,
      std::unique_ptr<EndpointResponse> responses);

  void OnFetchPriceEmailPrefJsonParsed(
      data_decoder::DataDecoder::ValueOrError result);

  std::string country_;

  std::string locale_;

  raw_ptr<PrefService> pref_service_;

  raw_ptr<signin::IdentityManager> identity_manager_;

  raw_ptr<syncer::SyncService> sync_service_;

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
