// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UNIFIED_CONSENT_URL_KEYED_DATA_COLLECTION_CONSENT_HELPER_H_
#define COMPONENTS_UNIFIED_CONSENT_URL_KEYED_DATA_COLLECTION_CONSENT_HELPER_H_

#include <memory>

#include "base/observer_list.h"

class PrefService;
namespace syncer {
class SyncService;
}

namespace unified_consent {

// Helper class that allows clients to check whether the user has consented
// for URL-keyed data collection.
class UrlKeyedDataCollectionConsentHelper {
 public:
  enum class State {
    kInitializing,
    kDisabled,
    kEnabled,
  };

  class Observer {
   public:
    // Called when the state of the URL-keyed data collection changes.
    virtual void OnUrlKeyedDataCollectionConsentStateChanged(
        UrlKeyedDataCollectionConsentHelper* consent_helper) = 0;
  };

  // Creates a new |UrlKeyedDataCollectionConsentHelper| instance that checks
  // whether *anonymized* data collection is enabled. This should be used when
  // the client needs to check whether the user has granted consent for
  // *anonymized* URL-keyed data collection. It is enabled if the preference
  // |prefs::kUrlKeyedAnonymizedDataCollectionEnabled| from |pref_service| is
  // set to true.
  //
  // Note: |pref_service| must outlive the returned instance.
  static std::unique_ptr<UrlKeyedDataCollectionConsentHelper>
  NewAnonymizedDataCollectionConsentHelper(PrefService* pref_service);

  // Creates a new |UrlKeyedDataCollectionConsentHelper| instance that checks
  // whether *personalized* data collection is enabled. This should be used when
  // the client needs to check whether the user has granted consent for
  // URL-keyed data collection keyed by their Google account.
  //
  // Implementation-wise URL-keyed data collection is enabled if history sync
  // has an active upload state.
  static std::unique_ptr<UrlKeyedDataCollectionConsentHelper>
  NewPersonalizedDataCollectionConsentHelper(syncer::SyncService* sync_service);

  // Creates a new |UrlKeyedDataCollectionConsentHelper| instance that checks
  // whether *bookmarks* data collection is enabled. This should be used when
  // the client needs to check whether the user has granted consent for
  // bookmarks data collection keyed by their Google account.
  // TODO(crbug.com/40067025): Remove the `require_sync_feature_enabled` param
  // once kReplaceSyncPromosWithSignInPromos is launched.
  static std::unique_ptr<UrlKeyedDataCollectionConsentHelper>
  NewPersonalizedBookmarksDataCollectionConsentHelper(
      syncer::SyncService* sync_service,
      bool require_sync_feature_enabled);

  UrlKeyedDataCollectionConsentHelper(
      const UrlKeyedDataCollectionConsentHelper&) = delete;
  UrlKeyedDataCollectionConsentHelper& operator=(
      const UrlKeyedDataCollectionConsentHelper&) = delete;

  virtual ~UrlKeyedDataCollectionConsentHelper();

  // Returns the state of the consent helper. To throttle requests until after
  // initialization, use the `ConsentThrottle` class.
  virtual State GetConsentState() = 0;

  // Returns true if the user has consented for URL keyed anonymized data
  // collection. Note, this is a simplified form of `GetConsentState()` where
  // kInitializing and kDisabled are both considered NOT enabled.
  bool IsEnabled();

  // Methods to register or remove observers.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  UrlKeyedDataCollectionConsentHelper();

  // Fires |OnUrlKeyedDataCollectionConsentStateChanged| on all the observers.
  void FireOnStateChanged();

 private:
  base::ObserverList<Observer, true>::Unchecked observer_list_;
};

}  // namespace unified_consent

#endif  // COMPONENTS_UNIFIED_CONSENT_URL_KEYED_DATA_COLLECTION_CONSENT_HELPER_H_
