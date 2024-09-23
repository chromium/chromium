// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_ARC_ARC_VM_DATA_MIGRATION_CONFIRMATION_DIALOG_H_
#define CHROME_BROWSER_UI_ASH_ARC_ARC_VM_DATA_MIGRATION_CONFIRMATION_DIALOG_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "ui/views/window/dialog_delegate.h"

class PrefService;

namespace arc {

using ArcVmDataMigrationConfirmationCallback = base::OnceCallback<void(bool)>;

// A dialog to ask the user to confirm ARCVM /data migration. Executes the
// passed callback with |accepted| = true/false when the OK/Cancel button is
// clicked.
class ArcVmDataMigrationConfirmationDialog : public views::DialogDelegate {
 public:
  ArcVmDataMigrationConfirmationDialog(
      PrefService* prefs,
      ArcVmDataMigrationConfirmationCallback callback);

  ArcVmDataMigrationConfirmationDialog(
      const ArcVmDataMigrationConfirmationDialog&) = delete;
  ArcVmDataMigrationConfirmationDialog& operator=(
      const ArcVmDataMigrationConfirmationDialog&) = delete;

  ~ArcVmDataMigrationConfirmationDialog() override;

 private:
  void InitializeView(int days_until_deadline);
  void OnButtonClicked(int days_until_deadline, bool accepted);

  ArcVmDataMigrationConfirmationCallback callback_;

  base::WeakPtrFactory<ArcVmDataMigrationConfirmationDialog> weak_ptr_factory_{
      this};
};

void ShowArcVmDataMigrationConfirmationDialog(
    PrefService* prefs,
    ArcVmDataMigrationConfirmationCallback callback);

}  // namespace arc

#endif  // CHROME_BROWSER_UI_ASH_ARC_ARC_VM_DATA_MIGRATION_CONFIRMATION_DIALOG_H_
