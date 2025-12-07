// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/app_dialog/app_local_block_dialog_view.h"

#include "base/containers/contains.h"
#include "base/metrics/histogram_functions.h"
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

constexpr char kOnDeviceControlsBlockDialogHistogram[] =
    "ChromeOS.OnDeviceControls.BlockedAppDialogShown";

// Used for metrics. Those values are logged to UMA. Entries should not be
// renumbered and numeric values should never be reused. Please keep in sync
// with "OnDeviceControlsBlockedAppDialog" in
// src/tools/metrics/histograms/metadata/families/enums.xml.
enum class OnDeviceControlsBlockedAppDialog {
  kDialogShown = 0,
  kDialogReplaced = 1,
  kMaxValue = kDialogReplaced,
};

}  // namespace

// static
void apps::AppServiceProxy::CreateLocalBlockDialog(
    const std::string& app_name) {
  if (g_app_local_block_dialog_view) {
    g_app_local_block_dialog_view->AddApp(app_name);
    base::UmaHistogramEnumeration(
        kOnDeviceControlsBlockDialogHistogram,
        OnDeviceControlsBlockedAppDialog::kDialogReplaced);
    return;
  }

  views::DialogDelegate::CreateDialogWidget(
      new AppLocalBlockDialogView(app_name), nullptr, nullptr)
      ->Show();
  base::UmaHistogramEnumeration(kOnDeviceControlsBlockDialogHistogram,
                                OnDeviceControlsBlockedAppDialog::kDialogShown);
}

AppLocalBlockDialogView::AppLocalBlockDialogView(const std::string& app_name)
    : AppDialogView(ui::ImageModel::FromVectorIcon(kGuardianIcon,
                                                   ui::kColorIcon,
                                                   kIconSize)) {
  InitializeView();
  AddTitle(/*title_text=*/std::u16string());

  // This needs to be called after `InitializeView()` and `AddTitle()` because
  // it sets the title.
  AddApp(app_name);

  AddSubtitle(l10n_util::GetStringUTF16(IDS_APP_LOCAL_BLOCK_HEADING));

  DCHECK_EQ(nullptr, g_app_local_block_dialog_view);
  g_app_local_block_dialog_view = this;
}

AppLocalBlockDialogView::~AppLocalBlockDialogView() {
  DCHECK_EQ(this, g_app_local_block_dialog_view);
  g_app_local_block_dialog_view = nullptr;
}

// static
AppLocalBlockDialogView* AppLocalBlockDialogView::GetActiveViewForTesting() {
  return g_app_local_block_dialog_view;
}

void AppLocalBlockDialogView::AddApp(const std::string& app_name) {
  if (base::Contains(app_names_, app_name)) {
    return;
  }

  app_names_.emplace_back(app_name);

  // There are only 2 different title strings. Skip unnecessary updates when
  // more than 2 apps are blocked.
  const size_t num_of_blocked_apps = app_names_.size();
  if (num_of_blocked_apps > 2) {
    return;
  }

  const int title_string_id = num_of_blocked_apps == 1
                                  ? IDS_APP_LOCAL_BLOCK_PROMPT_TITLE
                                  : IDS_APP_LOCAL_BLOCK_PROMPT_MULTIPLE_TITLE;

  SetTitleText(l10n_util::GetStringFUTF16(title_string_id,
                                          base::UTF8ToUTF16(app_names_[0]),
                                          ui::GetChromeOSDeviceName()));
}
