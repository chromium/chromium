// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_LOCAL_CARD_MIGRATION_DIALOG_STATE_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_LOCAL_CARD_MIGRATION_DIALOG_STATE_H_

namespace autofill {

// The current view state of the local card migration dialog.
enum class LocalCardMigrationDialogState {
  // Dialog that offers users to migrate browser-saved local cards.
  kOffered,
  // Dialog that shows to users migration is done.
  kFinished,
  // Dialog that notifies users there are errors in the process of
  // migration, and requires further actions from users.
  kActionRequired,
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_LOCAL_CARD_MIGRATION_DIALOG_STATE_H_
