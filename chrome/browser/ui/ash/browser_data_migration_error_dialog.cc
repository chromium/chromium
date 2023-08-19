// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/browser_data_migration_error_dialog.h"

#include <string>

#include "chrome/browser/ui/simple_message_box.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/text/bytes_formatting.h"
#include "ui/gfx/native_widget_types.h"

namespace ash {

void OpenBrowserDataMigrationErrorDialog(uint64_t required_size) {
  chrome::ShowWarningMessageBox(
      gfx::NativeWindow(), std::u16string(),
      l10n_util::GetStringFUTF16(
          IDS_LACROS_DATA_MIGRATION_OUT_OF_DISK_ERROR_TEXT,
          ui::FormatBytes(static_cast<int64_t>(required_size))));
}

}  // namespace ash
