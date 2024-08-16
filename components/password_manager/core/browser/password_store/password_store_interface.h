// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_PASSWORD_STORE_INTERFACE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_PASSWORD_STORE_INTERFACE_H_

#include <vector>

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list_types.h"
#include "components/keyed_service/core/refcounted_keyed_service.h"
#include "components/password_manager/core/browser/password_form_digest.h"
#include "components/password_manager/core/browser/password_store/password_store_change.h"

namespace base {
class Location;
}  // namespace base

namespace syncer {
class DataTypeControllerDelegate;
class SyncService;
}  // namespace syncer

namespace password_manager {

struct PasswordForm;

class PasswordStoreBackend;
class PasswordStoreConsumer;
class SmartBubbleStatsStore;

using IsAccountStore = base::StrongAlias<class IsAccountStoreTag, bool>;

const IsAccountStore kProfileStore = IsAccountStore(false);
const IsAccountStore kAccountStore = IsAccountStore(true);

// Interface for storing form passwords in a secure way. The usage is
// independent of the platform.
// All methods are expected to have an asynchronous implementation that persists
// changes to a local database or an external service organizing the passwords.
// The I/O heavy initialization should also be performed asynchronously. If this
// deferred initialization fails, all subsequent method calls should fail
// without side effects, return no data, and send no notifications.
class PasswordStoreInterface : public RefcountedKeyedService {
 public:
  // An interface used to notify clients (observers) of this object that data in
  // the password store has changed. Register the observer via
  // `PasswordStore::AddObserver`.
  class Observer : public base::CheckedObserver {
   public:
    // Notifies the observer that password data changed (e.g. added or changed).
    // Don't rely on `changes` containing REMOVED entries. Certain stores don't
    // propagate that information as soon as the unified password manager works.
    // The passed `store` issued the observer notification in case there might
    // be multiple ones.
    // Instead, use `OnLoginsRetained` to validate tracked/shown passwords.
    virtual void OnLoginsChanged(PasswordStoreInterface* store,
                                 const PasswordStoreChangeList& changes) = 0;

    // Notifies the observer that password data changed. Will be called from
    // the UI thread. The `retained_passwords` are a complete list of passwords
    // and blocklisted sites. The passed `store` issued the observer
    // notification in case there might be multiple ones.
    virtual void OnLoginsRetained(
        PasswordStoreInterface* store,
        const std::vector<PasswordForm>& retained_passwords) = 0;
  };

  // Necessary condition to offer saving passwords.
  virtual bool IsAbleToSavePasswords() const = 0;

  // Adds the given PasswordForm to the secure password store asynchronously.
  // `completion` will be run after the form is added.
  virtual void AddLogin(const PasswordForm& form,
                        base::OnceClosure completion = base::DoNothing()) = 0;

  // Adds all forms in the given vector of PasswordForm to the secure password
  // store asynchronously. `completion` will be run after the forms are added.
  virtual void AddLogins(const std::vector<PasswordForm>& forms,
                         base::OnceClosure completion = base::DoNothing()) = 0;

  // Updates the matching PasswordForm in the secure password store (async).
  // If any of the primary key fields (signon_realm, url, username_element,
  // username_value, password_element) are updated, then the second version of
  // the method must be used that takes `old_primary_key`, i.e., the old values
  // for the primary key fields (the rest of the fields are ignored).
  // completion will be run after the form is updated.
  virtual void UpdateLogin(
      const PasswordForm& form,
      base::OnceClosure completion = base::DoNothing()) = 0;

  // Updates all matching forms in the given vector of PasswordForm in the
  // secure password store (async). Completion will be run after the forms are
  // updated.
  virtual void UpdateLogins(const std::vector<PasswordForm>& forms,
                            base::OnceClosure completion) = 0;

  virtual void UpdateLoginWithPrimaryKey(
      const PasswordForm& new_form,
      const PasswordForm& old_primary_key,
      base::OnceClosure completion = base::DoNothing()) = 0;

  // Removes the matching PasswordForm from the secure password store (async).
  // `location` is used for logging purposes and investigations.
  virtual void RemoveLogin(const base::Location& location,
                           const PasswordForm& form) = 0;

