// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/ui/media_overlay_impl.h"

#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chromecast/app/grit/shell_resources.h"
#include "chromecast/graphics/cast_window_manager.h"
#include "media/base/audio_decoder_config.h"
#include "ui/aura/window.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace chromecast {

namespace {

constexpr SkColor kBackgroundColor = SkColorSetA(SK_ColorBLACK, 200);
constexpr int kElementSpacing = 16;
constexpr int kVolumeBarHeight = 16;
constexpr int kVolumePopupPadding = 16;
constexpr int kVolumePopupBottomInset = 32;
constexpr base::TimeDelta kUiHideDelay = base::Seconds(3);

}  // namespace

MediaOverlayImpl::MediaOverlayImpl(CastWindowManager* window_manager)
    : window_manager_(window_manager),
      ui_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      controller_(nullptr),
      volume_icon_image_(ui::ResourceBundle::GetSharedInstance().GetImageNamed(
          IDR_CAST_SHELL_SPEAKER_ICON)),
      toast_font_list_(gfx::FontList("Helvetica, 28px")),
      weak_factory_(this) {
  DCHECK(window_manager_);

  gfx::Rect window_bounds = window_manager_->GetRootWindow()->bounds();
  auto container_view = std::make_unique<views::View>();
  auto layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets::TLBR(0, window_bounds.width() / 4, kVolumePopupBottomInset,
                        window_bounds.width() / 4),
      0);
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kEnd);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);
  container_view->SetLayoutManager(std::move(layout));

  AddToast(container_view.get());
  AddVolumeBar(container_view.get());

  overlay_widget_ =
      CreateOverlayWidget(window_bounds, std::move(container_view));
  overlay_widget_->Show();
  volume_panel_->SetVisible(false);
  toast_label_->SetVisible(false);

  media::MediaPipelineObserver::AddObserver(this);
}

MediaOverlayImpl::~MediaOverlayImpl() {
  media::MediaPipelineObserver::RemoveObserver(this);
}

void MediaOverlayImpl::AddVolumeBar(views::View* container) {
  std::unique_ptr<views::ImageView> volume_icon =
      std::make_unique<views::ImageView>();
  volume_icon->SetImage(volume_icon_image_.ToImageSkia());
  volume_icon->SetImageSize(gfx::Size(25, 19));

  volume_bar_ = new views::ProgressBar(kVolumeBarHeight, false);
  volume_bar_->SetBackgroundColor(kBackgroundColor);

  volume_panel_ = new views::View();
  auto layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets(kVolumePopupPadding), kElementSpacing);
  auto* layout_ptr = volume_panel_->SetLayoutManager(std::move(layout));
  layout_ptr->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  volume_panel_->SetBackground(views::CreateSolidBackground(kBackgroundColor));
  volume_panel_->AddChildView(volume_icon.release());
  volume_panel_->AddChildView(volume_bar_);
  layout_ptr->SetFlexForView(volume_bar_, 1);

  container->AddChildView(volume_panel_);
}

void MediaOverlayImpl::AddToast(views::View* container) {
  toast_label_ = new views::Label(u"");
  toast_label_->SetFontList(toast_font_list_);
  toast_label_->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_CENTER);

  // Background color of the label is set to auto-compute the best
  // constrasting color for the displayed text. If the background color is not
  // opaque, we need to take into account the color underneath, which in worst
  // case scenario will be white
  toast_label_->SetBackgroundColor(
      color_utils::GetResultingPaintColor(kBackgroundColor, SK_ColorWHITE));
  toast_label_->SetBackground(views::CreateSolidBackground(kBackgroundColor));
  toast_label_->SetBorder(views::CreateEmptyBorder(kVolumePopupPadding));
  toast_label_->SetMultiLine(true);

  container->AddChildView(toast_label_);
}

void MediaOverlayImpl::SetController(Controller* controller) {
  controller_ = controller;
  NotifyController();
}

void MediaOverlayImpl::ShowMessage(const std::u16string& message) {
  if (!ui_task_runner_->BelongsToCurrentThread()) {
    ui_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&MediaOverlayImpl::ShowMessage,
                                  weak_factory_.GetWeakPtr(), message));
    return;
  }
  ShowToast(message);
}

void MediaOverlayImpl::ShowVolumeBar(float volume) {
  if (!ui_task_runner_->BelongsToCurrentThread()) {
    ui_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&MediaOverlayImpl::ShowVolumeBar,
                                  weak_factory_.GetWeakPtr(), volume));
    return;
  }

  volume_bar_->SetValue(volume);
  volume_panel_->SetVisible(true);
  overlay_widget_->GetContentsView()->Layout();

  volume_widget_timer_.Start(FROM_HERE, kUiHideDelay, this,
                             &MediaOverlayImpl::HideVolumeWidget);
}

void MediaOverlayImpl::HideVolumeWidget() {
  volume_panel_->SetVisible(false);
}

void MediaOverlayImpl::ShowToast(const std::u16string& text) {
  toast_label_->SetText(text);
  toast_label_->SizeToFit(volume_panel_->bounds().width());
  toast_label_->SetVisible(true);
  overlay_widget_->GetContentsView()->Layout();

  toast_visible_timer_.Start(FROM_HERE, kUiHideDelay, this,
                             &MediaOverlayImpl::HideToast);
}

void MediaOverlayImpl::HideToast() {
  toast_label_->SetVisible(false);
}

std::unique_ptr<views::Widget> MediaOverlayImpl::CreateOverlayWidget(
    const gfx::Rect& bounds,
    std::unique_ptr<views::View> content_view) {
  std::unique_ptr<views::Widget> widget = std::make_unique<views::Widget>();

  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.context = window_manager_->GetRootWindow();
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.bounds = bounds;
  params.accept_events = false;

  widget->Init(std::move(params));
  widget->SetContentsView(std::move(content_view));
  window_manager_->SetZOrder(widget->GetNativeView(), mojom::ZOrder::VOLUME);

  return widget;
}

void MediaOverlayImpl::OnAudioPipelineInitialized(
    media::MediaPipelineImpl* pipeline,
    const ::media::AudioDecoderConfig& config) {
  if (config.codec() == ::media::AudioCodec::kAC3 ||
      config.codec() == ::media::AudioCodec::kEAC3 ||
      config.codec() == ::media::AudioCodec::kDTS ||
      config.codec() == ::media::AudioCodec::kDTSXP2 ||
      config.codec() == ::media::AudioCodec::kMpegHAudio) {
    passthrough_pipelines_.insert(pipeline);
  }

  NotifyController();
}

void MediaOverlayImpl::OnPipelineDestroyed(media::MediaPipelineImpl* pipeline) {
  passthrough_pipelines_.erase(pipeline);
  NotifyController();
}

void MediaOverlayImpl::NotifyController() {
  if (!controller_)
    return;
  controller_->SetSurroundSoundInUse(!passthrough_pipelines_.empty());
}

}  // namespace chromecast
