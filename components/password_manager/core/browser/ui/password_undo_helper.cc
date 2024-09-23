// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/password_undo_helper.h"

#include "base/memory/raw_ptr.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#include "components/undo/undo_operation.h"

namespace password_manager {

namespace {

enum PasswordOperationType {
  kRemoveOperation,
  kAddOperation,
  kLastItem = kAddOperation
};

template <PasswordOperationType Type>
class PasswordOperation : public UndoOperation {
 public:
  PasswordOperation(PasswordStoreInterface* profile_store,
                    PasswordStoreInterface* account_store,
                    UndoManager* undo_manager,
                    const password_manager::PasswordForm& form)
      : profile_store_(profile_store),
        account_store_(account_store),
        undo_manager_(undo_manager),
        form_(form) {}
  PasswordOperation(const PasswordOperation&) = delete;
  PasswordOperation& operator=(const PasswordOperation&) = delete;
  ~PasswordOperation() override = default;

  // UndoOperation:
  void Undo() override {
    switch (Type) {
      case PasswordOperationType::kRemoveOperation:
        RemoveLogin(form_);
        break;
      case PasswordOperationType::kAddOperation:
        AddLogin(form_);
        break;
    }
  }
  int GetUndoLabelId() const override { return 0; }
  int GetRedoLabelId() const override { return 0; }

 private:
  void AddLogin(const password_manager::PasswordForm& form) {
    // Add redo operation for an added form.
    DCHECK(profile_store_);
    DCHECK(undo_manager_);

    undo_manager_->AddUndoOperation(
        std::make_unique<
            PasswordOperation<PasswordOperationType::kRemoveOperation>>(
            profile_store_, account_store_, undo_manager_, form_));
    if (form.IsUsingAccountStore()) {
      account_store_->AddLogin(form);
    }
    if (form.IsUsingProfileStore()) {
      profile_store_->AddLogin(form);
    }
  }

  void RemoveLogin(const password_manager::PasswordForm& form) {
    // Add undo operation for a removed form.
    undo_manager_->AddUndoOperation(
        std::make_unique<
            PasswordOperation<PasswordOperationType::kAddOperation>>(
            profile_store_, account_store_, undo_manager_, form_));
    if (form.IsUsingAccountStore()) {
      account_store_->RemoveLogin(FROM_HERE, form);
    }
    if (form.IsUsingProfileStore()) {
      profile_store_->RemoveLogin(FROM_HERE, form);
    }
  }

  raw_ptr<PasswordStoreInterface> profile_store_;
  raw_ptr<PasswordStoreInterface> account_store_;
  raw_ptr<UndoManager> undo_manager_ = nullptr;
  password_manager::PasswordForm form_;
};

}  // namespace

PasswordUndoHelper::PasswordUndoHelper(PasswordStoreInterface* profile_store,
                                       PasswordStoreInterface* account_store)
    : profile_store_(profile_store), account_store_(account_store) {}

void PasswordUndoHelper::PasswordRemoved(
    const password_manager::PasswordForm& form) {
  undo_manager_.AddUndoOperation(
      std::make_unique<PasswordOperation<PasswordOperationType::kAddOperation>>(
          profile_store_, account_store_, &undo_manager_, form));
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
