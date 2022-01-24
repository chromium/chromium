// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/ui/media_control_ui.h"

#include <algorithm>
#include <string>

#include "base/bind.h"
#include "base/cxx17_backports.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_checker.h"
#include "chromecast/graphics/cast_window_manager.h"
#include "ui/aura/window.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"

#define LOG_VIEW(name) DVLOG(1) << #name << ": " << name->bounds().ToString();

namespace chromecast {

namespace {

constexpr base::TimeDelta kUpdateMediaTimePeriod = base::Seconds(1);
const int kButtonSmallHeight = 56;
const int kButtonBigHeight = 124;

void SetButtonImage(views::ImageButton* button, const gfx::VectorIcon& icon) {
  button->SetImage(
      views::Button::STATE_NORMAL,
      gfx::CreateVectorIcon(icon, button->height(), SK_ColorWHITE));
}

// A view that invokes an |on_tapped| closure whenever it detects a tap gesture.
class TouchView : public views::View {
 public:
  METADATA_HEADER(TouchView);
  explicit TouchView(base::RepeatingClosure on_tapped)
      : on_tapped_(std::move(on_tapped)) {}
  TouchView(const TouchView&) = delete;
  TouchView& operator=(const TouchView&) = delete;

 private:
  // views::View implementation:
  void OnGestureEvent(ui::GestureEvent* event) override {
    if (event->type() == ui::ET_GESTURE_TAP) {
      on_tapped_.Run();
    }
  }

  base::RepeatingClosure on_tapped_;
};

BEGIN_METADATA(TouchView, views::View)
END_METADATA

}  // namespace

MediaControlUi::MediaControlUi(CastWindowManager* window_manager)
    : window_manager_(window_manager),
      app_is_fullscreen_(false),
      is_paused_(false),
      media_duration_(-1.0),
      last_media_time_(0.0),
      weak_factory_(this) {
  DCHECK(window_manager_);

  // |touch_view| will detect touch events and decide whether to show
  // or hide |view_|, which contains the media controls.
  auto touch_view = std::make_unique<TouchView>(base::BindRepeating(
      &MediaControlUi::OnTapped, weak_factory_.GetWeakPtr()));

  // Main view.
  auto view = std::make_unique<views::View>();
  view->SetVisible(false);
  view->SetBackground(
      views::CreateSolidBackground(SkColorSetA(SK_ColorBLACK, 0x80)));
  view->SetBoundsRect(
      window_manager_->GetRootWindow()->GetBoundsInRootWindow());
  view_ = touch_view->AddChildView(std::move(view));

  // Buttons.
  btn_previous_ = view_->AddChildView(
      CreateImageButton(mojom::MediaCommand::PREVIOUS,
                        vector_icons::kPreviousIcon, kButtonSmallHeight));
  btn_play_pause_ = view_->AddChildView(
      CreateImageButton(mojom::MediaCommand::TOGGLE_PLAY_PAUSE,
                        vector_icons::kPlayIcon, kButtonBigHeight));
  btn_next_ = view_->AddChildView(CreateImageButton(
      mojom::MediaCommand::NEXT, vector_icons::kNextIcon, kButtonSmallHeight));
  btn_replay30_ = view_->AddChildView(
      CreateImageButton(mojom::MediaCommand::REPLAY_30_SECONDS,
                        vector_icons::kBack30Icon, kButtonSmallHeight));
  btn_forward30_ = view_->AddChildView(
      CreateImageButton(mojom::MediaCommand::FORWARD_30_SECONDS,
                        vector_icons::kForward30Icon, kButtonSmallHeight));

  // Labels.
  lbl_title_ =
      view_->AddChildView(std::make_unique<views::Label>(std::u16string()));
  lbl_meta_ =
      view_->AddChildView(std::make_unique<views::Label>(std::u16string()));

  // Progress Bar.
  progress_bar_ = view_->AddChildView(std::make_unique<views::ProgressBar>());

  LayoutElements();

  // Main widget.
  widget_.reset(new views::Widget);
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.context = window_manager_->GetRootWindow();
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.bounds = window_manager_->GetRootWindow()->GetBoundsInRootWindow();
  widget_->Init(std::move(params));
  widget_->SetContentsView(std::move(touch_view));

  window_manager_->SetZOrder(widget_->GetNativeView(),
                             mojom::ZOrder::MEDIA_INFO);
}

MediaControlUi::~MediaControlUi() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void MediaControlUi::SetClient(
    mojo::PendingRemote<mojom::MediaControlClient> client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  client_.Bind(std::move(client));
  MaybeShowWidget();
}

void MediaControlUi::SetAttributes(
    mojom::MediaControlUiAttributesPtr attributes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  last_media_timestamp_ = base::TimeTicks::Now();
  last_media_time_ = attributes->current_time;

  // Only update the media time if the player is playing.
  if (attributes->is_playing) {
    media_time_update_timer_.Start(
        FROM_HERE, kUpdateMediaTimePeriod,
        base::BindRepeating(&MediaControlUi::UpdateMediaTime,
                            weak_factory_.GetWeakPtr()));
  } else {
    media_time_update_timer_.Stop();
  }

  if (attributes->is_paused != is_paused_) {
    is_paused_ = attributes->is_paused;
    if (is_paused_ && !visible()) {
      ShowMediaControls(true);
    }
    SetButtonImage(btn_play_pause_, is_paused_ ? vector_icons::kPlayIcon
                                               : vector_icons::kPauseIcon);
  }

  if (attributes->duration > 0.0) {
    media_duration_ = attributes->duration;
  } else if (attributes->duration == 0.0) {
    media_duration_ = -1.0;
  }

  if (attributes->duration > 0.0) {
    progress_bar_->SetVisible(true);
  } else if (attributes->duration == 0.0) {
    progress_bar_->SetVisible(false);
  }

  lbl_title_->SetText(base::UTF8ToUTF16(attributes->title));
  lbl_meta_->SetText(base::UTF8ToUTF16(attributes->metadata));
}

void MediaControlUi::SetBounds(const gfx::Rect& new_bounds) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto approx_equals = [](int a, int b) {
    const int epsilon = 20;
    return std::abs(a - b) < epsilon;
  };

