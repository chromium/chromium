// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/nonclosable_app_ui/nonclosable_app_ui_utils.h"

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/system/toast_data.h"
#include "ash/public/cpp/system/toast_manager.h"
#include "base/i18n/message_formatter.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

namespace {
const char kPreventCloseToastIdPrefix[] = "prevent_close_toast_id-";
}  // namespace

void ShowNonclosableAppToast(const std::string& app_id,
                             const std::string& app_name) {
  ToastManager::Get()->Show(
      {/*id=*/kPreventCloseToastIdPrefix + app_id,
       ToastCatalogName::kAppNotClosable,
       /*text=*/
       base::i18n::MessageFormatter::FormatWithNamedArgs(
           l10n_util::GetStringUTF16(IDS_PREVENT_CLOSE_TOAST_MESSAGE),
           /*name0=*/"APP_NAME", app_name)});
}

}  // namespace ash
