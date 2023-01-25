// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_AFFILIATION_AFFILIATIONS_PREFETCHER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_AFFILIATION_AFFILIATIONS_PREFETCHER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "components/password_manager/core/browser/password_store_interface.h"

namespace password_manager {

struct PasswordForm;

class AffiliationService;

// This class prefetches affiliation information on start-up for all credentials
// stored in a PasswordStore.
class AffiliationsPrefetcher : public KeyedService,
                               public PasswordStoreInterface::Observer,
                               public PasswordStoreConsumer {
 public:
  explicit AffiliationsPrefetcher(AffiliationService* affiliation_service);
  ~AffiliationsPrefetcher() override;

  void RegisterPasswordStore(PasswordStoreInterface* store);

  // KeyedService:
  void Shutdown() override;

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

  void OnResultFromAllStoresReceived(
      std::vector<std::vector<std::unique_ptr<PasswordForm>>> results);

  void InitializeWithPasswordStores();

  raw_ptr<AffiliationService, DanglingUntriaged> affiliation_service_ = nullptr;

  // Password stores registered via RegisterPasswordStore but aren't observed
  // yet.
  std::vector<raw_ptr<PasswordStoreInterface>> pending_initializations_;

  // Password store which are currently being observed.
  std::vector<raw_ptr<PasswordStoreInterface>> password_stores_;

  // Allows to aggregate GetAllLogins results from multiple stores.
  base::RepeatingCallback<void(std::vector<std::unique_ptr<PasswordForm>>)>
      on_password_forms_received_barrier_callback_;

  // Indicates whether passwords were fetched for all stores in
  // |password_stores_|.
  bool is_ready_ = false;

  base::WeakPtrFactory<AffiliationsPrefetcher> weak_ptr_factory_{this};
};

}  // namespace password_manager
#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_AFFILIATION_AFFILIATIONS_PREFETCHER_H_
