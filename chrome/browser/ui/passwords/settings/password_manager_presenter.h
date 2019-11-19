// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_SETTINGS_PASSWORD_MANAGER_PRESENTER_H_
#define CHROME_BROWSER_UI_PASSWORDS_SETTINGS_PASSWORD_MANAGER_PRESENTER_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "components/password_manager/core/browser/ui/credential_provider_interface.h"
#include "components/prefs/pref_member.h"
#include "components/undo/undo_manager.h"
#include "ui/shell_dialogs/select_file_dialog.h"

namespace autofill {
struct PasswordForm;
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

  // Changes the username and password corresponding to |sort_key|.
  void ChangeSavedPassword(const std::string& sort_key,
                           const base::string16& new_username,
                           const base::Optional<base::string16>& new_password);

  // Removes the saved password entries at |index|, or corresponding to
  // |sort_key|, respectively.
  // TODO(https://crbug.com/778146): Unify these methods and the implementation
  // across Desktop and Android.
  void RemoveSavedPassword(size_t index);
  void RemoveSavedPassword(const std::string& sort_key);

  // Removes the saved exception entries at |index|, or corresponding to
  // |sort_key|, respectively.
  // TODO(https://crbug.com/778146): Unify these methods and the implementation
  // across Desktop and Android.
  void RemovePasswordException(size_t index);
  void RemovePasswordException(const std::string& sort_key);

  // Undoes the last saved password or exception removal.
  void UndoRemoveSavedPasswordOrException();

  // Requests to reveal the plain text password corresponding to |sort_key|. If
  // |sort_key| is a valid key into |password_map_|, runs |callback| with the
  // corresponding value, or nullopt otherwise.
  // TODO(https://crbug.com/778146): Update this method to take a DisplayEntry
  // instead.
  void RequestShowPassword(
      const std::string& sort_key,
      base::OnceCallback<void(base::Optional<base::string16>)> callback) const;

  // Wrapper around |PasswordStore::AddLogin| that adds the corresponding undo
  // action to |undo_manager_|.
  void AddLogin(const autofill::PasswordForm& form);

  // Wrapper around |PasswordStore::RemoveLogin| that adds the corresponding
  // undo action to |undo_manager_|.
  void RemoveLogin(const autofill::PasswordForm& form);

 private:
  // Convenience typedef for a map containing PasswordForms grouped into
  // equivalence classes. Each equivalence class corresponds to one entry shown
  // in the UI, and deleting an UI entry will delete all PasswordForms that are
  // a member of the corresponding equivalence class. The keys of the map are
  // sort keys, obtained by password_manager::CreateSortKey(). Each value of the
  // map contains forms with the same sort key.
  using PasswordFormMap =
      std::map<std::string,
               std::vector<std::unique_ptr<autofill::PasswordForm>>>;

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

  // Returns the password store associated with the currently active profile.
  password_manager::PasswordStore* GetPasswordStore(bool use_account_store);

  PasswordFormMap password_map_;
  PasswordFormMap exception_map_;

  UndoManager undo_manager_;

  // Whether to show stored passwords or not.
  BooleanPrefMember show_passwords_;

  // UI view that owns this presenter.
  PasswordUIView* password_view_;

  DISALLOW_COPY_AND_ASSIGN(PasswordManagerPresenter);
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_SETTINGS_PASSWORD_MANAGER_PRESENTER_H_
