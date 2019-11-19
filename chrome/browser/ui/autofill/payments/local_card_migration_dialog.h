// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_LOCAL_CARD_MIGRATION_DIALOG_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_LOCAL_CARD_MIGRATION_DIALOG_H_

#include "base/callback.h"
#include "base/macros.h"

namespace autofill {

// The cross-platform UI interface which displays all the local card migration
// dialogs.
class LocalCardMigrationDialog {
 public:
  virtual void ShowDialog() = 0;
  virtual void CloseDialog() = 0;

 protected:
  LocalCardMigrationDialog() {}
  virtual ~LocalCardMigrationDialog() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(LocalCardMigrationDialog);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_LOCAL_CARD_MIGRATION_DIALOG_H_
