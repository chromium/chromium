// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_AFFILIATION_AFFILIATED_MATCH_HELPER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_AFFILIATION_AFFILIATED_MATCH_HELPER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "components/password_manager/core/browser/password_form_digest.h"
#include "components/password_manager/core/browser/password_store/password_store_consumer.h"

namespace affiliations {
class AffiliationService;
}  // namespace affiliations

namespace password_manager {

// Interacts with the AffiliationService on behalf of the PasswordStore.
// For each GetLogins() request, it provides the PasswordStore with a list of
// additional realms that are affiliation-based matches to the observed realm.
//
// Currently, the supported use-case is obtaining Android applications or Web
// realms affiliated with the web site containing the observed form. This is
// achieved by implementing the "proactive fetching" strategy for interacting
// with the AffiliationService (see affiliation_service.h for details), with
// Android applications and web realms playing the role of facet Y.
class AffiliatedMatchHelper {
 public:
  // Callback to returns the list of affiliated signon_realms (as per defined in
  // PasswordForm) to the caller.
  using AffiliatedRealmsCallback =
      base::OnceCallback<void(std::vector<std::string> affiliations,
                              std::vector<std::string> groups)>;

  using PSLExtensionCallback =
      base::OnceCallback<void(const base::flat_set<std::string>&)>;

  // The |password_store| must outlive |this|. Both arguments must be non-NULL,
  // except in tests which do not Initialize() the object.
  explicit AffiliatedMatchHelper(
      affiliations::AffiliationService* affiliation_service);
  AffiliatedMatchHelper(const AffiliatedMatchHelper&) = delete;
  AffiliatedMatchHelper& operator=(const AffiliatedMatchHelper&) = delete;
  virtual ~AffiliatedMatchHelper();

  // Retrieves realms of Android applications and Web realms affiliated with the
  // realm of the |observed_form| if it is web-based. Otherwise, yields the
  // empty list. The |result_callback| will be invoked in both cases, on the
  // same thread.
  virtual void GetAffiliatedAndGroupedRealms(
      const PasswordFormDigest& observed_form,
      AffiliatedRealmsCallback result_callback);

  // Retrieves affiliation and branding information about the Android
  // credentials in |forms|, sets |affiliated_web_realm|, |app_display_name| and
  // |app_icon_url| of forms, and invokes |result_callback|.
  virtual void InjectAffiliationAndBrandingInformation(
      LoginsResult forms,
      base::OnceCallback<void(LoginsResultOrError)> result_callback);

  virtual void GetPSLExtensions(PSLExtensionCallback callback);

  // Returns whether or not |form| represents a valid Web credential for the
  // purposes of affiliation-based matching.
  static bool IsValidWebCredential(const PasswordFormDigest& form);

 private:
  // Called back by AffiliationService to supply the list of facets
  // affiliated with the Android credential in |form|. Injects affiliation and
  // branding information by setting |affiliated_web_realm|, |app_display_name|
  // and |app_icon_url| on |form| if |success| is true and |results| is
  // non-empty. Invokes |barrier_closure|.
  void CompleteInjectAffiliationAndBrandingInformation(
      PasswordForm* form,
      base::OnceClosure barrier_closure,
      const affiliations::AffiliatedFacets& results,
      bool success);

  void OnPSLExtensionsReceived(std::vector<std::string> psl_extensions);

  const raw_ptr<affiliations::AffiliationService> affiliation_service_;

  std::optional<base::flat_set<std::string>> psl_extensions_;

  std::vector<PSLExtensionCallback> psl_extensions_callbacks_;

  base::WeakPtrFactory<AffiliatedMatchHelper> weak_ptr_factory_{this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_AFFILIATION_AFFILIATED_MATCH_HELPER_H_
