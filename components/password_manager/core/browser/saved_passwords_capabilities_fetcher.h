// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SAVED_PASSWORDS_CAPABILITIES_FETCHER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SAVED_PASSWORDS_CAPABILITIES_FETCHER_H_

#include <memory>
#include <set>

#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/capabilities_service.h"
#include "components/password_manager/core/browser/password_scripts_fetcher.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"

namespace password_manager {

class SavedPasswordsCapabilitiesFetcher
    : public PasswordScriptsFetcher,
      public SavedPasswordsPresenter::Observer {
 public:
  struct CapabilitiesFetchResult {
    CapabilitiesFetchResult();
    ~CapabilitiesFetchResult();

    bool has_script = false;
    base::TimeTicks last_fetch_timestamp;

    bool IsResultStale() const;
  };

  SavedPasswordsCapabilitiesFetcher(
      std::unique_ptr<CapabilitiesService> fetcher,
      scoped_refptr<password_manager::PasswordStoreInterface> password_store);

  SavedPasswordsCapabilitiesFetcher(const SavedPasswordsCapabilitiesFetcher&) =
      delete;
  SavedPasswordsCapabilitiesFetcher& operator=(
      const SavedPasswordsCapabilitiesFetcher&) = delete;

  ~SavedPasswordsCapabilitiesFetcher() override;

  // PasswordScriptsFetcher:
  void PrewarmCache() override;
  void RefreshScriptsIfNecessary(
      base::OnceClosure fetch_finished_callback) override;
  void FetchScriptAvailability(const url::Origin& origin,
                               ResponseCallback callback) override;
  bool IsScriptAvailable(const url::Origin& origin) const override;
  bool IsCacheStale() const override;
  base::Value::Dict GetDebugInformationForInternals() const override;
  base::Value::List GetCacheEntries() const override;

 private:
  // SavedPasswordsPresenter::Observer:
  void OnEdited(const PasswordForm& form) override;
  void OnSavedPasswordsChanged(
      SavedPasswordsPresenter::SavedPasswordsView passwords) override;

  // Fetches capabilities for all origins in cache.
  void FetchCapababilitiesForAllStoredOrigins();

  // Callback to process refresh capabilities request.
  void FetchCapababilitiesForAllStoredOriginsDone(
      base::TimeTicks request_start_timestamp,
      const std::set<url::Origin>& capabilities);

  // Fetches capabilities info for a single origin.
  void FetchCapababilitiesForSingleOrigin(const url::Origin& origin,
                                          ResponseCallback callback);

  // Callback to process `FetchCapababilitiesForSingleOrigin` request.
  void FetchCapababilitiesForSingleOriginDone(
      const url::Origin& origin,
      ResponseCallback callback,
      base::TimeTicks request_start_timestamp,
      const std::set<url::Origin>& capabilities);

  // Returns a sorted list of unique origins of all stored credentials.
  std::vector<url::Origin> GetOriginsOfStoredPasswords() const;

  // Resets the `cache_` with the current list of origins from stored
  // credentials.
  void ReinitializeCache();

  // Returns the state of the cache.
  CacheState GetCacheState() const;

  std::unique_ptr<CapabilitiesService> fetcher_;

  // Manages the list of saved passwords, including updates.
  password_manager::SavedPasswordsPresenter saved_passwords_presenter_;

  // Stores the callbacks that are waiting for the refresh capabilities request
  // to finish.
  std::vector<base::OnceClosure> all_origins_callbacks_;

  // Stores the single origin callbacks that are waiting for the capabilities
  // refresh request to finish.
  std::vector<std::pair<url::Origin, ResponseCallback>>
      single_origin_callbacks_;

  // Stores script capabilities and last request timestamp per origin.
  std::map<url::Origin, CapabilitiesFetchResult> cache_;

  // Boolean that remembers whether `cache_` is initialized. This is set to
  // true when the delegate obtains the list of saved passwords for the first
  // time.
  bool is_cache_initialized_ = false;

  // Boolean that remembers whether there is a refresh capabilities request in
  // process.
  bool refresh_in_process_ = false;

  // Boolean that remembers whether cache should be refreshed (e.g due to an new
  // password added to the store or a prewarming request before initialization).
  bool should_refresh_cache_ = false;

  // A scoped observer for `saved_passwords_presenter_`.
  base::ScopedObservation<password_manager::SavedPasswordsPresenter,
                          password_manager::SavedPasswordsPresenter::Observer>
      observed_saved_password_presenter_{this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SAVED_PASSWORDS_CAPABILITIES_FETCHER_H_
