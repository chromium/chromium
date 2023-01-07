// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_BROWSER_DATA_MIGRATION_ERROR_DIALOG_H_
#define CHROME_BROWSER_UI_ASH_BROWSER_DATA_MIGRATION_ERROR_DIALOG_H_

#include <stdint.h>

namespace ash {

// Displays an error dialog to tell browser_data_migration error
// due to out-of-disk.
void OpenBrowserDataMigrationErrorDialog(uint64_t required_size);

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_BROWSER_DATA_MIGRATION_ERROR_DIALOG_H_
