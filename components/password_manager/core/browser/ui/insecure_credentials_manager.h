// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_INSECURE_CREDENTIALS_MANAGER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_INSECURE_CREDENTIALS_MANAGER_H_

#include <map>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/types/strong_alias.h"
#include "build/build_config.h"
#include "components/password_manager/core/browser/leak_detection/bulk_leak_check.h"
#include "components/password_manager/core/browser/ui/credential_utils.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#include "url/gurl.h"

namespace password_manager {

struct CredentialUIEntry;
class LeakCheckCredential;

// This class provides clients with saved insecure credentials and possibility
// to save new LeakedCredentials, edit/delete/[un]mute insecure credentials and
// match insecure credentials with corresponding autofill::PasswordForms. It
// supports an observer interface, and clients can register themselves to get
// notified about changes to the list.
class InsecureCredentialsManager : public SavedPasswordsPresenter::Observer {
 public:
  // Observer interface. Clients can implement this to get notified about
  // changes to the list of insecure and weak credentials. Clients can register
  // and de-register themselves, and are expected to do so before the provider
  // gets out of scope.
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnInsecureCredentialsChanged() = 0;
  };

  explicit InsecureCredentialsManager(SavedPasswordsPresenter* presenter);
  ~InsecureCredentialsManager() override;

#if !BUILDFLAG(IS_ANDROID)
  // Computes reused credentials in a separate thread and then passes the result
  // to OnReuseCheckDone.
  void StartReuseCheck(base::OnceClosure on_check_done = base::DoNothing());

  // Computes weak credentials in a separate thread and then passes the result
  // to OnWeakCheckDone.
  void StartWeakCheck(base::OnceClosure on_check_done = base::DoNothing());
#endif

  // Marks all saved credentials which have same username & password as
  // insecure.
  void SaveInsecureCredential(
      const LeakCheckCredential& credential,
      TriggerBackendNotification should_trigger_notification);

  // Attempts to mute |credential| from the password store.
  // Returns whether the mute succeeded.
  bool MuteCredential(const CredentialUIEntry& credential);

  // Attempts to unmute |credential| from the password store.
  // Returns whether the unmute succeeded.
  bool UnmuteCredential(const CredentialUIEntry& credential);

  // Returns a vector of currently insecure credentials.
  std::vector<CredentialUIEntry> GetInsecureCredentialEntries() const;

  // Allows clients and register and de-register themselves.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  // Updates |weak_passwords| set and notifies observers that weak credentials
  // were changed.
  void OnWeakCheckDone(base::ElapsedTimer timer_since_weak_check_start,
                       base::flat_set<std::u16string> weak_passwords);
  void OnPartialWeakCheckDone(base::flat_set<std::u16string> weak_passwords);

  // Updates |reused_passwords| set and notifies observers that insecure
  // credentials were changed.
  void OnReuseCheckDone(base::ElapsedTimer timer_since_reuse_check_start,
                        base::flat_set<std::u16string> reused_passwords);

  // SavedPasswordsPresenter::Observer:
  void OnSavedPasswordsChanged(const PasswordStoreChangeList& changes) override;

  // Notifies observers when insecure credentials have changed.
  void NotifyInsecureCredentialsChanged();

  // A weak handle to the presenter used to join the list of insecure
  // credentials with saved passwords. Needs to outlive this instance.
  raw_ptr<SavedPasswordsPresenter> presenter_ = nullptr;

  // Cache of the most recently obtained weak passwords.
  base::flat_set<std::u16string> weak_passwords_;

  // Cache of the most recently obtained reused passwords.
  base::flat_set<std::u16string> reused_passwords_;

  // A scoped observer for |presenter_|.
  base::ScopedObservation<SavedPasswordsPresenter,
                          SavedPasswordsPresenter::Observer>
      observed_saved_password_presenter_{this};

  base::ObserverList<Observer, /*check_empty=*/true> observers_;

  base::WeakPtrFactory<InsecureCredentialsManager> weak_ptr_factory_{this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_INSECURE_CREDENTIALS_MANAGER_H_
