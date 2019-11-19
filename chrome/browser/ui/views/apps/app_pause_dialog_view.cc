// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/app_pause_dialog_view.h"

#include <memory>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
// static
void apps::AppServiceProxy::CreatePauseDialog(
    const std::string& app_name,
    gfx::ImageSkia image,
    const apps::PauseData& pause_data,
    apps::AppServiceProxy::OnPauseDialogClosedCallback closed_callback) {
  constrained_window::CreateBrowserModalDialogViews(
      new AppPauseDialogView(app_name, image, pause_data,
                             std::move(closed_callback)),
      nullptr)
      ->Show();
}

AppPauseDialogView::AppPauseDialogView(
    const std::string& app_name,
    gfx::ImageSkia image,
    const apps::PauseData& pause_data,
    apps::AppServiceProxy::OnPauseDialogClosedCallback closed_callback)
    : BubbleDialogDelegateView(nullptr, views::BubbleBorder::NONE),
      closed_callback_(std::move(closed_callback)) {
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      provider->GetDialogInsetsForContentType(views::TEXT, views::TEXT),
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_HORIZONTAL)));

  auto* icon_view = AddChildView(std::make_unique<views::ImageView>());
  icon_view->SetImage(image);

  const int cutoff = pause_data.minutes == 0 || pause_data.hours == 0 ? 0 : -1;
  base::string16 heading_text = l10n_util::GetStringFUTF16(
      IDS_APP_PAUSE_HEADING, base::UTF8ToUTF16(app_name),
      ui::TimeFormat::Detailed(
          ui::TimeFormat::Format::FORMAT_DURATION,
          ui::TimeFormat::Length::LENGTH_LONG, cutoff,
          base::TimeDelta::FromHours(pause_data.hours) +
              base::TimeDelta::FromMinutes(pause_data.minutes)));

  auto* label = AddChildView(std::make_unique<views::Label>(heading_text));
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
}

AppPauseDialogView::~AppPauseDialogView() = default;

bool AppPauseDialogView::Accept() {
  std::move(closed_callback_).Run();
  return true;
}

gfx::Size AppPauseDialogView::CalculatePreferredSize() const {
  const int default_width = views::LayoutProvider::Get()->GetDistanceMetric(
                                DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH) -
                            margins().width();
  return gfx::Size(default_width, GetHeightForWidth(default_width));
}

int AppPauseDialogView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_OK;
}

ui::ModalType AppPauseDialogView::GetModalType() const {
  return ui::MODAL_TYPE_SYSTEM;
}

base::string16 AppPauseDialogView::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_APP_PAUSE_PROMPT_TITLE);
}

bool AppPauseDialogView::ShouldShowCloseButton() const {
  return false;
}
