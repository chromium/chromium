// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/desktop_capture/audio_permission_warning_view.h"

#include "base/mac/mac_util.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ui/views/desktop_capture/audio_capture_permission_checker.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_types.h"
#include "ui/color/color_id.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"

AudioPermissionWarningView::AudioPermissionWarningView(
    base::RepeatingCallback<void()> cancel_callback)
    : cancel_callback_(cancel_callback) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(12)));

  auto* content_wrapper = AddChildView(std::make_unique<views::View>());
  content_wrapper->SetBackground(views::CreateRoundedRectBackground(
      ui::kColorSysSurface, /*top_radius=*/10.0f, /*bottom_radius=*/10.0f));
  auto* content_wrapper_layout =
      content_wrapper->SetLayoutManager(std::make_unique<views::FlexLayout>());
  content_wrapper_layout->SetOrientation(views::LayoutOrientation::kVertical);
  content_wrapper_layout->SetInteriorMargin(gfx::Insets(12));

  label_ = content_wrapper->AddChildView(
      std::make_unique<views::Label>(l10n_util::GetStringUTF16(
          IDS_DESKTOP_MEDIA_PICKER_SYSTEM_AUDIO_PERMISSION_TEXT_MAC)));

  label_->SetMultiLine(true);
  label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label_->SetTextStyle(views::style::STYLE_BODY_4);
  label_->GetViewAccessibility().SetRole(ax::mojom::Role::kAlert);

  auto* button_container =
      content_wrapper->AddChildView(std::make_unique<views::View>());
  auto* button_layout =
      button_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, gfx::Insets(8),
          /*between_child_spacing=*/8));
  button_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kEnd);

  // Cancel button.
  cancel_button_ =
      button_container->AddChildView(std::make_unique<views::MdTextButton>(
          cancel_callback_, l10n_util::GetStringUTF16(IDS_APP_CANCEL)));
  cancel_button_->SetStyle(ui::ButtonStyle::kText);

  // Open system settings button.
  system_settings_button_ =
      button_container->AddChildView(std::make_unique<views::MdTextButton>(
          base::BindRepeating(&AudioPermissionWarningView::OpenSystemSettings,
                              base::Unretained(this)),
          l10n_util::GetStringUTF16(
              IDS_DESKTOP_MEDIA_PICKER_PERMISSION_BUTTON_MAC)));

  AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));
}

AudioPermissionWarningView::~AudioPermissionWarningView() = default;

void AudioPermissionWarningView::SetWarningVisible(bool visible) {
  // Child views may remain in focus even though their parent is hidden,
  // unless their focus behavior is set to 'never'.
  cancel_button_->SetFocusBehavior(visible ? FocusBehavior::ACCESSIBLE_ONLY
                                           : FocusBehavior::NEVER);
  system_settings_button_->SetFocusBehavior(
      visible ? FocusBehavior::ACCESSIBLE_ONLY : FocusBehavior::NEVER);
  SetVisible(visible);
  if (visible) {
    label_->GetViewAccessibility().NotifyEvent(ax::mojom::Event::kAlert, true);
  }
}

void AudioPermissionWarningView::OpenSystemSettings() {
  if (!system_settings_opened_logged_) {
    RecordUmaAudioCapturePermissionCheckerInteractions(
        AudioCapturePermissionCheckerInteractions::
            kSystemSettingsOpenedAfterDenial);
    system_settings_opened_logged_ = true;
  }
  base::ThreadPool::PostTask(
      FROM_HERE,
      base::BindOnce(
          &base::mac::OpenSystemSettingsPane,
          base::mac::SystemSettingsPane::kPrivacySecurity_ScreenRecording,
          /*id_param=*/""));
}

bool AudioPermissionWarningView::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  CHECK_EQ(accelerator.key_code(), ui::VKEY_ESCAPE);
  cancel_callback_.Run();
  return true;
}

BEGIN_METADATA(AudioPermissionWarningView)
END_METADATA
