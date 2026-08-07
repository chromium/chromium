// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/password_undo_helper.h"

#include "base/memory/raw_ptr.h"
#include "components/password_manager/core/browser/password_store/password_form_converters.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/password_manager/core/browser/password_store/stored_credential.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#include "components/undo/undo_operation.h"

namespace password_manager {

namespace {

enum PasswordOperationType {
  kRemoveOperation,
  kUpdateOperation,
  kAddOperation,
  kLastItem = kAddOperation
};

template <PasswordOperationType Type>
class PasswordOperation : public UndoOperation {
 public:
  PasswordOperation(PasswordStoreInterface* profile_store,
                    PasswordStoreInterface* account_store,
                    UndoManager* undo_manager,
                    StoredCredential credential)
      : profile_store_(profile_store),
        account_store_(account_store),
        undo_manager_(undo_manager),
        credential_(std::move(credential)) {}
  PasswordOperation(const PasswordOperation&) = delete;
  PasswordOperation& operator=(const PasswordOperation&) = delete;
  ~PasswordOperation() override = default;

  // UndoOperation:
  void Undo() override {
    switch (Type) {
      case PasswordOperationType::kRemoveOperation:
        RemoveLogin(credential_);
        break;
      case PasswordOperationType::kAddOperation:
        AddLogin(credential_);
        break;
      case PasswordOperationType::kUpdateOperation:
        UpdateLogin(credential_);
        break;
    }
  }
  int GetUndoLabelId() const override { return 0; }
  int GetRedoLabelId() const override { return 0; }

 private:
  void AddLogin(const StoredCredential& credential) {
    // Add redo operation for an added credential.
    DCHECK(profile_store_);
    DCHECK(undo_manager_);

    undo_manager_->AddUndoOperation(
        std::make_unique<
            PasswordOperation<PasswordOperationType::kRemoveOperation>>(
            profile_store_, account_store_, undo_manager_,
            CloneStoredCredential(credential_)));
    if (credential.IsUsingAccountStore()) {
      account_store_->AddLogin(CloneStoredCredential(credential));
    }
    if (credential.IsUsingProfileStore()) {
      profile_store_->AddLogin(CloneStoredCredential(credential));
    }
  }

  void UpdateLogin(const StoredCredential& new_credential) {
    DCHECK(profile_store_);
    DCHECK(undo_manager_);

    undo_manager_->AddUndoOperation(
        std::make_unique<
            PasswordOperation<PasswordOperationType::kUpdateOperation>>(
            profile_store_, account_store_, undo_manager_,
            CloneStoredCredential(credential_)));
    if (new_credential.IsUsingAccountStore()) {
      account_store_->UpdateLogin(CloneStoredCredential(new_credential));
    }
    if (new_credential.IsUsingProfileStore()) {
      profile_store_->UpdateLogin(CloneStoredCredential(new_credential));
    }
  }

  void RemoveLogin(const StoredCredential& credential) {
    // Add undo operation for a removed credential.
    undo_manager_->AddUndoOperation(
        std::make_unique<
            PasswordOperation<PasswordOperationType::kAddOperation>>(
            profile_store_, account_store_, undo_manager_,
            CloneStoredCredential(credential_)));
    if (credential.IsUsingAccountStore()) {
      account_store_->RemoveLogin(FROM_HERE, CloneStoredCredential(credential));
    }
    if (credential.IsUsingProfileStore()) {
      profile_store_->RemoveLogin(FROM_HERE, CloneStoredCredential(credential));
    }
  }

  raw_ptr<PasswordStoreInterface> profile_store_;
  raw_ptr<PasswordStoreInterface> account_store_;
  raw_ptr<UndoManager> undo_manager_ = nullptr;
  StoredCredential credential_;
};

}  // namespace

PasswordUndoHelper::PasswordUndoHelper(PasswordStoreInterface* profile_store,
                                       PasswordStoreInterface* account_store)
    : profile_store_(profile_store), account_store_(account_store) {}

void PasswordUndoHelper::PasswordRemoved(StoredCredential credential) {
  undo_manager_.AddUndoOperation(
      std::make_unique<PasswordOperation<PasswordOperationType::kAddOperation>>(
          profile_store_, account_store_, &undo_manager_,
          std::move(credential)));
}
void PasswordUndoHelper::BackupPasswordRemoved(
    StoredCredential credential_with_backup) {
  undo_manager_.AddUndoOperation(
      std::make_unique<
          PasswordOperation<PasswordOperationType::kUpdateOperation>>(
          profile_store_, account_store_, &undo_manager_,
          std::move(credential_with_backup)));
}

void PasswordUndoHelper::Undo() {
  undo_manager_.Undo();
}

void PasswordUndoHelper::StartGroupingActions() {
  undo_manager_.StartGroupingActions();
}

void PasswordUndoHelper::EndGroupingActions() {
  undo_manager_.EndGroupingActions();
}

}  // namespace password_manager
