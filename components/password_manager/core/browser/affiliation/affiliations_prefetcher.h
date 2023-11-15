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
#include "base/scoped_observation.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/password_manager/core/browser/password_store/password_store_consumer.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/webauthn/core/browser/passkey_model.h"

namespace password_manager {

struct PasswordForm;

class AffiliationService;

// This class prefetches affiliation information on start-up for all credentials
// stored in a PasswordStore.
class AffiliationsPrefetcher : public KeyedService,
                               public PasswordStoreInterface::Observer,
                               public webauthn::PasskeyModel::Observer,
                               public PasswordStoreConsumer {
 public:
  explicit AffiliationsPrefetcher(AffiliationService* affiliation_service);
  ~AffiliationsPrefetcher() override;

  // Registers a passkey model and starts listening for passkey changes. Only
  // one passkey model may be registered.
  void RegisterPasskeyModel(webauthn::PasskeyModel* passkey_model);

  void RegisterPasswordStore(PasswordStoreInterface* store);

  // KeyedService:
  void Shutdown() override;

  // Disables affiliations prefetching and clears all the existing cache.
  void DisablePrefetching();

 private:
  // PasswordStoreInterface::Observer:
  void OnLoginsChanged(PasswordStoreInterface* store,
                       const PasswordStoreChangeList& changes) override;
  void OnLoginsRetained(
      PasswordStoreInterface* store,
      const std::vector<PasswordForm>& retained_passwords) override;

  // webauthn::PasskeyModel::Observer:
  void OnPasskeysChanged(
      const std::vector<webauthn::PasskeyModelChange>& changes) override;
  void OnPasskeyModelShuttingDown() override;

  // PasswordStoreConsumer:
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<PasswordForm>> results) override;

  void OnResultFromAllStoresReceived(
      std::vector<std::vector<std::unique_ptr<PasswordForm>>> results);

  void InitializeWithPasswordStores();

  const raw_ptr<AffiliationService> affiliation_service_ = nullptr;

  // Password stores registered via RegisterPasswordStore but aren't observed
  // yet.
  std::vector<raw_ptr<PasswordStoreInterface>> pending_initializations_;

  // Password store which are currently being observed.
  std::vector<raw_ptr<PasswordStoreInterface>> password_stores_;

  // Passkey model being observed. May be null.
  base::ScopedObservation<webauthn::PasskeyModel,
                          webauthn::PasskeyModel::Observer>
      passkey_model_observation_{this};

  // Allows to aggregate GetAllLogins results from multiple stores.
  base::RepeatingCallback<void(std::vector<std::unique_ptr<PasswordForm>>)>
      on_password_forms_received_barrier_callback_;

  // Indicates whether passwords were fetched for all stores in
  // |password_stores_|.
  bool is_ready_ = false;

  // Whether this class should continue prefetching passwords.
  bool is_prefetching_canceled_ = false;

  base::WeakPtrFactory<AffiliationsPrefetcher> weak_ptr_factory_{this};
};

}  // namespace password_manager
#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_AFFILIATION_AFFILIATIONS_PREFETCHER_H_
