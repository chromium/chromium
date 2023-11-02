// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_REQUIREMENTS_SERVICE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_REQUIREMENTS_SERVICE_H_

#include <memory>
#include <utility>

#include "base/containers/lru_cache.h"
#include "components/autofill/core/browser/proto/password_requirements.pb.h"
#include "components/autofill/core/common/signatures.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/password_manager/core/browser/generation/password_requirements_spec_fetcher.h"
#include "url/gurl.h"

namespace autofill {
class PasswordRequirementsSpec;
}

namespace network {
class SharedURLLoaderFactory;
}

namespace password_manager {

// A service that fetches, stores and returns requirements for generating a
// random password on a specific form and site.
class PasswordRequirementsService : public KeyedService {
 public:
  // If |fetcher| is a nullptr, no network requests happen.
  explicit PasswordRequirementsService(
      std::unique_ptr<autofill::PasswordRequirementsSpecFetcher> fetcher);

  PasswordRequirementsService(const PasswordRequirementsService&) = delete;
  PasswordRequirementsService& operator=(const PasswordRequirementsService&) =
      delete;

  ~PasswordRequirementsService() override;

  // Returns the password requirements for a field that appears on a site
  // with domain |main_frame_domain| and has the specified |form_signature|
  // and |field_signature|.
  //
  // This function returns synchronously and only returns results if these
  // have been retrieved via the Add/Prefetch methods below and the data is
  // still in the cache.
  autofill::PasswordRequirementsSpec GetSpec(
      const GURL& main_frame_domain,
      autofill::FormSignature form_signature,
      autofill::FieldSignature field_signature);

  // Triggers a fetch for password requirements for the domain passed in
  // |main_frame_domain| and stores it into the MRU cache.
  void PrefetchSpec(const GURL& main_frame_domain);

  // Stores the password requirements for |main_frame_domain| and for the field
  // identified via |form_signature| and |field_signature| in the MRU caches.
  void AddSpec(const GURL& main_frame_domain,
               autofill::FormSignature form_signature,
               autofill::FieldSignature field_signature,
               const autofill::PasswordRequirementsSpec& spec);

#if defined(UNIT_TEST)
  // Wipes MRU cached data to ensure that it gets fetched again.
  // This style of delegation is used because UNIT_TEST is only available in
  // header files as per presubmit checks.
  void ClearDataForTesting() { ClearDataForTestingImpl(); }
#endif

 private:
  void OnFetchedRequirements(const GURL& main_frame_domain,
                             const autofill::PasswordRequirementsSpec& spec);
  void ClearDataForTestingImpl();

  using FullSignature =
      std::pair<autofill::FormSignature, autofill::FieldSignature>;
  base::LRUCache<GURL, autofill::PasswordRequirementsSpec> specs_for_domains_;
  base::LRUCache<FullSignature, autofill::PasswordRequirementsSpec>
      specs_for_signatures_;
  // May be a nullptr.
  std::unique_ptr<autofill::PasswordRequirementsSpecFetcher> fetcher_;
};

std::unique_ptr<PasswordRequirementsService> CreatePasswordRequirementsService(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_REQUIREMENTS_SERVICE_H_
