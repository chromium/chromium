// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SYNC_SYNC_PASSPHRASE_DIALOG_H_
#define CHROME_BROWSER_UI_SYNC_SYNC_PASSPHRASE_DIALOG_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "ui/base/interaction/element_identifier.h"

namespace syncer {
class SyncService;
}

DECLARE_ELEMENT_IDENTIFIER_VALUE(kSyncPassphrasePasswordFieldId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kSyncPassphraseOkButtonFieldId);

class Browser;

// Factory function to create and show the Sync passphrase dialog.
// `decrypt_data_callback` should return whether the passphrease was correct.
void ShowSyncPassphraseDialog(
    Browser& browser,
    base::RepeatingCallback<bool(const std::u16string&)> decrypt_data_callback);

// Decrypts sync data. Returns true in case of success.
// When this returns false, the user will be prompted to try again.
bool SyncPassphraseDialogDecryptData(syncer::SyncService* sync_service,
                                     const std::u16string& passphrase);

#endif  // CHROME_BROWSER_UI_SYNC_SYNC_PASSPHRASE_DIALOG_H_
