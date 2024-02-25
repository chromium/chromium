// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_TAILORED_SECURITY_SERVICE_TAILORED_SECURITY_SERVICE_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_TAILORED_SECURITY_SERVICE_TAILORED_SECURITY_SERVICE_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
}

namespace signin {
class IdentityManager;
}

namespace syncer {
class SyncService;
}

namespace safe_browsing {

class TailoredSecurityServiceObserver;

// Provides an API for querying Google servers for a user's tailored security
// account Opt-In.
class TailoredSecurityService : public KeyedService {
 public:
  // Handles all the work of making an API request. This class encapsulates
  // the entire state of the request. When an instance is destroyed, all
  // aspects of the request are cancelled.
  class Request {
   public:
    virtual ~Request();

    Request(const Request&) = delete;
    Request& operator=(const Request&) = delete;

    // Returns true if the request is "pending" (i.e., it has been started, but
    // is not yet completed).
    virtual bool IsPending() const = 0;

    // Returns the response code received from the server, which will only be
    // valid if the request succeeded.
    virtual int GetResponseCode() const = 0;

    // Returns the contents of the response body received from the server.
    virtual const std::string& GetResponseBody() const = 0;

    virtual void SetPostData(const std::string& post_data) = 0;

    // Tells the request to begin.
    virtual void Start() = 0;

    virtual void Shutdown() = 0;

   protected:
    Request();
  };

  using QueryTailoredSecurityBitCallback =
      base::OnceCallback<void(bool is_enabled, base::Time previous_update)>;

  using CompletionCallback = base::OnceCallback<void(Request*, bool success)>;

  TailoredSecurityService(signin::IdentityManager* identity_manager,
                          syncer::SyncService* sync_service,
                          PrefService* prefs);
  ~TailoredSecurityService() override;

  void AddObserver(TailoredSecurityServiceObserver* observer);
  void RemoveObserver(TailoredSecurityServiceObserver* observer);

  // Called to increment/decrement |active_query_request_|. When
  // |active_query_request_| goes from zero to nonzero, we begin querying the
  // tailored security setting. When it goes from nonzero to zero, we stop
  // querying the tailored security setting. Virtual for tests. Returns a
  // boolean value for if a query was added successfully.
  virtual bool AddQueryRequest();
  virtual void RemoveQueryRequest();

  // Queries whether TailoredSecurity is enabled on the server.
  void QueryTailoredSecurityBit();

  // Starts the request to send to the backend to retrieve the bit.
  void StartRequest(QueryTailoredSecurityBitCallback callback);

  // Sets the state of tailored security bit to |is_enabled| for testing.
  void SetTailoredSecurityBitForTesting(
      bool is_enabled,
      QueryTailoredSecurityBitCallback callback,
      const net::NetworkTrafficAnnotationTag& traffic_annotation);

  // Enables/disables sending queries to the tailored security API. If any
  // queries are issued while querying is disabled, the most recent query will
  // be stored to be ran when querying is re-enabled. On iOS,
  // TailoredSecurityTabHelper uses this method to stop querying when the app is
  // backgrounded.
  void SetCanQuery(bool can_query);

  // KeyedService implementation:
  void Shutdown() override;

 protected:
  // Callback when the `kAccountTailoredSecurityUpdateTimestamp` is updated
  virtual void TailoredSecurityTimestampUpdateCallback();

  // This function is pulled out for testing purposes. Caller takes ownership of
  // the new Request.
  virtual std::unique_ptr<Request> CreateRequest(
      const GURL& url,
      CompletionCallback callback,
      const net::NetworkTrafficAnnotationTag& traffic_annotation);

  // Used for tests.
  size_t GetNumberOfPendingTailoredSecurityServiceRequests();

  // Extracts a JSON-encoded HTTP response into a dictionary.
  static base::Value::Dict ReadResponse(Request* request);

