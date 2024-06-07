// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/desktop_capture/desktop_media_permission_pane_view.h"

#include "base/metrics/user_metrics.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_features.h"
#include "ui/compositor/layer.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

namespace {

BASE_FEATURE(kDesktopCapturePermissionCheckerRestartMessage,
             "DesktopCapturePermissionCheckerRestartMessage",
             base::FEATURE_ENABLED_BY_DEFAULT);

std::u16string WithRestartMessage(int message_id) {
  return l10n_util::GetStringUTF16(message_id) + u"\n" +
         l10n_util::GetStringUTF16(
             IDS_DESKTOP_MEDIA_PICKER_PERMISSION_RESTART_TEXT_MAC);
}

std::u16string GetLabelText(DesktopMediaList::Type type) {
  switch (type) {
    case DesktopMediaList::Type::kScreen:
      return base::FeatureList::IsEnabled(
                 kDesktopCapturePermissionCheckerRestartMessage)
                 ? WithRestartMessage(
                       IDS_DESKTOP_MEDIA_PICKER_SCREEN_PERMISSION_TEXT_PERIOD_MAC)
                 : l10n_util::GetStringUTF16(
                       IDS_DESKTOP_MEDIA_PICKER_SCREEN_PERMISSION_TEXT_MAC);

    case DesktopMediaList::Type::kWindow:
      return base::FeatureList::IsEnabled(
                 kDesktopCapturePermissionCheckerRestartMessage)
                 ? WithRestartMessage(
                       IDS_DESKTOP_MEDIA_PICKER_WINDOW_PERMISSION_TEXT_PERIOD_MAC)
                 : l10n_util::GetStringUTF16(
                       IDS_DESKTOP_MEDIA_PICKER_WINDOW_PERMISSION_TEXT_MAC);

    case DesktopMediaList::Type::kNone:
    case DesktopMediaList::Type::kWebContents:
    case DesktopMediaList::Type::kCurrentTab:
      break;
  }
  NOTREACHED_NORETURN();
}

}  // namespace

DesktopMediaPermissionPaneView::DesktopMediaPermissionPaneView(
    DesktopMediaList::Type type)
    : type_(type) {
  SetBackground(
      views::CreateThemedRoundedRectBackground(ui::kColorSysSurface4,
                                               /*top_radius=*/0.0f,
                                               /*bottom_radius=*/8.0f));
  const ChromeLayoutProvider* const provider = ChromeLayoutProvider::Get();
  views::BoxLayout* layout =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical, gfx::Insets(0),
          provider->GetDistanceMetric(
              DISTANCE_UNRELATED_CONTROL_VERTICAL_LARGE)));
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);

  views::Label* label =
      AddChildView(std::make_unique<views::Label>(GetLabelText(type_)));
  label->SetMultiLine(true);

  View* button_container = AddChildView(std::make_unique<views::View>());
  views::BoxLayout* button_layout =
      button_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal));
  button_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kCenter);
  // Unretained safe because button is (transitively) owned by `this`.
  views::MdTextButton* button =
      button_container->AddChildView(std::make_unique<views::MdTextButton>(
          base::BindRepeating(
              &DesktopMediaPermissionPaneView::OpenScreenRecordingSettingsPane,
              base::Unretained(this)),
          l10n_util::GetStringUTF16(
              IDS_DESKTOP_MEDIA_PICKER_PERMISSION_BUTTON_MAC)));
  button->SetStyle(ui::ButtonStyle::kProminent);
}

DesktopMediaPermissionPaneView::~DesktopMediaPermissionPaneView() = default;

bool DesktopMediaPermissionPaneView::WasPermissionButtonClicked() const {
  return clicked_;
}

void DesktopMediaPermissionPaneView::OpenScreenRecordingSettingsPane() {
  clicked_ = true;
  switch (type_) {
    case DesktopMediaList::Type::kScreen:
    case DesktopMediaList::Type::kWindow:
      RecordAction(base::UserMetricsAction(
          type_ == DesktopMediaList::Type::kScreen
              ? "GetDisplayMedia.PermissionPane.Screen.ClickedButton"
              : "GetDisplayMedia.PermissionPane.Window.ClickedButton"));
      base::ThreadPool::PostTask(
          FROM_HERE,
          base::BindOnce(
              &base::mac::OpenSystemSettingsPane,
              base::mac::SystemSettingsPane::kPrivacySecurity_ScreenRecording,
              ""));
      return;

    case DesktopMediaList::Type::kNone:
    case DesktopMediaList::Type::kWebContents:
    case DesktopMediaList::Type::kCurrentTab:
      break;
  }
  NOTREACHED_NORETURN();
}

BEGIN_METADATA(DesktopMediaPermissionPaneView)
END_METADATA
