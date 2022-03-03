// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_ACCOUNT_CAPABILITIES_FETCHER_H_
#define COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_ACCOUNT_CAPABILITIES_FETCHER_H_

#include "base/callback.h"
#include "components/signin/public/identity_manager/account_capabilities.h"
#include "google_apis/gaia/core_account_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class AccountCapabilitiesFetcher {
 public:
  using OnCompleteCallback =
      base::OnceCallback<void(const CoreAccountId&,
                              const absl::optional<AccountCapabilities>&)>;

  explicit AccountCapabilitiesFetcher(const CoreAccountId& account_id,
                                      OnCompleteCallback on_complete_callback);
  virtual ~AccountCapabilitiesFetcher();

  AccountCapabilitiesFetcher(const AccountCapabilitiesFetcher&) = delete;
  AccountCapabilitiesFetcher& operator=(const AccountCapabilitiesFetcher&) =
      delete;

  // Start fetching account capabilities.
  virtual void Start() = 0;

 protected:
  const CoreAccountId& account_id() { return account_id_; }

  // Completes the fetch by calling `on_complete_callback_`. Must be called no
  // more than once per object lifetime.
  // `this` may be destroyed after calling this function.
  void CompleteFetchAndMaybeDestroySelf(
      const absl::optional<AccountCapabilities>& capabilities);

 private:
  const CoreAccountId account_id_;
  OnCompleteCallback on_complete_callback_;
};

#endif  // COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_ACCOUNT_CAPABILITIES_FETCHER_H_
