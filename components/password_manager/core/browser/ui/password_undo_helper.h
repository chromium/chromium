// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_PASSWORD_UNDO_HELPER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_PASSWORD_UNDO_HELPER_H_

#include "base/memory/raw_ptr.h"
#include "components/undo/undo_manager.h"

namespace password_manager {

class PasswordStoreInterface;
struct PasswordForm;

// Helper class to revert deletion of a saved passwords or password exception
// entries.
class PasswordUndoHelper {
 public:
  explicit PasswordUndoHelper(PasswordStoreInterface* profile_store,
                              PasswordStoreInterface* account_store);
  PasswordUndoHelper(const PasswordUndoHelper&) = delete;
  PasswordUndoHelper& operator=(const PasswordUndoHelper&) = delete;

  // Adds password to the undo action.
  void PasswordRemoved(const password_manager::PasswordForm& form);

  // Reverts last grouped deletion.
  void Undo();

  // Starts grouping multiple password deletions under single undo action.
  // Should be called before first call to 'PasswordRemoved()'.
  void StartGroupingActions();

  // Ends grouping multiple password deletions under single undo action.
  // Should be called after last call to 'PasswordRemoved()'.
  void EndGroupingActions();

 private:
  UndoManager undo_manager_;

  raw_ptr<PasswordStoreInterface> profile_store_;
  raw_ptr<PasswordStoreInterface> account_store_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_PASSWORD_UNDO_HELPER_H_
