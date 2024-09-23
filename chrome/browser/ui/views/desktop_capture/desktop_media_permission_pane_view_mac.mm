// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/desktop_capture/desktop_media_permission_pane_view_mac.h"

#include "base/apple/foundation_util.h"
#include "base/mac/launch_application.h"
#include "base/mac/mac_util.h"
#include "base/metrics/user_metrics.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/component_extension_resources.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_features.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_util_mac.h"
#include "ui/lottie/animation.h"
#include "ui/views/background.h"
#include "ui/views/controls/animated_image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"

BASE_FEATURE(kAnimationForDesktopCapturePermissionChecker,
             "AnimationForDesktopCapturePermissionChecker",
             base::FEATURE_ENABLED_BY_DEFAULT);

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
  NOTREACHED();
}

std::unique_ptr<views::View> MakeToggleAnimation() {
  if (!base::FeatureList::IsEnabled(
          kAnimationForDesktopCapturePermissionChecker)) {
    return nullptr;
  }

  NSImage* app_icon = [NSImage imageNamed:NSImageNameApplicationIcon];
  if (!app_icon) {
    return nullptr;
  }

  std::optional<std::vector<uint8_t>> toggle_animation =
      ui::ResourceBundle::GetSharedInstance().GetLottieData(
          SCREEN_SHARING_TOGGLE_ANIMATION_JSON);
  if (!toggle_animation.has_value()) {
    return nullptr;
  }

  auto animation_container = std::make_unique<views::View>();
  views::BoxLayout* animation_layout =
      animation_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal));
  animation_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kCenter);
  animation_layout->set_between_child_spacing(50);

  views::ImageView* logo_image_view =
      animation_container->AddChildView(std::make_unique<views::ImageView>());
  logo_image_view->SetImage(gfx::ImageSkiaFromNSImage(app_icon));
  logo_image_view->SetImageSize(gfx::Size(55, 55));
  // Adds a margin on the left side of the logo to balance the right margin that
  // is included in the toggle animation. This visually centers the content of
  // |animation_container|.
  logo_image_view->SetBorder(
      views::CreateEmptyBorder(gfx::Insets::TLBR(0, 30, 0, 0)));

  views::AnimatedImageView* animation = animation_container->AddChildView(
      std::make_unique<views::AnimatedImageView>());
  animation->SetAnimatedImage(std::make_unique<lottie::Animation>(
      cc::SkottieWrapper::UnsafeCreateSerializable(
          std::move(*toggle_animation))));

  animation->SetHorizontalAlignment(views::ImageViewBase::Alignment::kTrailing);
  animation->SetImageSize(gfx::Size(80, 80));
  animation->Play();
  return animation_container;
}

}  // namespace

DesktopMediaPermissionPaneViewMac::DesktopMediaPermissionPaneViewMac(
    DesktopMediaList::Type type,
    base::RepeatingCallback<void()> open_screen_recording_settings_callback)
    : type_(type),
      open_screen_recording_settings_callback_(
          open_screen_recording_settings_callback
              ? open_screen_recording_settings_callback
              : base::BindRepeating(&base::mac::OpenSystemSettingsPane,
                                    base::mac::SystemSettingsPane::
                                        kPrivacySecurity_ScreenRecording,
                                    /*id_param=*/"")) {
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

  if (std::unique_ptr<views::View> toggle_view = MakeToggleAnimation()) {
    AddChildView(std::move(toggle_view));
  }
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
  button_ =
      button_container->AddChildView(std::make_unique<views::MdTextButton>(
          base::BindRepeating(
              &DesktopMediaPermissionPaneViewMac::
                  OpenScreenRecordingSettingsPane,
              base::Unretained(this)),
          l10n_util::GetStringUTF16(
              IDS_DESKTOP_MEDIA_PICKER_PERMISSION_BUTTON_MAC)));
  button_->SetStyle(ui::ButtonStyle::kProminent);
}

DesktopMediaPermissionPaneViewMac::~DesktopMediaPermissionPaneViewMac() =
    default;

bool DesktopMediaPermissionPaneViewMac::WasPermissionButtonClicked() const {
  return clicked_;
}

void DesktopMediaPermissionPaneViewMac::SimulateClickForTesting() {
  button_->AcceleratorPressed(ui::Accelerator());
}

void DesktopMediaPermissionPaneViewMac::OpenScreenRecordingSettingsPane() {
  clicked_ = true;
  switch (type_) {
    case DesktopMediaList::Type::kScreen:
    case DesktopMediaList::Type::kWindow:
      RecordAction(base::UserMetricsAction(
          type_ == DesktopMediaList::Type::kScreen
              ? "GetDisplayMedia.PermissionPane.Screen.ClickedButton"
              : "GetDisplayMedia.PermissionPane.Window.ClickedButton"));
      base::ThreadPool::PostTask(FROM_HERE,
                                 open_screen_recording_settings_callback_);
      return;

    case DesktopMediaList::Type::kNone:
    case DesktopMediaList::Type::kWebContents:
    case DesktopMediaList::Type::kCurrentTab:
      break;
  }
  NOTREACHED();
}

BEGIN_METADATA(DesktopMediaPermissionPaneViewMac)
END_METADATA
