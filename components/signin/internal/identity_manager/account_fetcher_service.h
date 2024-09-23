// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_ACCOUNT_FETCHER_SERVICE_H_
#define COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_ACCOUNT_FETCHER_SERVICE_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service_observer.h"
#include "components/signin/public/base/persistent_repeating_timer.h"

class AccountCapabilities;
class AccountCapabilitiesFetcher;
class AccountCapabilitiesFetcherFactory;
class AccountInfoFetcher;
class AccountTrackerService;
class ProfileOAuth2TokenService;
class PrefRegistrySimple;
class SigninClient;
struct CoreAccountInfo;

#if BUILDFLAG(IS_ANDROID)
class ChildAccountInfoFetcherAndroid;
#endif

namespace gfx {
class Image;
}

namespace image_fetcher {
struct RequestMetadata;
class ImageDecoder;
class ImageFetcherImpl;
}  // namespace image_fetcher

namespace signin {
enum class Tribool;
}

class AccountFetcherService : public ProfileOAuth2TokenServiceObserver {
 public:
  // Name of the preference that tracks the int64_t representation of the last
  // time the AccountTrackerService was updated.
  static const char kLastUpdatePref[];

  AccountFetcherService();

  AccountFetcherService(const AccountFetcherService&) = delete;
  AccountFetcherService& operator=(const AccountFetcherService&) = delete;

  ~AccountFetcherService() override;

  // Registers the preferences used by AccountFetcherService.
  static void RegisterPrefs(PrefRegistrySimple* user_prefs);

  void Initialize(SigninClient* signin_client,
                  ProfileOAuth2TokenService* token_service,
                  AccountTrackerService* account_tracker_service,
                  std::unique_ptr<image_fetcher::ImageDecoder> image_decoder,
                  std::unique_ptr<AccountCapabilitiesFetcherFactory>
                      account_capabilities_fetcher_factory);

  // Indicates if all user information has been fetched. If the result is false,
  // there are still unfininshed fetchers.
  virtual bool IsAllUserInfoFetched() const;
  virtual bool AreAllAccountCapabilitiesFetched() const;

  AccountTrackerService* account_tracker_service() const {
    return account_tracker_service_;
  }

  // It is important that network fetches are not enabled until the network is
  // initialized. See http://crbug.com/441399 for more context.
  void OnNetworkInitialized();

  // Force-enables network fetches. For use in testing contexts. Use this only
  // if also controlling the URLLoaderFactory used to make network requests
  // (via |signin_client|).
  void EnableNetworkFetchesForTest();

  // Force-enables account removals in response to refresh token revocations.
  // For use in testing contexts. Safer to use than
  // EnableNetworkFetchesForTest(), as invoking this method does not result in
  // network requests.
  void EnableAccountRemovalForTest();

  // Returns the AccountCapabilitiesFetcherFactory, for use in tests only.
  AccountCapabilitiesFetcherFactory*
  GetAccountCapabilitiesFetcherFactoryForTest();

  // Calling this method provides a hint that Account Capabilities may be
  // fetched in the near future, and front-loads some processing to speed
  // up future fetches. This is purely a latency optimization; calling this
  // method is optional.
  void PrepareForFetchingAccountCapabilities();

#if BUILDFLAG(IS_ANDROID)
  // Refresh the AccountInfo if the existing one is stale
  void RefreshAccountInfoIfStale(const CoreAccountId& account_id);

  // Called by ChildAccountInfoFetcherAndroid.
  void SetIsChildAccount(const CoreAccountId& account_id,
                         bool is_child_account);
#endif

  // Destroy any fetchers created for the specified account.
  void DestroyFetchers(const CoreAccountId& account_id);

  // ProfileOAuth2TokenServiceObserver implementation.
  void OnRefreshTokenAvailable(const CoreAccountId& account_id) override;
  void OnRefreshTokenRevoked(const CoreAccountId& account_id) override;
  void OnRefreshTokensLoaded() override;

 private:
  friend class AccountInfoFetcher;

