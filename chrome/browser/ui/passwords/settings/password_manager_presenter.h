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
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "components/password_manager/core/browser/password_store_interface.h"
#include "components/password_manager/core/browser/ui/credential_provider_interface.h"
#include "components/password_manager/core/browser/ui/plaintext_reason.h"
#include "components/prefs/pref_member.h"
#include "components/undo/undo_manager.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace password_manager {
class PasswordManagerClient;
struct PasswordForm;
}  // namespace password_manager

class PasswordUIView;

// Contains the common logic used by a PasswordUIView to
// interact with PasswordStore. It provides completion callbacks for
// PasswordStore operations and updates the view on PasswordStore changes.
class PasswordManagerPresenter
    : public password_manager::PasswordStoreInterface::Observer,
      public password_manager::PasswordStoreConsumer,
      public password_manager::CredentialProviderInterface {
 public:
  // |password_view| the UI view that owns this presenter, must not be NULL.
  explicit PasswordManagerPresenter(PasswordUIView* password_view);

  PasswordManagerPresenter(const PasswordManagerPresenter&) = delete;
  PasswordManagerPresenter& operator=(const PasswordManagerPresenter&) = delete;

  ~PasswordManagerPresenter() override;

  void Initialize();

  // PasswordStoreInterface::Observer implementation.
  void OnLoginsChanged(
      password_manager::PasswordStoreInterface* store,
      const password_manager::PasswordStoreChangeList& changes) override;
  void OnLoginsRetained(password_manager::PasswordStoreInterface* store,
                        const std::vector<password_manager::PasswordForm>&
                            retained_passwords) override;

  // Repopulates the password and exception entries.
  void UpdatePasswordLists();

  // Gets the password entry at |index|.
  const password_manager::PasswordForm* GetPassword(size_t index) const;

  // Gets the password entries corresponding to |sort_key|.
  base::span<const std::unique_ptr<password_manager::PasswordForm>>
  GetPasswordsForKey(const std::string& sort_key) const;

  // Gets the vector of password entries with the same credentials and from the
  // same site as the one stored at |index|.
  base::span<const std::unique_ptr<password_manager::PasswordForm>>
  GetPasswords(size_t index) const;

  // Gets the vector of usernames from password entries from the same site as
  // the one stored at |index|. Note that this vector can contain duplicates.
  std::vector<std::u16string> GetUsernamesForRealm(size_t index);

  // password::manager::CredentialProviderInterface:
  std::vector<std::unique_ptr<password_manager::PasswordForm>> GetAllPasswords()
      override;

  // Gets the password exception entry at |index|.
  const password_manager::PasswordForm* GetPasswordException(
      size_t index) const;

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

  // Moves a list of passwords stored in the profile store to the account store.
  // For each password to move, the result is a no-op if any of these is true:
  // |sort_key| is invalid, |sort_key| corresponds to a password already in the
  // account store, or the user is not using the account-scoped password
  // storage.
  void MovePasswordsToAccountStore(
      const std::vector<std::string>& sort_keys,
      password_manager::PasswordManagerClient* client);

#if !BUILDFLAG(IS_ANDROID)
  // Requests to reveal the plain text password corresponding to |sort_key|. If
  // |sort_key| is a valid key into |password_map_|, runs |callback| with the
  // corresponding value, or nullopt otherwise.
  // TODO(https://crbug.com/778146): Update this method to take a DisplayEntry
  // instead.
  void RequestPlaintextPassword(
      const std::string& sort_key,
      password_manager::PlaintextReason reason,
      base::OnceCallback<void(absl::optional<std::u16string>)> callback) const;
#endif

  // Wrapper around |PasswordStore::AddLogin| that adds the corresponding undo
  // action to |undo_manager_|.
  void AddLogin(const password_manager::PasswordForm& form);

  // Wrapper around |PasswordStore::RemoveLogin| that adds the corresponding
  // undo action to |undo_manager_|.
  void RemoveLogin(const password_manager::PasswordForm& form);

 private:
  // Convenience typedef for a map containing PasswordForms grouped into
  // equivalence classes. Each equivalence class corresponds to one entry shown
  // in the UI, and deleting an UI entry will delete all PasswordForms that are
  // a member of the corresponding equivalence class. The keys of the map are
  // sort keys, obtained by password_manager::CreateSortKey(). Each value of the
  // map contains forms with the same sort key.
  using PasswordFormMap =
      std::map<std::string,
               std::vector<std::unique_ptr<password_manager::PasswordForm>>>;

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
      std::vector<std::unique_ptr<password_manager::PasswordForm>> results)
      override;

  void CancelAllRequests();

  // Sets the password and exception list of the UI view.
  void SetPasswordList();
  void SetPasswordExceptionList();

  PasswordFormMap password_map_;
  PasswordFormMap exception_map_;

  UndoManager undo_manager_;

  // Whether to show stored passwords or not.
  BooleanPrefMember show_passwords_;

  // UI view that owns this presenter.
  raw_ptr<PasswordUIView> password_view_;

  base::WeakPtrFactory<PasswordManagerPresenter> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_SETTINGS_PASSWORD_MANAGER_PRESENTER_H_