  auto root_bounds = window_manager_->GetRootWindow()->bounds();
  app_is_fullscreen_ = approx_equals(root_bounds.width(), new_bounds.width()) &&
                       approx_equals(root_bounds.height(), new_bounds.height());
  MaybeShowWidget();
}

void MediaControlUi::MaybeShowWidget() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (client_ && app_is_fullscreen_) {
    LOG(INFO) << "Enabling platform media controls";
    widget_->Show();
  } else {
    LOG(INFO) << "Disabling platform media controls";
    widget_->Hide();
  }
}

void MediaControlUi::ShowMediaControls(bool visible) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  view_->SetVisible(visible);
}

bool MediaControlUi::visible() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return (view_ && view_->GetVisible());
}

void MediaControlUi::UpdateMediaTime() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  double progress =
      last_media_time_ +
      (base::TimeTicks::Now() - last_media_timestamp_).InSecondsF();
  if (media_duration_ > 0.0) {
    progress = base::clamp(progress, 0.0, media_duration_);
    progress_bar_->SetValue(progress / media_duration_);
  }
}

void MediaControlUi::ButtonPressed(mojom::MediaCommand command) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (client_)
    client_->Execute(command);
}

void MediaControlUi::OnTapped() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ShowMediaControls(!visible());
}

std::unique_ptr<views::ImageButton> MediaControlUi::CreateImageButton(
    mojom::MediaCommand command,
    const gfx::VectorIcon& icon,
    int height) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto button = std::make_unique<views::ImageButton>(base::BindRepeating(
      &MediaControlUi::ButtonPressed, base::Unretained(this), command));
  button->SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  button->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
  button->SetSize(gfx::Size(height, height));
  button->SetFocusBehavior(views::View::FocusBehavior::ACCESSIBLE_ONLY);
  SetButtonImage(button.get(), icon);

  return button;
}

void MediaControlUi::LayoutElements() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const int kPrevousNextMargin = 112;
  const int kSmallButtonBottomMargin = 56;
  const int kLargeButtonBottomMargin = 22;
  const int kReplayOffset = 236 / 3;
  const int kParentWidth = view_->width();
  const int kParentHeight = view_->height();

  btn_previous_->SetPosition(gfx::Point(
      kPrevousNextMargin,
      kParentHeight - btn_previous_->height() - kSmallButtonBottomMargin));

  btn_next_->SetPosition(gfx::Point(
      kParentWidth - btn_next_->width() - kPrevousNextMargin,
      kParentHeight - btn_next_->height() - kSmallButtonBottomMargin));

  btn_play_pause_->SetPosition(gfx::Point(
      (kParentWidth - btn_play_pause_->width()) / 2,
      kParentHeight - btn_play_pause_->height() - kLargeButtonBottomMargin));

  btn_replay30_->SetPosition(gfx::Point(
      kParentWidth / 2 - kReplayOffset - btn_replay30_->width(),
      kParentHeight - btn_replay30_->height() - kSmallButtonBottomMargin));

  btn_forward30_->SetPosition(gfx::Point(
      kParentWidth / 2 + kReplayOffset,
      kParentHeight - btn_forward30_->height() - kSmallButtonBottomMargin));

  const int kProgressMargin = 56;
  const int kProgressBarHeight = 5;

  progress_bar_->SetBounds(
      kPrevousNextMargin, btn_previous_->y() - kProgressMargin,
      kParentWidth - kPrevousNextMargin * 2, kProgressBarHeight);

  const int kTitleMargin = 56;
  const int kTitleLineHeight = 80;
  const int kMetadataMargin = 20;
  const int kMetadataLineHeight = 24;

  lbl_title_->SetFontList(gfx::FontList("GoogleSans, 68px"));
  lbl_title_->SetLineHeight(kTitleLineHeight);
  lbl_title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  lbl_title_->SetAutoColorReadabilityEnabled(false);
  lbl_title_->SetEnabledColor(SkColorSetA(SK_ColorWHITE, 0xFF));
  lbl_title_->SetBounds(
      kPrevousNextMargin, progress_bar_->y() - kTitleMargin - kTitleLineHeight,
      kParentWidth - 2 * kPrevousNextMargin, kTitleLineHeight);

  lbl_meta_->SetFontList(gfx::FontList("GoogleSans, 24px"));
  lbl_meta_->SetLineHeight(kMetadataLineHeight);
  lbl_meta_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  lbl_meta_->SetAutoColorReadabilityEnabled(false);
  lbl_meta_->SetEnabledColor(SkColorSetA(SK_ColorWHITE, 0xFF * 0.7));
  lbl_meta_->SetBounds(kPrevousNextMargin,
                       lbl_title_->y() - kMetadataMargin - kMetadataLineHeight,
                       kParentWidth - 2 * kPrevousNextMargin,
                       kMetadataLineHeight);

  LOG_VIEW(view_);
  LOG_VIEW(btn_previous_);
  LOG_VIEW(btn_next_);
  LOG_VIEW(btn_play_pause_);
  LOG_VIEW(btn_replay30_);
  LOG_VIEW(btn_forward30_);
  LOG_VIEW(progress_bar_);
  LOG_VIEW(lbl_title_);
  LOG_VIEW(lbl_meta_);
}

}  // namespace chromecast
