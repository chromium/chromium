// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_SCRIPTS_FETCHER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_SCRIPTS_FETCHER_H_

#include "base/callback_forward.h"
#include "base/values.h"
#include "components/keyed_service/core/keyed_service.h"

namespace url {
class Origin;
}

namespace password_manager {

// Abstract interface to fetch the list of password scripts.
class PasswordScriptsFetcher : public KeyedService {
 public:
  using ResponseCallback = base::OnceCallback<void(bool)>;
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class CacheState {
    // Cache is ready.
    kReady = 0,
    // Cache was set but it is stale. Re-fetch needed.
    kStale = 1,
    // Cache was never set,
    kNeverSet = 2,
    // Cache is waiting for an in-flight request.
    kWaiting = 3,
    kMaxValue = kWaiting,
  };
  // Triggers pre-fetching the list of scripts. Should be called from UI
  // preceding Bulk Check.
  virtual void PrewarmCache() = 0;
  // Triggers fetching if the cache was never set or is stale (but doesn't
  // trigger a duplicate request if another request is in-flight). Otherwise, it
  // runs the callback immediately (the expected reaction as a call of
  // |PrewarmCache| was supposed to fetch the data in advance). In case of
  // several calls of the method, the callbacks will be called one after
  // another.
  virtual void RefreshScriptsIfNecessary(
      base::OnceClosure fetch_finished_callback) = 0;
  // Returns whether there is a password change script for |origin| via
  // |callback|. If the cache was never set or is stale, it triggers a re-fetch.
  // In case of a network error, the verdict will default to no script being
  // available.
  virtual void FetchScriptAvailability(const url::Origin& origin,
                                       ResponseCallback callback) = 0;
  // Immediately returns whether there is a password change script for |origin|.
  // The method does NOT trigger any network requests if the cache is not ready
  // or stale but reads the current state of the cache.
  // In case of a network error while fetching the scripts, the result will
  // always be false.
  // TODO(crbug.com/1086114): It is better to deprecate this method and always
  // use |FetchScriptAvailability| instead because |IsScriptAvailable| may
  // return stale data.
  virtual bool IsScriptAvailable(const url::Origin& origin) const = 0;

  // Returns if the cache is not currently ready and a refresh is needed (and
  // possibly currently ongoing).
  virtual bool IsCacheStale() const = 0;

  // Return high-level state summary of the PasswordScriptsFetcher in form
  // of a `base::Value::Dict` for display on chrome://apc-internals.
  virtual base::Value::Dict GetDebugInformationForInternals() const = 0;

  // Return a list of all entries currently held in the cache.
  virtual base::Value::List GetCacheEntries() const = 0;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_SCRIPTS_FETCHER_H_
