// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_CORE_BROWSER_GRAMMATICAL_GENDER_SERVICE_H_
#define COMPONENTS_LANGUAGE_CORE_BROWSER_GRAMMATICAL_GENDER_SERVICE_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/core_account_id.h"
#include "ui/base/resource/resource_bundle.h"

namespace language {

// Determines the grammatical gender with which the user should be addressed in
// grammatically gendered languages, based on the capabilities of the primary
// Google Account. The gender is only taken from the account if
// kGrammaticalGenderLocalization is enabled; otherwise it is always
// Gender::kOther. Must only be used on the UI thread.
class GrammaticalGenderService : public KeyedService,
                                 public signin::IdentityManager::Observer {
 public:
  using Gender = ui::ResourceBundle::Gender;

  // Observers are only notified of changes after they start observing, and the
  // gender is usually first determined when the service is created. Observers
  // should therefore call GetGrammaticalGender() when they start observing.
  class Observer : public base::CheckedObserver {
   public:
    // Called whenever the determined gender changes.
    virtual void OnGrammaticalGenderChanged(Gender gender) = 0;

    // Called when the service is shutting down. Observers must stop observing
    // the service.
    virtual void OnGrammaticalGenderServiceShutdown() = 0;
  };

  // `identity_manager` must be non-null.
  explicit GrammaticalGenderService(signin::IdentityManager* identity_manager);

  GrammaticalGenderService(const GrammaticalGenderService&) = delete;
  GrammaticalGenderService& operator=(const GrammaticalGenderService&) = delete;
  ~GrammaticalGenderService() override;

  // Returns the gender with which the user should be addressed, the last known
  // gender if the current one can't be determined yet (e.g. the primary
  // account's capabilities haven't been fetched), or std::nullopt if there is
  // no last known gender. Gender::kOther means the user has no known
  // grammatical gender preference.
  std::optional<Gender> GetGrammaticalGender() const;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // KeyedService:
  void Shutdown() override;

  // signin::IdentityManager::Observer:
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;
  void OnRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info) override;
  void OnExtendedAccountInfoUpdated(const AccountInfo& info) override;
  // Note: OnExtendedAccountInfoRemoved() isn't overridden. Once the primary
  // account's extended account info is removed, its capabilities are unknown,
  // so the last known gender is deliberately kept.
  void OnIdentityManagerShutdown(
      signin::IdentityManager* identity_manager) override;

 private:
  // Computes the gender from the current state of the primary account. Returns
  // std::nullopt if it can't be determined yet.
  std::optional<Gender> ComputeGrammaticalGender() const;

  // Calls UpdateGrammaticalGender() if `account_id` is the primary account.
  void MaybeUpdateGrammaticalGenderForAccount(const CoreAccountId& account_id);

  // Recomputes the gender, and notifies observers if it has changed.
  void UpdateGrammaticalGender();

  // Whether kGrammaticalGenderLocalization is enabled. If not, the gender is
  // always Gender::kOther.
  const bool feature_enabled_;
  raw_ptr<signin::IdentityManager> identity_manager_;
  std::optional<Gender> gender_;
  base::ObserverList<Observer, /*check_empty=*/true> observers_;
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace language

#endif  // COMPONENTS_LANGUAGE_CORE_BROWSER_GRAMMATICAL_GENDER_SERVICE_H_
