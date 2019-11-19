// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/ui/media_control_ui.h"

#include <algorithm>

#include "base/bind.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_checker.h"
#include "chromecast/graphics/cast_window_manager.h"
#include "ui/aura/window.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"

#define LOG_VIEW(name) DVLOG(1) << #name << ": " << name->bounds().ToString();

namespace chromecast {

namespace {

constexpr base::TimeDelta kUpdateMediaTimePeriod =
    base::TimeDelta::FromSeconds(1);
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
  explicit TouchView(base::RepeatingClosure on_tapped)
      : on_tapped_(std::move(on_tapped)) {}

 private:
  // views::View implementation:
  void OnGestureEvent(ui::GestureEvent* event) override {
    if (event->type() == ui::ET_GESTURE_TAP) {
      on_tapped_.Run();
    }
  }

  base::RepeatingClosure on_tapped_;

  DISALLOW_COPY_AND_ASSIGN(TouchView);
};

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
  touch_view_ = std::make_unique<TouchView>(base::BindRepeating(
      &MediaControlUi::OnTapped, weak_factory_.GetWeakPtr()));

  // Main view.
  view_ = std::make_unique<views::View>();
  view_->set_owned_by_client();
  view_->SetVisible(false);
  view_->SetBackground(
      views::CreateSolidBackground(SkColorSetA(SK_ColorBLACK, 0x80)));
  view_->SetBoundsRect(
      window_manager_->GetRootWindow()->GetBoundsInRootWindow());
  touch_view_->AddChildView(view_.get());

  // Buttons.
  btn_previous_ =
      CreateImageButton(vector_icons::kPreviousIcon, kButtonSmallHeight);
  btn_previous_->set_owned_by_client();
  view_->AddChildView(btn_previous_.get());
  btn_play_pause_ =
      CreateImageButton(vector_icons::kPlayIcon, kButtonBigHeight);
  btn_play_pause_->set_owned_by_client();
  view_->AddChildView(btn_play_pause_.get());
  btn_next_ = CreateImageButton(vector_icons::kNextIcon, kButtonSmallHeight);
  btn_next_->set_owned_by_client();
  view_->AddChildView(btn_next_.get());
  btn_replay30_ =
      CreateImageButton(vector_icons::kBack30Icon, kButtonSmallHeight);
  btn_replay30_->set_owned_by_client();
  view_->AddChildView(btn_replay30_.get());
  btn_forward30_ =
      CreateImageButton(vector_icons::kForward30Icon, kButtonSmallHeight);
  btn_forward30_->set_owned_by_client();
  view_->AddChildView(btn_forward30_.get());

  // Labels.
  lbl_title_ = std::make_unique<views::Label>(base::string16());
  lbl_title_->set_owned_by_client();
  view_->AddChildView(lbl_title_.get());
  lbl_meta_ = std::make_unique<views::Label>(base::string16());
  lbl_meta_->set_owned_by_client();
  view_->AddChildView(lbl_meta_.get());

  // Progress Bar.
  progress_bar_ = std::make_unique<views::ProgressBar>();
  progress_bar_->set_owned_by_client();
  view_->AddChildView(progress_bar_.get());

  LayoutElements();

  // Main widget.
  widget_.reset(new views::Widget);
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.context = window_manager_->GetRootWindow();
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.bounds = window_manager_->GetRootWindow()->GetBoundsInRootWindow();
  widget_->Init(std::move(params));
  widget_->SetContentsView(touch_view_.release());  // Ownership passed.

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
    SetButtonImage(btn_play_pause_.get(), is_paused_
                                              ? vector_icons::kPlayIcon
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
    progress = std::min(std::max(0.0, progress), media_duration_);
    progress_bar_->SetValue(progress / media_duration_);
  }
}

void MediaControlUi::ButtonPressed(views::Button* sender,
                                   const ui::Event& event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!client_) {
    return;
  }

  if (sender == btn_previous_.get()) {
    client_->Execute(mojom::MediaCommand::PREVIOUS);
  } else if (sender == btn_play_pause_.get()) {
    client_->Execute(mojom::MediaCommand::TOGGLE_PLAY_PAUSE);
  } else if (sender == btn_next_.get()) {
    client_->Execute(mojom::MediaCommand::NEXT);
  } else if (sender == btn_replay30_.get()) {
    client_->Execute(mojom::MediaCommand::REPLAY_30_SECONDS);
  } else if (sender == btn_forward30_.get()) {
    client_->Execute(mojom::MediaCommand::FORWARD_30_SECONDS);
  } else {
    NOTREACHED();
  }
}

void MediaControlUi::OnTapped() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ShowMediaControls(!visible());
}

std::unique_ptr<views::ImageButton> MediaControlUi::CreateImageButton(
    const gfx::VectorIcon& icon,
    int height) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto button = std::make_unique<views::ImageButton>(this);
  button->SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  button->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
  button->SetSize(gfx::Size(height, height));
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