  void RefreshAllAccountInfo(bool only_fetch_if_invalid);

#if BUILDFLAG(IS_ANDROID)
  // Called on all account state changes. Decides whether to fetch new child
  // status information or reset old values that aren't valid now.
  void UpdateChildInfo();
#endif

  void MaybeEnableNetworkFetches();

  // Virtual so that tests can override the network fetching behaviour.
  // Further the two fetches are managed by a different refresh logic and
  // thus, can not be combined.
  void StartFetchingUserInfo(const CoreAccountId& account_id);
#if BUILDFLAG(IS_ANDROID)
  void StartFetchingChildInfo(const CoreAccountId& account_id);

  // Resets the child status to false if it is true. If there is more than one
  // account in a profile, only the main account can be a child.
  void ResetChildInfo();
#endif

  void StartFetchingAccountCapabilities(
      const CoreAccountInfo& core_account_info);

  // Refreshes the AccountInfo associated with |account_id|.
  void RefreshAccountInfo(const CoreAccountId& account_id,
                          bool only_fetch_if_invalid);

  // Called by AccountInfoFetcher.
  void OnUserInfoFetchSuccess(const CoreAccountId& account_id,
                              const base::Value::Dict& user_info);
  void OnUserInfoFetchFailure(const CoreAccountId& account_id);

  // Called by AccountCapabilitiesFetcher.
  void OnAccountCapabilitiesFetchComplete(
      const CoreAccountId& account_id,
      const std::optional<AccountCapabilities>& account_capabilities);

  image_fetcher::ImageFetcherImpl* GetOrCreateImageFetcher();

  // Called in |OnUserInfoFetchSuccess| after the account info has been fetched.
  void FetchAccountImage(const CoreAccountId& account_id);

  void OnImageFetched(const CoreAccountId& account_id,
                      const std::string& image_url_with_size,
                      const gfx::Image& image,
                      const image_fetcher::RequestMetadata& image_metadata);

  raw_ptr<AccountTrackerService> account_tracker_service_ =
      nullptr;                                                  // Not owned.
  raw_ptr<ProfileOAuth2TokenService> token_service_ = nullptr;  // Not owned.
  raw_ptr<SigninClient> signin_client_ = nullptr;               // Not owned.
  bool network_fetches_enabled_ = false;
  bool network_initialized_ = false;
  bool refresh_tokens_loaded_ = false;
  bool enable_account_removal_for_test_ = false;
  std::unique_ptr<signin::PersistentRepeatingTimer> repeating_timer_;

#if BUILDFLAG(IS_ANDROID)
  CoreAccountId child_request_account_id_;
  std::unique_ptr<ChildAccountInfoFetcherAndroid> child_info_request_;
#endif

  // Holds references to account info fetchers keyed by account_id.
  std::unordered_map<CoreAccountId, std::unique_ptr<AccountInfoFetcher>>
      user_info_requests_;

  std::unique_ptr<AccountCapabilitiesFetcherFactory>
      account_capabilities_fetcher_factory_;
  std::map<CoreAccountId, std::unique_ptr<AccountCapabilitiesFetcher>>
      account_capabilities_requests_;

  // CoreAccountId and the corresponding fetch start time. These two member
  // variables are only used to record account information fetch duration.
  base::flat_map<CoreAccountId, base::TimeTicks> user_info_fetch_start_times_;
  base::flat_map<CoreAccountId, base::TimeTicks> user_avatar_fetch_start_times_;

  // Used for fetching the account images.
  std::unique_ptr<image_fetcher::ImageFetcherImpl> image_fetcher_;
  std::unique_ptr<image_fetcher::ImageDecoder> image_decoder_;

  base::ScopedObservation<ProfileOAuth2TokenService,
                          ProfileOAuth2TokenServiceObserver>
      token_service_observation_{this};

  SEQUENCE_CHECKER(sequence_checker_);
};

#endif  // COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_ACCOUNT_FETCHER_SERVICE_H_
