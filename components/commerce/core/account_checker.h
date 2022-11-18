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
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

class PrefService;
class PrefChangeRegistrar;

namespace commerce {

// Used to check user account status.
class AccountChecker : public signin::IdentityManager::Observer {
 public:
  AccountChecker(const AccountChecker&) = delete;
  ~AccountChecker() override;

  virtual bool IsSignedIn();

  virtual bool IsAnonymizedUrlDataCollectionEnabled();

  virtual bool IsWebAndAppActivityEnabled();

 protected:
  friend class ShoppingService;
  friend class MockAccountChecker;

  // This class should only be initialized in ShoppingService.
  AccountChecker(
      PrefService* pref_service,
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  // Fetch users' pref from server on whether to receive price tracking emails.
  void FetchPriceEmailPref();

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

  // Send users' pref to server on whether to receive price tracking emails.
  void SendPriceEmailPref();

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

  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      scoped_identity_manager_observation_{this};

  const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  bool is_waiting_for_pref_fetch_completion_;

  base::WeakPtrFactory<AccountChecker> weak_ptr_factory_;
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_ACCOUNT_CHECKER_H_
