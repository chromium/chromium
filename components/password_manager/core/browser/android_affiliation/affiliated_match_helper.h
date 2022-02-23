// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ANDROID_AFFILIATION_AFFILIATED_MATCH_HELPER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ANDROID_AFFILIATION_AFFILIATED_MATCH_HELPER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "components/password_manager/core/browser/password_store_interface.h"
#include "components/password_manager/core/browser/site_affiliation/affiliation_service.h"

namespace password_manager {

struct PasswordForm;

// Interacts with the AffiliationService on behalf of the PasswordStore.
// For each GetLogins() request, it provides the PasswordStore with a list of
// additional realms that are affiliation-based matches to the observed realm.
//
// Currently, the supported use-case is obtaining Android applications or Web
// realms affiliated with the web site containing the observed form. This is
// achieved by implementing the "proactive fetching" strategy for interacting
// with the AffiliationService (see affiliation_service.h for details), with
// Android applications and web realms playing the role of facet Y.
//
// More specifically, this class prefetches affiliation information on start-up
// for all credentials stored in the PasswordStore. Then, the actual GetLogins()
// can be restricted to the cache, so that realms of the observed web forms will
// never be looked up against the Affiliation API.
class AffiliatedMatchHelper : public PasswordStoreInterface::Observer,
                              public PasswordStoreConsumer {
 public:
  // Callback to returns the list of affiliated signon_realms (as per defined in
  // PasswordForm) to the caller.
  using AffiliatedRealmsCallback =
      base::OnceCallback<void(const std::vector<std::string>&)>;

  // The |password_store| must outlive |this|. Both arguments must be non-NULL,
  // except in tests which do not Initialize() the object.
  explicit AffiliatedMatchHelper(AffiliationService* affiliation_service);
  AffiliatedMatchHelper(const AffiliatedMatchHelper&) = delete;
  AffiliatedMatchHelper& operator=(const AffiliatedMatchHelper&) = delete;
  ~AffiliatedMatchHelper() override;

  // Schedules deferred initialization.
  void Initialize(PasswordStoreInterface* password_store);

  // Retrieves realms of Android applications and Web realms affiliated with the
  // realm of the |observed_form| if it is web-based. Otherwise, yields the
  // empty list. The |result_callback| will be invoked in both cases, on the
  // same thread.
  virtual void GetAffiliatedAndroidAndWebRealms(
      const PasswordFormDigest& observed_form,
      AffiliatedRealmsCallback result_callback);

  // Returns whether or not |form| represents a valid Web credential for the
  // purposes of affiliation-based matching.
  static bool IsValidWebCredential(const PasswordFormDigest& form);

  // I/O heavy initialization on start-up will be delayed by this long.
  // This should be high enough not to exacerbate start-up I/O contention too
  // much, but also low enough that the user be able log-in shortly after
  // browser start-up into web sites using Android credentials.
  // TODO(engedy): See if we can tie this instead to some meaningful event.
  static constexpr base::TimeDelta kInitializationDelayOnStartup =
      base::Seconds(30);

  AffiliationService* get_affiliation_service() { return affiliation_service_; }

 private:
  // Reads all autofillable credentials from the password store and starts
  // observing the store for future changes.
  void DoDeferredInitialization();

  // Called back by AffiliationService to supply the list of facets
  // affiliated with |original_facet_uri| so that a GetAffiliatedAndroidRealms()
  // call can be completed.
  void CompleteGetAffiliatedAndroidAndWebRealms(
      const FacetURI& original_facet_uri,
      AffiliatedRealmsCallback result_callback,
      const AffiliatedFacets& results,
      bool success);

  // PasswordStoreInterface::Observer:
  void OnLoginsChanged(PasswordStoreInterface* store,
                       const PasswordStoreChangeList& changes) override;
  void OnLoginsRetained(
      PasswordStoreInterface* store,
      const std::vector<PasswordForm>& retained_passwords) override;

  // PasswordStoreConsumer:
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<PasswordForm>> results) override;

  raw_ptr<PasswordStoreInterface> password_store_ = nullptr;

  raw_ptr<AffiliationService> affiliation_service_;

  base::WeakPtrFactory<AffiliatedMatchHelper> weak_ptr_factory_{this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ANDROID_AFFILIATION_AFFILIATED_MATCH_HELPER_H_
