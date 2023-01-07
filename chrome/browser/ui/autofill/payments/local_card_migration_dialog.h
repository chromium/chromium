// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_LOCAL_CARD_MIGRATION_DIALOG_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_LOCAL_CARD_MIGRATION_DIALOG_H_

#include "base/functional/callback.h"

namespace content {
class WebContents;
}  // namespace content

namespace autofill {

// The cross-platform UI interface which displays all the local card migration
// dialogs.
class LocalCardMigrationDialog {
 public:
  LocalCardMigrationDialog(const LocalCardMigrationDialog&) = delete;
  LocalCardMigrationDialog& operator=(const LocalCardMigrationDialog&) = delete;
  virtual void ShowDialog(content::WebContents& web_contents) = 0;
  virtual void CloseDialog() = 0;

 protected:
  LocalCardMigrationDialog() = default;
  virtual ~LocalCardMigrationDialog() = default;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_LOCAL_CARD_MIGRATION_DIALOG_H_