  // Unpacks the response and calls `callback`. Called by a `Request` when a
  // tailored security service query sequence has completed. When `success` is
  // `true`, the method will try to extract the Tailored Security bit value
  // from the request and run `callback`; when `false` the method performs error
  // handling.
  void ExtractTailoredSecurityBitFromResponseAndRunCallback(
      QueryTailoredSecurityBitCallback callback,
      Request* request,
      bool success);

  // Called with whether the tailored security setting `is_enabled` and the
  // timestamp of the most recent update (excluding the current update in
  // progress).
  void OnTailoredSecurityBitRetrieved(bool is_enabled,
                                      base::Time previous_update);

  // After `kAccountTailoredSecurityUpdateTimestamp` is updated, we check the
  // true value of the account tailored security preference and run this
  // callback.
  virtual void MaybeNotifySyncUser(bool is_enabled, base::Time previous_update);

  // Returns whether the user has history sync enabled in preferences.
  bool HistorySyncEnabledForUser();

  PrefService* prefs() { return prefs_; }

  signin::IdentityManager* identity_manager() { return identity_manager_; }

  virtual scoped_refptr<network::SharedURLLoaderFactory>
  GetURLLoaderFactory() = 0;

 private:
  FRIEND_TEST_ALL_PREFIXES(
      TailoredSecurityServiceTest,
      HistorySyncEnabledForUserReturnsFalseWhenSyncServiceIsNull);
  FRIEND_TEST_ALL_PREFIXES(
      TailoredSecurityServiceTest,
      RetryEnabledTimestampUpdateCallbackSetsStateToRetryNeeded);
  FRIEND_TEST_ALL_PREFIXES(TailoredSecurityServiceTest,
                           RetryEnabledTimestampUpdateCallbackRecordsStartTime);
  FRIEND_TEST_ALL_PREFIXES(
      TailoredSecurityServiceTest,
      RetryDisabledTimestampUpdateCallbackDoesNotRecordStartTime);
  FRIEND_TEST_ALL_PREFIXES(TailoredSecurityServiceTest,
                           RetryDisabledStateRemainsUnset);
  friend class TailoredSecurityTabHelperTest;

  // Saves the supplied `TailoredSecurityRetryState` to preferences.
  void SaveRetryState(TailoredSecurityRetryState state);

  // Stores pointer to `IdentityManager` instance. It must outlive the
  // `TailoredSecurityService` and can be null during tests.
  raw_ptr<signin::IdentityManager> identity_manager_;

  // Stores pointer to `SyncService` instance. It must outlive the
  // `TailoredSecurityService` and can be null during tests.
  raw_ptr<syncer::SyncService> sync_service_;

  // Pending TailoredSecurity queries to be canceled if not complete by
  // profile shutdown.
  std::map<Request*, std::unique_ptr<Request>>
      pending_tailored_security_requests_;

  // Observers.
  base::ObserverList<TailoredSecurityServiceObserver, true>::Unchecked
      observer_list_;

  // The number of active query requests. When this goes from non-zero to zero,
  // we stop `timer_`. When it goes from zero to non-zero, we start it.
  size_t active_query_request_ = 0;

  // Timer to periodically check tailored security bit.
  base::OneShotTimer timer_;

  bool is_tailored_security_enabled_ = false;
  base::Time last_updated_;

  bool is_shut_down_ = false;

  // Allows querying and requests to start. On iOS platforms, this is used to
  // ensure that requests aren't made when the app is backgrounded.
  bool can_query_ = true;

  // Used to store and call the most recent callback request when querying is
  // disabled.
  QueryTailoredSecurityBitCallback saved_callback_;

  // The preferences for the given profile.
  raw_ptr<PrefService> prefs_;

  // This is used to observe when sync users update their Tailored Security
  // setting.
  PrefChangeRegistrar pref_registrar_;

  // Callback run when we should notify a sync user about a state change.
  base::RepeatingCallback<void(bool)> notify_sync_user_callback_;

  base::WeakPtrFactory<TailoredSecurityService> weak_ptr_factory_{this};
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_TAILORED_SECURITY_SERVICE_TAILORED_SECURITY_SERVICE_H_
