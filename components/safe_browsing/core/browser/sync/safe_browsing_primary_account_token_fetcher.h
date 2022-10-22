// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_SYNC_SAFE_BROWSING_PRIMARY_ACCOUNT_TOKEN_FETCHER_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_SYNC_SAFE_BROWSING_PRIMARY_ACCOUNT_TOKEN_FETCHER_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/safe_browsing/core/browser/safe_browsing_token_fetch_tracker.h"
#include "components/safe_browsing/core/browser/safe_browsing_token_fetcher.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace signin {
class AccessTokenFetcher;
class IdentityManager;
}  // namespace signin

namespace safe_browsing {

// This class fetches access tokens for Safe Browsing for the current
// primary account.
class SafeBrowsingPrimaryAccountTokenFetcher : public SafeBrowsingTokenFetcher {
 public:
  // Create a SafeBrowsingPrimaryAccountTokenFetcher for the primary account of
  // |identity_manager|. |identity_manager| is unowned, and must outlive this
  // object.
  explicit SafeBrowsingPrimaryAccountTokenFetcher(
      signin::IdentityManager* identity_manager);

  ~SafeBrowsingPrimaryAccountTokenFetcher() override;

  // SafeBrowsingTokenFetcher:
  void Start(Callback callback) override;
  void OnInvalidAccessToken(const std::string& invalid_access_token) override;

 private:
  void OnTokenFetched(int request_id,
                      GoogleServiceAuthError error,
                      signin::AccessTokenInfo access_token_info);
  void OnTokenTimeout(int request_id);

  SEQUENCE_CHECKER(sequence_checker_);

  // Reference to the identity manager to fetch from.
  raw_ptr<signin::IdentityManager, DanglingUntriaged> identity_manager_;

  // Active fetchers, keyed by ID.
  base::flat_map<int, std::unique_ptr<signin::AccessTokenFetcher>>
      token_fetchers_;

  SafeBrowsingTokenFetchTracker token_fetch_tracker_;

  base::WeakPtrFactory<SafeBrowsingPrimaryAccountTokenFetcher>
      weak_ptr_factory_;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_SYNC_SAFE_BROWSING_PRIMARY_ACCOUNT_TOKEN_FETCHER_H_
