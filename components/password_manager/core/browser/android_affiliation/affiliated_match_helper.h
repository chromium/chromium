// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ANDROID_AFFILIATION_AFFILIATED_MATCH_HELPER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ANDROID_AFFILIATION_AFFILIATED_MATCH_HELPER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/android_affiliation/android_affiliation_service.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/password_store_consumer.h"

namespace password_manager {

class AndroidAffiliationService;
struct PasswordForm;

// Interacts with the AndroidAffiliationService on behalf of the PasswordStore.
// For each GetLogins() request, it provides the PasswordStore with a list of
// additional realms that are affiliation-based matches to the observed realm.
//
// Currently, the supported use-case is obtaining Android applications or Web
// realms affiliated with the web site containing the observed form. This is
// achieved by implementing the "proactive fetching" strategy for interacting
// with the AndroidAffiliationService (see android_affiliation_service.h for
// details), with Android applications and web realms playing the role of
// facet Y.
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

  using PasswordFormsCallback =
      base::OnceCallback<void(std::vector<std::unique_ptr<PasswordForm>>)>;

  // The |password_store| must outlive |this|. Both arguments must be non-NULL,
  // except in tests which do not Initialize() the object.
  AffiliatedMatchHelper(
      PasswordStore* password_store,
      std::unique_ptr<AndroidAffiliationService> affiliation_service);
  ~AffiliatedMatchHelper() override;

  // Schedules deferred initialization.
  void Initialize();

  // Retrieves realms of Android applications and Web realms affiliated with the
  // realm of the |observed_form| if it is web-based. Otherwise, yields the
  // empty list. The |result_callback| will be invoked in both cases, on the
  // same thread.
  virtual void GetAffiliatedAndroidAndWebRealms(
      const PasswordFormDigest& observed_form,
      AffiliatedRealmsCallback result_callback);

  // Retrieves affiliation and branding information about the Android
  // credentials in |forms|, sets |affiliated_web_realm|, |app_display_name| and
  // |app_icon_url| of forms, and invokes |result_callback|.
  // NOTE: When |strategy_on_cache_miss| is set to |FAIL|, this will not issue
  // an on-demand network request. And if a request to cache fails, no
  // affiliation and branding information will be injected into corresponding
  // form.
  virtual void InjectAffiliationAndBrandingInformation(
      std::vector<std::unique_ptr<PasswordForm>> forms,
      AndroidAffiliationService::StrategyOnCacheMiss strategy_on_cache_miss,
      PasswordFormsCallback result_callback);

  // Returns whether or not |form| represents an Android credential.
  static bool IsValidAndroidCredential(const PasswordFormDigest& form);

  // Returns whether or not |form| represents a valid Web credential for the
  // purposes of affiliation-based matching.
  static bool IsValidWebCredential(const PasswordFormDigest& form);

  // I/O heavy initialization on start-up will be delayed by this long.
  // This should be high enough not to exacerbate start-up I/O contention too
  // much, but also low enough that the user be able log-in shortly after
  // browser start-up into web sites using Android credentials.
  // TODO(engedy): See if we can tie this instead to some meaningful event.
  static constexpr base::TimeDelta kInitializationDelayOnStartup =
      base::TimeDelta::FromSeconds(8);

 private:
  // Reads all autofillable credentials from the password store and starts
  // observing the store for future changes.
  void DoDeferredInitialization();

  // Called back by AndroidAffiliationService to supply the list of facets
  // affiliated with |original_facet_uri| so that a GetAffiliatedAndroidRealms()
  // call can be completed.
  void CompleteGetAffiliatedAndroidAndWebRealms(
      const FacetURI& original_facet_uri,
      AffiliatedRealmsCallback result_callback,
      const AffiliatedFacets& results,
      bool success);

  // Called back by AndroidAffiliationService to supply the list of facets
  // affiliated with the Android credential in |form|. Injects affiliation and
  // branding information by setting |affiliated_web_realm|, |app_display_name|
  // and |app_icon_url| on |form| if |success| is true and |results| is
  // non-empty. Invokes |barrier_closure|.
  void CompleteInjectAffiliationAndBrandingInformation(
      PasswordForm* form,
      base::OnceClosure barrier_closure,
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

  PasswordStore* const password_store_;

  // Being the sole consumer of AndroidAffiliationService, |this| owns the
  // service.
  std::unique_ptr<AndroidAffiliationService> affiliation_service_;

  base::WeakPtrFactory<AffiliatedMatchHelper> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AffiliatedMatchHelper);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ANDROID_AFFILIATION_AFFILIATED_MATCH_HELPER_H_
