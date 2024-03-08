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
  PasswordAffiliationSourceAdapter(PasswordStoreInterface* store,
                                   AffiliationSource::Observer* observer);
  ~PasswordAffiliationSourceAdapter() override;

  // AffiliationSource:
  void GetFacets(AffiliationSource::ResultCallback response_callback) override;
  void StartObserving() override;

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

  AffiliationSource::ResultCallback on_password_forms_received_callback_;

  const raw_ptr<PasswordStoreInterface> store_;
  base::ScopedObservation<PasswordStoreInterface,
                          PasswordStoreInterface::Observer>
      scoped_observation_{this};

  const raw_ref<AffiliationSource::Observer> observer_;

  base::WeakPtrFactory<PasswordAffiliationSourceAdapter> weak_ptr_factory_{
      this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_AFFILIATION_PASSWORD_AFFILIATION_SOURCE_ADAPTER_H_
