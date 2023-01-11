// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCOUNT_MANAGER_CORE_CHROMEOS_ACCESS_TOKEN_FETCHER_H_
#define COMPONENTS_ACCOUNT_MANAGER_CORE_CHROMEOS_ACCESS_TOKEN_FETCHER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "chromeos/crosapi/mojom/account_manager.mojom.h"
#include "components/account_manager_core/chromeos/account_manager.h"
#include "google_apis/gaia/oauth2_access_token_consumer.h"
#include "google_apis/gaia/oauth2_access_token_fetcher.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace crosapi {

// Mojo interface implementation to fetch access tokens using Chrome OS Account
// Manager.
class COMPONENT_EXPORT(ASH_COMPONENTS_ACCOUNT_MANAGER) AccessTokenFetcher
    : public mojom::AccessTokenFetcher,
      public OAuth2AccessTokenConsumer {
 public:
  // `account_manager` is a non owning pointer to Chrome OS Account Manager and
  // is guaranteed to outlive `this` instance.
  // `mojo_account_key` is the account for which an access token needs to be
  // fetched.
  // `done_callback` is called after an access token fetch is complete. Used by
  // the owner of `this` object to figure out when it is safe to delete it.
  AccessTokenFetcher(
      account_manager::AccountManager* account_manager,
      mojom::AccountKeyPtr mojo_account_key,
      const std::string& consumer_name,
      base::OnceCallback<void(AccessTokenFetcher*)> done_callback,
      mojo::PendingReceiver<mojom::AccessTokenFetcher> receiver);
  AccessTokenFetcher(const AccessTokenFetcher&) = delete;
  AccessTokenFetcher& operator=(const AccessTokenFetcher&) = delete;
  ~AccessTokenFetcher() override;

  // mojom::AccessTokenFetcher overrides.
  void Start(const std::vector<std::string>& scopes,
             StartCallback callback) override;

  // OAuth2AccessTokenConsumer overrides.
  void OnGetTokenSuccess(const TokenResponse& token_response) override;
  void OnGetTokenFailure(const GoogleServiceAuthError& error) override;
  std::string GetConsumerName() const override;

 private:
  // Mojo pipe disconnection handler.
  void OnMojoPipeError();
  // Finish and cleanup by calling `done_callback_`.
  void Finish();

  std::unique_ptr<OAuth2AccessTokenFetcher> access_token_fetcher_;
  base::OnceCallback<void(mojom::AccessTokenResultPtr)> callback_;
  const std::string consumer_name_;
  // Called after `this` object's work is done and it can be safely deleted.
  base::OnceCallback<void(AccessTokenFetcher*)> done_callback_;
  mojo::Receiver<mojom::AccessTokenFetcher> receiver_;
};

}  // namespace crosapi

#endif  // COMPONENTS_ACCOUNT_MANAGER_CORE_CHROMEOS_ACCESS_TOKEN_FETCHER_H_
