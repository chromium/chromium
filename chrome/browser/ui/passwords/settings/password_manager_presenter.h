// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_SETTINGS_PASSWORD_MANAGER_PRESENTER_H_
#define CHROME_BROWSER_UI_PASSWORDS_SETTINGS_PASSWORD_MANAGER_PRESENTER_H_

#include <stddef.h>

#include <list>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "components/password_manager/core/browser/form_fetcher.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "components/password_manager/core/browser/ui/credential_provider_interface.h"
#include "components/password_manager/core/browser/ui/plaintext_reason.h"
#include "components/prefs/pref_member.h"
#include "components/undo/undo_manager.h"
#include "ui/shell_dialogs/select_file_dialog.h"

namespace autofill {
struct PasswordForm;
}

namespace password_manager {
class PasswordManagerClient;
}

class PasswordUIView;

// Contains the common logic used by a PasswordUIView to
// interact with PasswordStore. It provides completion callbacks for
// PasswordStore operations and updates the view on PasswordStore changes.
class PasswordManagerPresenter
    : public password_manager::PasswordStore::Observer,
      public password_manager::PasswordStoreConsumer,
      public password_manager::CredentialProviderInterface {
 public:
  // |password_view| the UI view that owns this presenter, must not be NULL.
  explicit PasswordManagerPresenter(PasswordUIView* password_view);
  ~PasswordManagerPresenter() override;

  void Initialize();

  // PasswordStore::Observer implementation.
  void OnLoginsChanged(
      const password_manager::PasswordStoreChangeList& changes) override;

  // Repopulates the password and exception entries.
  void UpdatePasswordLists();

  // Gets the password entry at |index|.
  const autofill::PasswordForm* GetPassword(size_t index) const;

  // Gets the vector of password entries with the same credentials and from the
  // same site as the one stored at |index|.
  base::span<const std::unique_ptr<autofill::PasswordForm>> GetPasswords(
      size_t index) const;

  // Gets the vector of usernames from password entries from the same site as
  // the one stored at |index|. Note that this vector can contain duplicates.
  std::vector<base::string16> GetUsernamesForRealm(size_t index);

  // password::manager::CredentialProviderInterface:
  std::vector<std::unique_ptr<autofill::PasswordForm>> GetAllPasswords()
      override;

  // Gets the password exception entry at |index|.
  const autofill::PasswordForm* GetPasswordException(size_t index) const;

  // Changes the password corresponding to |sort_keys|.
  bool ChangeSavedPassword(const std::vector<std::string>& sort_keys,
                           const base::string16& new_username,
                           const base::string16& new_password);

  // Removes the saved password entries at |index|, or corresponding to
  // |sort_key|, respectively.
  // TODO(https://crbug.com/778146): Unify these methods and the implementation
  // across Desktop and Android.
  void RemoveSavedPassword(size_t index);
  void RemoveSavedPasswords(const std::vector<std::string>& sort_keys);

  // Removes the saved exception entries at |index|, or corresponding to
  // |sort_key|, respectively.
  // TODO(https://crbug.com/778146): Unify these methods and the implementation
  // across Desktop and Android.
  void RemovePasswordException(size_t index);
  void RemovePasswordExceptions(const std::vector<std::string>& sort_keys);

  // Undoes the last saved password or exception removal.
  void UndoRemoveSavedPasswordOrException();

  // Moves a password stored in the profile store to the account store. Results
  // in a no-op if any of these is true: |sort_key| is invalid, |sort_key|
  // corresponds to a password already in the account store, or the user is not
  // using the account-scoped password storage.
  void MovePasswordToAccountStore(
      const std::string& sort_key,
      password_manager::PasswordManagerClient* client);

#if !defined(OS_ANDROID)
  // Requests to reveal the plain text password corresponding to |sort_key|. If
  // |sort_key| is a valid key into |password_map_|, runs |callback| with the
  // corresponding value, or nullopt otherwise.
  // TODO(https://crbug.com/778146): Update this method to take a DisplayEntry
  // instead.
  void RequestPlaintextPassword(
      const std::string& sort_key,
      password_manager::PlaintextReason reason,
      base::OnceCallback<void(base::Optional<base::string16>)> callback) const;
#endif

  // Wrapper around |PasswordStore::AddLogin| that adds the corresponding undo
  // action to |undo_manager_|.
  void AddLogin(const autofill::PasswordForm& form);

  // Wrapper around |PasswordStore::RemoveLogin| that adds the corresponding
  // undo action to |undo_manager_|.
  void RemoveLogin(const autofill::PasswordForm& form);

 private:
  // Used for moving a form from the profile store to the account store.
  class MovePasswordToAccountStoreHelper
      : public password_manager::FormFetcher::Consumer {
   public:
    // Starts moving |form|. |done_callback| is run when done.
    MovePasswordToAccountStoreHelper(
        const autofill::PasswordForm& form,
        password_manager::PasswordManagerClient* client,
        base::OnceClosure done_callback);
    ~MovePasswordToAccountStoreHelper() override;

   private:
    // FormFetcher::Consumer.
    void OnFetchCompleted() override;

    autofill::PasswordForm form_;
    password_manager::PasswordManagerClient* const client_;
    base::OnceClosure done_callback_;
    std::unique_ptr<password_manager::FormFetcher> form_fetcher_;
  };

  // Convenience typedef for a map containing PasswordForms grouped into
  // equivalence classes. Each equivalence class corresponds to one entry shown
  // in the UI, and deleting an UI entry will delete all PasswordForms that are
  // a member of the corresponding equivalence class. The keys of the map are
  // sort keys, obtained by password_manager::CreateSortKey(). Each value of the
  // map contains forms with the same sort key.
  using PasswordFormMap =
      std::map<std::string,
               std::vector<std::unique_ptr<autofill::PasswordForm>>>;

  using MovePasswordToAccountStoreHelperList =
      std::list<std::unique_ptr<MovePasswordToAccountStoreHelper>>;

  // Attempts to remove the entries corresponding to |index| from |form_map|.
  // This will also add a corresponding undo operation to |undo_manager_|.
  // Returns whether removing the entry succeeded.
  bool TryRemovePasswordEntries(PasswordFormMap* form_map, size_t index);

  // Attempts to remove the entries corresponding to |sort_key| from |form_map|.
  // This will also add a corresponding undo operation to |undo_manager_|.
  // Returns whether removing the entry succeeded.
  bool TryRemovePasswordEntries(PasswordFormMap* form_map,
                                const std::string& sort_key);

  // Attempts to remove the entries pointed to by |forms_iter| from |form_map|.
  // This will also add a corresponding undo operation to |undo_manager_|.
  // Returns whether removing the entry succeeded.
  bool TryRemovePasswordEntries(PasswordFormMap* form_map,
                                PasswordFormMap::const_iterator forms_iter);

  // PasswordStoreConsumer:
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<autofill::PasswordForm>> results) override;

  // Sets the password and exception list of the UI view.
  void SetPasswordList();
  void SetPasswordExceptionList();

  // Called when the helper pointed by |done_helper_it| has finished the moving
  // task. Removes it from |move_to_account_helpers_|.
  void OnMovePasswordToAccountCompleted(
      MovePasswordToAccountStoreHelperList::iterator done_helper_it);

  PasswordFormMap password_map_;
  PasswordFormMap exception_map_;

  UndoManager undo_manager_;

  // Whether to show stored passwords or not.
  BooleanPrefMember show_passwords_;

  // UI view that owns this presenter.
  PasswordUIView* password_view_;

  // Contains the helpers currently executing moving tasks.
  MovePasswordToAccountStoreHelperList move_to_account_helpers_;

  DISALLOW_COPY_AND_ASSIGN(PasswordManagerPresenter);
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_SETTINGS_PASSWORD_MANAGER_PRESENTER_H_
