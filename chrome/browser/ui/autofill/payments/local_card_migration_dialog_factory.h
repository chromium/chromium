// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_LOCAL_CARD_MIGRATION_DIALOG_FACTORY_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_LOCAL_CARD_MIGRATION_DIALOG_FACTORY_H_

namespace autofill {

class LocalCardMigrationDialogController;
class LocalCardMigrationDialog;

LocalCardMigrationDialog* CreateLocalCardMigrationDialogView(
    LocalCardMigrationDialogController* controller);

LocalCardMigrationDialog* CreateLocalCardMigrationErrorDialogView(
    LocalCardMigrationDialogController* controller);

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_LOCAL_CARD_MIGRATION_DIALOG_FACTORY_H_
