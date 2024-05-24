// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/app_dialog/app_local_block_dialog_view.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/color/color_id.h"
#include "ui/views/window/dialog_delegate.h"

namespace {

AppLocalBlockDialogView* g_app_local_block_dialog_view = nullptr;

constexpr int32_t kIconSize = 48;

}  // namespace

// static
void apps::AppServiceProxy::CreateLocalBlockDialog(
    const std::string& app_name) {
  views::DialogDelegate::CreateDialogWidget(
      new AppLocalBlockDialogView(app_name), nullptr, nullptr)
      ->Show();
}

AppLocalBlockDialogView::AppLocalBlockDialogView(const std::string& app_name)
    : AppDialogView(ui::ImageModel::FromVectorIcon(kGuardianIcon,
                                                   ui::kColorIcon,
                                                   kIconSize)) {
  SetTitle(l10n_util::GetStringFUTF16(IDS_APP_LOCAL_BLOCK_PROMPT_TITLE,
                                      base::UTF8ToUTF16(app_name),
                                      ui::GetChromeOSDeviceName()));

  std::u16string heading_text =
      l10n_util::GetStringUTF16(IDS_APP_LOCAL_BLOCK_HEADING);

  InitializeView(heading_text);

  g_app_local_block_dialog_view = this;
}

AppLocalBlockDialogView::~AppLocalBlockDialogView() {
  g_app_local_block_dialog_view = nullptr;
}

// static
AppLocalBlockDialogView* AppLocalBlockDialogView::GetActiveViewForTesting() {
  return g_app_local_block_dialog_view;
}