  // Remove all logins whose origins match the given filter and that were
  // created in the given date range. `completion` will be run after deletions
  // have been completed and notifications have been sent out.
  // If the platform supports sync, `sync_completion` will be run once the
  // deletions have also been propagated to the server (or, in rare cases, if
  // the user permanently disables Sync or deletions haven't been propagated
  // after 30 seconds). This is only relevant for Sync users and for account
  // store users - for other users, `sync_completion` will be run immediately
  // after `completion`. `location` is used for logging purposes and
  // investigations.
  virtual void RemoveLoginsByURLAndTime(
      const base::Location& location,
      const base::RepeatingCallback<bool(const GURL&)>& url_filter,
      base::Time delete_begin,
      base::Time delete_end,
      base::OnceClosure completion = base::NullCallback(),
      base::OnceCallback<void(bool)> sync_completion =
          base::NullCallback()) = 0;

  // Removes all logins created in the given date range. `completion` is run
  // after deletions have been completed and notifications have been sent out.
  // If any logins were removed 'true' will be passed to `completion`, 'false'
  // otherwise. `location` is used for logging purposes and investigations.
  virtual void RemoveLoginsCreatedBetween(
      const base::Location& location,
      base::Time delete_begin,
      base::Time delete_end,
      base::OnceCallback<void(bool)> completion = base::NullCallback()) = 0;

  // Sets the 'skip_zero_click' flag for all credentials that match
  // `origin_filter`. `completion` will be run after these modifications are
  // completed and notifications are sent out.
  virtual void DisableAutoSignInForOrigins(
      const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
      base::OnceClosure completion = base::NullCallback()) = 0;

  // Unblocklists the login with `form_digest` by deleting all the
  // corresponding blocklisted entries. If `completion` is not null, it will
  // be run after deletions have been completed. Should be called on the UI
  // thread.
  virtual void Unblocklist(
      const PasswordFormDigest& form_digest,
      base::OnceClosure completion = base::NullCallback()) = 0;

  // Searches for a matching PasswordForm, and notifies `consumer` on
  // completion.
  // TODO(crbug.com/40185049): Use a smart pointer for consumer.
  virtual void GetLogins(const PasswordFormDigest& form,
                         base::WeakPtr<PasswordStoreConsumer> consumer) = 0;

  // Gets the complete list of non-blocklist PasswordForms.`consumer` will be
  // notified on completion.
  // TODO(crbug.com/40185049): Use a smart pointer for consumer.
  virtual void GetAutofillableLogins(
      base::WeakPtr<PasswordStoreConsumer> consumer) = 0;

  // Gets the complete list of PasswordForms (regardless of their blocklist
  // status) and notify `consumer` on completion.
  // TODO(crbug.com/40185049): Use a smart pointer for consumer.
  virtual void GetAllLogins(base::WeakPtr<PasswordStoreConsumer> consumer) = 0;

  // Gets the complete list of PasswordForms, regardless of their blocklist
  // status. Also fills in affiliation and branding information for Android
  // credentials.
  // TODO(crbug.com/40185049): Use a smart pointer for consumer.
  virtual void GetAllLoginsWithAffiliationAndBrandingInformation(
      base::WeakPtr<PasswordStoreConsumer> consumer) = 0;

  // Adds an observer to be notified when the password store data changes.
  virtual void AddObserver(Observer* observer) = 0;

  // Removes `observer` from the observer list.
  virtual void RemoveObserver(Observer* observer) = 0;

  // Returns the store responsible for smart bubble behaviour websites stats.
  virtual SmartBubbleStatsStore* GetSmartBubbleStatsStore() = 0;

  // For sync codebase only: instantiates a proxy controller delegate to
  // interact with PasswordSyncBridge. Must be called from the UI thread.
  virtual std::unique_ptr<syncer::DataTypeControllerDelegate>
  CreateSyncControllerDelegate() = 0;

  // Propagates successful initialization of SyncService to reolve circular
  // dependency during PasswordStore creation. |sync_service| may not
  // have started yet but its preferences can already be queried.
  virtual void OnSyncServiceInitialized(syncer::SyncService* sync_service) = 0;

  // The passed callback will be invoked whenever sync is enabled/disabled.
  virtual base::CallbackListSubscription AddSyncEnabledOrDisabledCallback(
      base::RepeatingClosure sync_enabled_or_disabled_cb) = 0;

  // Tests only can retrieve the backend.
  virtual PasswordStoreBackend* GetBackendForTesting() = 0;

 protected:
  ~PasswordStoreInterface() override = default;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_PASSWORD_STORE_INTERFACE_H_
