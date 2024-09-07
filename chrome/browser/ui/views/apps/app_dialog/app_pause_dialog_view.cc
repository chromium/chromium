// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/app_dialog/app_pause_dialog_view.h"

#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/window/dialog_delegate.h"

namespace {

AppPauseDialogView* g_app_pause_dialog_view = nullptr;

}  // namespace

// static
void apps::AppServiceProxy::CreatePauseDialog(
    apps::AppType app_type,
    const std::string& app_name,
    const gfx::ImageSkia& image,
    const apps::PauseData& pause_data,
    apps::AppServiceProxy::OnPauseDialogClosedCallback closed_callback) {
  views::DialogDelegate::CreateDialogWidget(
      new AppPauseDialogView(app_type, app_name, image, pause_data,
                             std::move(closed_callback)),
      nullptr, nullptr)
      ->Show();
}

AppPauseDialogView::AppPauseDialogView(
    apps::AppType app_type,
    const std::string& app_name,
    const gfx::ImageSkia& image,
    const apps::PauseData& pause_data,
    apps::AppServiceProxy::OnPauseDialogClosedCallback closed_callback)
    : AppDialogView(ui::ImageModel::FromImageSkia(image)) {
  closed_callback_ = std::move(closed_callback);

  InitializeView();
  AddTitle(l10n_util::GetStringFUTF16(IDS_APP_PAUSE_PROMPT_TITLE,
                                      base::UTF8ToUTF16(app_name)));

  const int cutoff = pause_data.minutes == 0 || pause_data.hours == 0 ? 0 : -1;
  std::u16string subtitle_text = l10n_util::GetStringFUTF16(
      (app_type == apps::AppType::kWeb) ? IDS_APP_PAUSE_HEADING_FOR_WEB_APPS
                                        : IDS_APP_PAUSE_HEADING,
      base::UTF8ToUTF16(app_name),
      ui::TimeFormat::Detailed(
          ui::TimeFormat::Format::FORMAT_DURATION,
          ui::TimeFormat::Length::LENGTH_LONG, cutoff,
          base::Hours(pause_data.hours) + base::Minutes(pause_data.minutes)));

  AddSubtitle(subtitle_text);

  g_app_pause_dialog_view = this;
}

AppPauseDialogView::~AppPauseDialogView() {
  g_app_pause_dialog_view = nullptr;
  if (closed_callback_)
    std::move(closed_callback_).Run();
}

// static
AppPauseDialogView* AppPauseDialogView::GetActiveViewForTesting() {
  return g_app_pause_dialog_view;
}
