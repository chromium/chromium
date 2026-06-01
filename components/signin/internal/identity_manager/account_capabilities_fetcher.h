// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_ACCOUNT_CAPABILITIES_FETCHER_H_
#define COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_ACCOUNT_CAPABILITIES_FETCHER_H_

#include <optional>

#include "base/functional/callback.h"
#include "components/signin/public/identity_manager/account_capabilities.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "google_apis/gaia/core_account_id.h"

class AccountCapabilitiesFetcher {
 public:
  // This provides a hint regarding the priority this fetch should be performed
  // with.
  // Platform-specific implementations may or may not change the actual priority
  // of the fetch based on this.
  enum class FetchPriority {
    // This corresponds to a user-visible foreground operation, and should be
    // completed as quickly as possible.
    kForeground,
    // This corresponds to a non-user-visible background operation, and does not
    // need to be completed in a timely fashion.
    kBackground,
  };

  // Callback invoked each time the fetcher fetches some account capabilities.
  // This may be called zero, one or multiple times. It cannot be called after
  // `on_all_fetches_complete_callback_` is called.
  using OnSomeCapabilitiesFetchedCallback =
      base::RepeatingCallback<void(const CoreAccountId&,
                                   const AccountCapabilities&)>;
  // Callback invoked once all fetches are complete (either successfully or
  // unsuccessfully).
  using OnAllFetchesCompleteCallback =
      base::OnceCallback<void(const CoreAccountId&)>;

  explicit AccountCapabilitiesFetcher(
      const CoreAccountInfo& account_info,
      FetchPriority fetch_priority,
      OnSomeCapabilitiesFetchedCallback on_some_capabilities_fetched_callback,
      OnAllFetchesCompleteCallback on_all_fetches_complete_callback);
  virtual ~AccountCapabilitiesFetcher();

  AccountCapabilitiesFetcher(const AccountCapabilitiesFetcher&) = delete;
  AccountCapabilitiesFetcher& operator=(const AccountCapabilitiesFetcher&) =
      delete;

  // Start fetching account capabilities. Must be called no more than once per
  // object lifetime.
  void Start();

 protected:
  const CoreAccountInfo& account_info() { return account_info_; }
  const CoreAccountId& account_id() { return account_info_.account_id; }

  // Implemented by subclasses.
  virtual void StartImpl() = 0;

  // Helper for fetchers that implement a single-phase fetch.
  // Completes the fetch by calling `on_some_capabilities_fetched_callback_`
  // then `on_all_fetches_complete_callback_`. Must be called no more than once
  // per object lifetime.
  // `this` may be destroyed after calling this function.
  void UpdateAndCompleteFetchAndMaybeDestroySelf(
      const std::optional<AccountCapabilities>& capabilities);

  // Helper for fetchers that implement a multi-phase fetch.
  void UpdateFetchedCapabilities(const AccountCapabilities& capabilities);
  void CompleteFetchAndMaybeDestroySelf();

  FetchPriority fetch_priority() { return fetch_priority_; }

 private:
  // Ensures that `Start()` isn't called multiple times.
  bool started_ = false;

  const CoreAccountInfo account_info_;
  const FetchPriority fetch_priority_;
  OnSomeCapabilitiesFetchedCallback on_some_capabilities_fetched_callback_;
  OnAllFetchesCompleteCallback on_all_fetches_complete_callback_;
};

#endif  // COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_ACCOUNT_CAPABILITIES_FETCHER_H_
