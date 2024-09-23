// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_AFFILIATION_PASSWORD_AFFILIATION_SOURCE_ADAPTER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_AFFILIATION_PASSWORD_AFFILIATION_SOURCE_ADAPTER_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/affiliations/core/browser/affiliation_source.h"
#include "components/password_manager/core/browser/password_store/password_store_consumer.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"

namespace password_manager {

// This class represents a source for password-related data requiring
// affiliation updates. It utilizes password store information and monitors
// changes to notify observers.
class PasswordAffiliationSourceAdapter
    : public affiliations::AffiliationSource,
      public PasswordStoreInterface::Observer,
      public PasswordStoreConsumer {
 public:
  PasswordAffiliationSourceAdapter();
  ~PasswordAffiliationSourceAdapter() override;

  // AffiliationSource:
  void GetFacets(AffiliationSource::ResultCallback response_callback) override;
  void StartObserving(AffiliationSource::Observer* observer) override;

  // Registers the store to be observed for login changes.
  void RegisterPasswordStore(PasswordStoreInterface* store);

  // Disables fetching facets that require affiliations and stops observing
  // password changes.
  void DisableSource();

 private:
  // PasswordStoreInterface::Observer:
  void OnLoginsChanged(PasswordStoreInterface* store,
                       const PasswordStoreChangeList& changes) override;
  void OnLoginsRetained(
      PasswordStoreInterface* store,
      const std::vector<PasswordForm>& retained_passwords) override;

  // PasswordStoreConsumer:
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<PasswordForm>> results) override;

  // Whether this class should continue fetching passwords.
  bool is_fetching_canceled_ = false;

  AffiliationSource::ResultCallback on_password_forms_received_callback_;

  raw_ptr<PasswordStoreInterface> store_ = nullptr;
  raw_ptr<AffiliationSource::Observer> observer_ = nullptr;

  base::ScopedObservation<PasswordStoreInterface,
                          PasswordStoreInterface::Observer>
      scoped_observation_{this};

  base::WeakPtrFactory<PasswordAffiliationSourceAdapter> weak_ptr_factory_{
      this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_AFFILIATION_PASSWORD_AFFILIATION_SOURCE_ADAPTER_H_
