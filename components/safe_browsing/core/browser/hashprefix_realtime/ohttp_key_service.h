// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_HASHPREFIX_REALTIME_OHTTP_KEY_SERVICE_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_HASHPREFIX_REALTIME_OHTTP_KEY_SERVICE_H_

#include <optional>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"

class PrefService;

namespace net {
class HttpResponseHeaders;
}  // namespace net

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace safe_browsing {

class BackoffOperator;

// This class is responsible for managing the public key for sending Oblivious
// HTTP requests in hash real time lookup service.
class OhttpKeyService : public KeyedService {
 public:
  using Callback =
      base::OnceCallback<void(std::optional<std::string> ohttp_key)>;

  struct OhttpKeyAndExpiration {
    // The OHTTP key in this struct is formatted as described in
    // https://www.ietf.org/archive/id/draft-ietf-ohai-ohttp-02.html#name-key-configuration-encoding
    std::string key;
    base::Time expiration;
  };

  // The reason that a key fetch is triggered.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class FetchTriggerReason {
    // The key fetch is triggered during hash real-time lookup because there is
    // no available cached key.
    kDuringHashRealTimeLookup = 0,
    // The key fetch is triggered asynchronously by background scheduler.
    kAsyncFetch = 1,
    // The key fetch is triggered because the response from real-time lookup
    // contains key related error code.
    kKeyRelatedHttpErrorCode = 2,
    // The key fetch is triggered because the response from real-time lookup
    // contains key rotated header.
    kKeyRotatedHeader = 3,
    kMaxValue = kKeyRotatedHeader
  };

  OhttpKeyService(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      PrefService* pref_service,
      PrefService* local_state,
      base::RepeatingCallback<std::optional<std::string>()> country_getter);

  OhttpKeyService(const OhttpKeyService&) = delete;
  OhttpKeyService& operator=(const OhttpKeyService&) = delete;

  ~OhttpKeyService() override;

  // Gets an OHTTP key for encryption. It may or may not trigger a real time key
  // fetch, depending on whether there is a valid key already cached in memory
  // or there is an in-progress request triggered by other callers.
  // The key will be returned via |callback|. The callback runs with a nullopt
  // if the service cannot provide a valid key at the moment. Callers should
  // ensure |callback| is still valid when it is run. This function is
  // overridden in tests.
  virtual void GetOhttpKey(Callback callback);

  // Notifies the key service with the response from the lookup request. |key|
  // is used for the lookup request, |response_code| and |headers| are returned
  // from the lookup server. It may trigger a key fetch if the response contains
  // key related error or header. This function is overridden in tests.
  virtual void NotifyLookupResponse(
      const std::string& key,
      int response_code,
      scoped_refptr<net::HttpResponseHeaders> headers);

  // KeyedService:
  // Called before the actual deletion of the object.
  void Shutdown() override;

  void set_ohttp_key_for_testing(OhttpKeyAndExpiration ohttp_key);
  std::optional<OhttpKeyAndExpiration> get_ohttp_key_for_testing();

 private:
  // Listens to prefs changes that configure enabling/disabling the service.
  void OnConfiguringPrefsChanged();

  // Enables/disables the service.
  void SetEnabled(bool enable);

  // Starts to fetch a new key from the Safe Browsing key hosting endpoint. It
  // may be triggered by sync (|GetOhttpKey|) or async (|MaybeStartAsyncFetch|)
  // workflows. |trigger_reason| is used for logging metrics.
  void StartFetch(Callback callback, FetchTriggerReason trigger_reason);

  // Called when the response from the Safe Browsing key hosting endpoint is
  // received.
  void OnURLLoaderComplete(base::TimeTicks request_start_time,
                           std::unique_ptr<std::string> response_body);

  // Async workflow:
  // Starts to fetch a new key if the current key is close to expiration.
  // Otherwise, reschedule to check again in an hour.
  void MaybeStartOrRescheduleAsyncFetch();
  // Called when the async fetch is completed. This function schedules the next
  // async fetch based on the fetch result. Note that it does not use the
  // |ohttp_key| parameter because |ohttp_key_| already gets populated to it
  // when relevant before this method is called.
  void OnAsyncFetchCompleted(std::optional<std::string> ohttp_key);
  // Returns if async fetch should be started immediately, which is if the
  // |ohttp_key_| is unpopulated, is expired, or will soon expire.
  bool ShouldStartAsyncFetch();

  // Server triggered workflow:
  // Starts a key fetch if the |previous_key| is different from |ohttp_key_| or
  // the |ohttp_key_| is empty. |trigger_reason| is used for logging metrics.
  void MaybeStartServerTriggeredFetch(std::string previous_key,
                                      FetchTriggerReason trigger_reason);

  // Pref functions:
  // Gets the key and expiration time from pref. If there is an unexpired key,
  // populate it into |ohttp_key_|.
  void PopulateKeyFromPref();
  // Sets the current |ohttp_key_| into pref. Skip if there is no valid
  // |ohttp_key_|.
  void StoreKeyToPref();

  // The URLLoaderFactory we use to issue a network request.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  // |url_loader_| is not null iff there is a network request in progress.
  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  // All callbacks that have requested an OHTTP key but haven't received a
  // response yet.
  base::OnceCallbackList<Callback::RunType> pending_callbacks_;

  // The key cached in memory.
  std::optional<OhttpKeyAndExpiration> ohttp_key_;

  // Unowned object used for synchronizing the OHTTP key between the prefs and
  // the OHTTP key service.
  raw_ptr<PrefService> pref_service_;

  // Observes changes to profile prefs that configure whether the service is
  // enabled.
  PrefChangeRegistrar pref_change_registrar_;

  // Observes changes to local state prefs that configure whether the service is
  // enabled.
  PrefChangeRegistrar local_state_pref_change_registrar_;

  // Keeps track of the state of the service. It's enabled when standard
  // protection is on and the policy kHashPrefixRealTimeChecksAllowedByPolicy
  // isn't disabled.
  bool enabled_ = false;

  // Used to schedule async key fetch.
  base::OneShotTimer async_fetch_timer_;

  // Set to true when a server-triggered fetch is scheduled. Set to false on
  // |StartServerTriggeredFetch| called.
  bool server_triggered_fetch_scheduled_ = false;

  // Helper object that manages backoff state.
  std::unique_ptr<BackoffOperator> backoff_operator_;

  // Callback used to help determine if the service should be enabled.
  base::RepeatingCallback<std::optional<std::string>()> country_getter_;

  // Indicates whether a lookup response has been received using the current
  // |ohttp_key_|. Set to false when a new key is obtained. Set back to true
  // when the first response is received using this key. Used for logging
  // metrics.
  bool has_received_lookup_response_from_current_key_ = true;

  base::WeakPtrFactory<OhttpKeyService> weak_factory_{this};
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_HASHPREFIX_REALTIME_OHTTP_KEY_SERVICE_H_
