// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/status_bubble_views.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/i18n/rtl.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "cc/paint/paint_flags.h"
#include "chrome/browser/themes/theme_properties.h"
#include "components/url_formatter/elide_url.h"
#include "components/url_formatter/url_formatter.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/pathops/SkPathOps.h"
#include "ui/base/theme_provider.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/text_elider.h"
#include "ui/gfx/text_utils.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/animation/animation_delegate_views.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scrollbar/scroll_bar_views.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

#if defined(OS_CHROMEOS)
#include "ash/public/cpp/window_properties.h"
#include "ui/aura/window.h"
#endif

namespace {

// The alpha and color of the bubble's shadow.
constexpr SkColor kShadowColor = SkColorSetARGB(30, 0, 0, 0);

// The roundedness of the edges of our bubble.
constexpr int kBubbleCornerRadius = 4;

// How close the mouse can get to the infobubble before it starts sliding
// off-screen.
constexpr int kMousePadding = 20;

// The minimum horizontal space between the edges of the text and the edges of
// the status bubble, not including the outer shadow ring.
constexpr int kTextHorizPadding = 5;

// Delays before we start hiding or showing the bubble after we receive a
// show or hide request.
constexpr auto kShowDelay = base::TimeDelta::FromMilliseconds(80);
constexpr auto kHideDelay = base::TimeDelta::FromMilliseconds(250);

// How long each fade should last for.
constexpr auto kShowFadeDuration = base::TimeDelta::FromMilliseconds(120);
constexpr auto kHideFadeDuration = base::TimeDelta::FromMilliseconds(200);
constexpr int kFramerate = 25;

// How long each expansion step should take.
constexpr auto kMinExpansionStepDuration =
    base::TimeDelta::FromMilliseconds(20);
constexpr auto kMaxExpansionStepDuration =
    base::TimeDelta::FromMilliseconds(150);

// How long to delay before destroying an unused status bubble widget.
constexpr auto kDestroyPopupDelay = base::TimeDelta::FromSeconds(10);

const gfx::FontList& GetFont() {
  return views::style::GetFont(views::style::CONTEXT_LABEL,
                               views::style::STYLE_PRIMARY);
}

}  // namespace

// StatusBubbleViews::StatusViewAnimation --------------------------------------
class StatusBubbleViews::StatusViewAnimation
    : public gfx::LinearAnimation,
      public views::AnimationDelegateViews {
 public:
  StatusViewAnimation(StatusView* status_view,
                      float opacity_start,
                      float opacity_end);
  ~StatusViewAnimation() override;

  float GetCurrentOpacity();

 private:
  // gfx::LinearAnimation:
  void AnimateToState(double state) override;

  // gfx::AnimationDelegate:
  void AnimationEnded(const Animation* animation) override;

  StatusView* status_view_;

  // Start and end opacities for the current transition - note that as a
  // fade-in can easily turn into a fade out, opacity_start_ is sometimes
  // a value between 0 and 1.
  float opacity_start_;
  float opacity_end_;

  DISALLOW_COPY_AND_ASSIGN(StatusViewAnimation);
};

// StatusBubbleViews::StatusView -----------------------------------------------
//
// StatusView manages the display of the bubble, applying text changes and
// fading in or out the bubble as required.
class StatusBubbleViews::StatusView : public views::View {
 public:
  // The bubble can be in one of many states:
  enum BubbleState {
    BUBBLE_HIDDEN,         // Entirely BUBBLE_HIDDEN.
    BUBBLE_HIDING_FADE,    // In a fade-out transition.
    BUBBLE_HIDING_TIMER,   // Waiting before a fade-out.
    BUBBLE_SHOWING_TIMER,  // Waiting before a fade-in.
    BUBBLE_SHOWING_FADE,   // In a fade-in transition.
    BUBBLE_SHOWN           // Fully visible.
  };

  enum BubbleStyle {
    STYLE_BOTTOM,
    STYLE_FLOATING,
    STYLE_STANDARD,
    STYLE_STANDARD_RIGHT
  };

  explicit StatusView(StatusBubbleViews* status_bubble);
  ~StatusView() override;

  // views::View:
  gfx::Insets GetInsets() const override;

  // Set the bubble text, or hide the bubble if |text| is an empty string.
  // Triggers an animation sequence to display if |should_animate_open| is true.
  void SetText(const base::string16& text, bool should_animate_open);

  BubbleState state() const { return state_; }
  BubbleStyle style() const { return style_; }
  void SetStyle(BubbleStyle style);

  // Show the bubble instantly.
  void ShowInstantly();

  // Hide the bubble instantly; this may destroy the bubble and view.
  void HideInstantly();

  // Resets any timers we have. Typically called when the user moves a mouse.
  void ResetTimer();

  // This call backs the StatusView in order to fade the bubble in and out.
  void SetOpacity(float opacity);

  // Depending on the state of the bubble this will hide the popup or not.
  void OnAnimationEnded();

  gfx::Animation* animation() { return animation_.get(); }

  bool IsDestroyPopupTimerRunning() const;

 protected:
  // views::View:
  void OnThemeChanged() override;

 private:
  class InitialTimer;

  // Manage the timers that control the delay before a fade begins or ends.
  void StartTimer(base::TimeDelta time);
  void OnTimer();
  void CancelTimer();
  void RestartTimer(base::TimeDelta delay);

  // Manage the fades and starting and stopping the animations correctly.
  void StartFade(float start, float end, base::TimeDelta duration);
  void StartHiding();
  void StartShowing();

  // Set the text label's colors according to the theme.
  void SetTextLabelColors(views::Label* label);

  // views::View:
  const char* GetClassName() const override;
  void OnPaint(gfx::Canvas* canvas) override;

  BubbleState state_ = BUBBLE_HIDDEN;
  BubbleStyle style_ = STYLE_STANDARD;

  std::unique_ptr<StatusViewAnimation> animation_;

  // The status bubble that manages the popup widget and this view.
  StatusBubbleViews* status_bubble_;

  // The currently-displayed text.
  views::Label* text_;

  // A timer used to delay destruction of the popup widget. This is meant to
  // balance the performance tradeoffs of rapid creation/destruction and the
  // memory savings of closing the widget when it's hidden and unused.
  base::OneShotTimer destroy_popup_timer_;

  base::WeakPtrFactory<StatusBubbleViews::StatusView> timer_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(StatusView);
};

StatusBubbleViews::StatusView::StatusView(StatusBubbleViews* status_bubble)
    : status_bubble_(status_bubble) {
  animation_ = std::make_unique<StatusViewAnimation>(this, 0, 0);

  SetLayoutManager(std::make_unique<views::FillLayout>());

  std::unique_ptr<views::Label> text = std::make_unique<views::Label>();
  // Don't move this after AddChildView() since this function would trigger
  // repaint which should not happen in the constructor.
  SetTextLabelColors(text.get());
  text->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  text_ = AddChildView(std::move(text));
}

StatusBubbleViews::StatusView::~StatusView() {
  animation_->Stop();
  CancelTimer();
}

gfx::Insets StatusBubbleViews::StatusView::GetInsets() const {
  return gfx::Insets(kShadowThickness, kShadowThickness + kTextHorizPadding);
}

void StatusBubbleViews::StatusView::SetText(const base::string16& text,
                                            bool should_animate_open) {
  if (text.empty()) {
    StartHiding();
  } else {
    text_->SetText(text);
    if (should_animate_open)
      StartShowing();
  }
}

void StatusBubbleViews::StatusView::SetStyle(BubbleStyle style) {
  if (style_ != style) {
    style_ = style;
    SchedulePaint();
  }
}

void StatusBubbleViews::StatusView::ShowInstantly() {
  animation_->Stop();
  CancelTimer();
  SetOpacity(1.0);
  state_ = BUBBLE_SHOWN;
  GetWidget()->ShowInactive();
  destroy_popup_timer_.Stop();
}

void StatusBubbleViews::StatusView::HideInstantly() {
  animation_->Stop();
  CancelTimer();
  SetOpacity(0.0);
  text_->SetText(base::string16());
  state_ = BUBBLE_HIDDEN;
  // Don't orderOut: the window on macOS. Doing so for a child window requires
  // it to be detached/reattached, which may trigger a space switch. Instead,
  // just leave the window fully transparent and unclickable.
  GetWidget()->Hide();
  destroy_popup_timer_.Stop();
  // This isn't done in the constructor as tests may change the task runner
  // after the fact.
  destroy_popup_timer_.SetTaskRunner(status_bubble_->task_runner_);
  destroy_popup_timer_.Start(FROM_HERE, kDestroyPopupDelay, status_bubble_,
                             &StatusBubbleViews::DestroyPopup);
}

void StatusBubbleViews::StatusView::ResetTimer() {
  if (state_ == BUBBLE_SHOWING_TIMER) {
    // We hadn't yet begun showing anything when we received a new request
    // for something to show, so we start from scratch.
    RestartTimer(kShowDelay);
  }
}

void StatusBubbleViews::StatusView::SetOpacity(float opacity) {
  GetWidget()->SetOpacity(opacity);
}

void StatusBubbleViews::StatusView::OnAnimationEnded() {
  if (state_ == BUBBLE_SHOWING_FADE)
    state_ = BUBBLE_SHOWN;
  else if (state_ == BUBBLE_HIDING_FADE)
    HideInstantly();  // This view may be destroyed after calling HideInstantly.
}

bool StatusBubbleViews::StatusView::IsDestroyPopupTimerRunning() const {
  return destroy_popup_timer_.IsRunning();
}

void StatusBubbleViews::StatusView::OnThemeChanged() {
  views::View::OnThemeChanged();
  SetTextLabelColors(text_);
}

void StatusBubbleViews::StatusView::StartTimer(base::TimeDelta time) {
  if (timer_factory_.HasWeakPtrs())
    timer_factory_.InvalidateWeakPtrs();

  status_bubble_->task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&StatusBubbleViews::StatusView::OnTimer,
                     timer_factory_.GetWeakPtr()),
      time);
}

void StatusBubbleViews::StatusView::OnTimer() {
  if (state_ == BUBBLE_HIDING_TIMER) {
    state_ = BUBBLE_HIDING_FADE;
    StartFade(1.0f, 0.0f, kHideFadeDuration);
  } else if (state_ == BUBBLE_SHOWING_TIMER) {
    state_ = BUBBLE_SHOWING_FADE;
    StartFade(0.0f, 1.0f, kShowFadeDuration);
  }
}

void StatusBubbleViews::StatusView::CancelTimer() {
  if (timer_factory_.HasWeakPtrs())
    timer_factory_.InvalidateWeakPtrs();
}

void StatusBubbleViews::StatusView::RestartTimer(base::TimeDelta delay) {
  CancelTimer();
  StartTimer(delay);
}

void StatusBubbleViews::StatusView::StartFade(float start,
                                              float end,
                                              base::TimeDelta duration) {
  animation_ = std::make_unique<StatusViewAnimation>(this, start, end);

  // This will also reset the currently-occurring animation.
  animation_->SetDuration(duration);
  animation_->Start();
}

void StatusBubbleViews::StatusView::StartHiding() {
  if (state_ == BUBBLE_SHOWN) {
    state_ = BUBBLE_HIDING_TIMER;
    StartTimer(kHideDelay);
  } else if (state_ == BUBBLE_SHOWING_FADE) {
    state_ = BUBBLE_HIDING_FADE;
    // Figure out where we are in the current fade.
    float current_opacity = animation_->GetCurrentOpacity();

    // Start a fade in the opposite direction.
    StartFade(current_opacity, 0.0f, kHideFadeDuration * current_opacity);
  } else if (state_ == BUBBLE_SHOWING_TIMER) {
    HideInstantly();  // This view may be destroyed after calling HideInstantly.
  }
}

void StatusBubbleViews::StatusView::StartShowing() {
  destroy_popup_timer_.Stop();

  if (state_ == BUBBLE_HIDDEN) {
    GetWidget()->ShowInactive();
    state_ = BUBBLE_SHOWING_TIMER;
    StartTimer(kShowDelay);
  } else if (state_ == BUBBLE_HIDING_TIMER) {
    state_ = BUBBLE_SHOWN;
    CancelTimer();
  } else if (state_ == BUBBLE_HIDING_FADE) {
    // We're partway through a fade.
    state_ = BUBBLE_SHOWING_FADE;

    // Figure out where we are in the current fade.
    float current_opacity = animation_->GetCurrentOpacity();

    // Start a fade in the opposite direction.
    StartFade(current_opacity, 1.0f, kShowFadeDuration * current_opacity);
  } else if (state_ == BUBBLE_SHOWING_TIMER) {
    // We hadn't yet begun showing anything when we received a new request
    // for something to show, so we start from scratch.
    ResetTimer();
  }
}

void StatusBubbleViews::StatusView::SetTextLabelColors(views::Label* text) {
  const auto* theme_provider = status_bubble_->base_view()->GetThemeProvider();
  SkColor bubble_color =
      theme_provider->GetColor(ThemeProperties::COLOR_STATUS_BUBBLE);
  text->SetBackgroundColor(bubble_color);
  // Text color is the background tab text color, adjusted if required.
  text->SetEnabledColor(theme_provider->GetColor(
      ThemeProperties::COLOR_TAB_FOREGROUND_INACTIVE_FRAME_ACTIVE));
}

const char* StatusBubbleViews::StatusView::GetClassName() const {
  return "StatusBubbleViews::StatusView";
}

void StatusBubbleViews::StatusView::OnPaint(gfx::Canvas* canvas) {
  gfx::ScopedCanvas scoped(canvas);
  float scale = canvas->UndoDeviceScaleFactor();
  const float radius = kBubbleCornerRadius * scale;

  SkScalar rad[8] = {};

  // Top Edges - if the bubble is in its bottom position (sticking downwards),
  // then we square the top edges. Otherwise, we square the edges based on the
  // position of the bubble within the window (the bubble is positioned in the
  // southeast corner in RTL and in the southwest corner in LTR).
  if (style_ != STYLE_BOTTOM) {
    if (base::i18n::IsRTL() != (style_ == STYLE_STANDARD_RIGHT)) {
      // The text is RtL or the bubble is on the right side (but not both).

      // Top Left corner.
      rad[0] = radius;
      rad[1] = radius;
    } else {
      // Top Right corner.
      rad[2] = radius;
      rad[3] = radius;
    }
  }

  // Bottom edges - Keep these squared off if the bubble is in its standard
  // position (sticking upward).
  if (style_ != STYLE_STANDARD && style_ != STYLE_STANDARD_RIGHT) {
    // Bottom Right Corner.
    rad[4] = radius;
    rad[5] = radius;

    // Bottom Left Corner.
    rad[6] = radius;
    rad[7] = radius;
  }

  // Snap to pixels to avoid shadow blurriness.
  gfx::Size scaled_size = gfx::ScaleToRoundedSize(size(), scale);

  // This needs to be pixel-aligned too. Floor is perferred here because a more
  // conservative value prevents the bottom edge from occasionally leaving a gap
  // where the web content is visible.
  const int shadow_thickness_pixels = std::floor(kShadowThickness * scale);

  // The shadow will overlap the window frame. Clip it off when the bubble is
  // docked. Otherwise when the bubble is floating preserve the full shadow so
  // the bubble looks complete.
  int clip_left = style_ == STYLE_STANDARD ? shadow_thickness_pixels : 0;
  int clip_right = style_ == STYLE_STANDARD_RIGHT ? shadow_thickness_pixels : 0;
  if (base::i18n::IsRTL())
    std::swap(clip_left, clip_right);

  const int clip_bottom = clip_left || clip_right ? shadow_thickness_pixels : 0;
  gfx::Rect clip_rect(scaled_size);
  clip_rect.Inset(clip_left, 0, clip_right, clip_bottom);
  canvas->ClipRect(clip_rect);

  gfx::RectF bubble_rect{gfx::SizeF(scaled_size)};
  // Reposition() moves the bubble down and to the left in order to overlap the
  // client edge (or window frame when there's no client edge) by 1 DIP. We want
  // a 1 pixel shadow on the innermost pixel of that overlap. So we inset the
  // bubble bounds by 1 DIP minus 1 pixel. Failing to do this results in drawing
  // further and further outside the window as the scale increases.
  const int inset = shadow_thickness_pixels - 1;
  bubble_rect.Inset(style_ == STYLE_STANDARD_RIGHT ? 0 : inset, 0,
                    style_ == STYLE_STANDARD_RIGHT ? inset : 0, inset);
  // Align to pixel centers now that the layout is correct.
  bubble_rect.Inset(0.5, 0.5);

  SkPath path;
  path.addRoundRect(gfx::RectFToSkRect(bubble_rect), rad);

  cc::PaintFlags flags;
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setStrokeWidth(1);
  flags.setAntiAlias(true);

  SkPath stroke_path;
  flags.getFillPath(path, &stroke_path);

  // Get the fill path by subtracting the shadow so they align neatly.
  SkPath fill_path;
  Op(path, stroke_path, kDifference_SkPathOp, &fill_path);
  flags.setStyle(cc::PaintFlags::kFill_Style);

  const auto* theme_provider = status_bubble_->base_view()->GetThemeProvider();
  const SkColor bubble_color =
      theme_provider->GetColor(ThemeProperties::COLOR_STATUS_BUBBLE);
  flags.setColor(bubble_color);
  canvas->sk_canvas()->drawPath(fill_path, flags);

  flags.setColor(kShadowColor);
  canvas->sk_canvas()->drawPath(stroke_path, flags);
}


// StatusBubbleViews::StatusViewAnimation --------------------------------------

StatusBubbleViews::StatusViewAnimation::StatusViewAnimation(
    StatusView* status_view,
    float opacity_start,
    float opacity_end)
    : gfx::LinearAnimation(this, kFramerate),
      views::AnimationDelegateViews(status_view),
      status_view_(status_view),
      opacity_start_(opacity_start),
      opacity_end_(opacity_end) {}

StatusBubbleViews::StatusViewAnimation::~StatusViewAnimation() {
  // Remove ourself as a delegate so that we don't get notified when
  // animations end as a result of destruction.
  set_delegate(NULL);
}

float StatusBubbleViews::StatusViewAnimation::GetCurrentOpacity() {
  return static_cast<float>(opacity_start_ +
                            (opacity_end_ - opacity_start_) *
                                gfx::LinearAnimation::GetCurrentValue());
}

void StatusBubbleViews::StatusViewAnimation::AnimateToState(double state) {
  status_view_->SetOpacity(GetCurrentOpacity());
}

void StatusBubbleViews::StatusViewAnimation::AnimationEnded(
    const gfx::Animation* animation) {
  status_view_->SetOpacity(opacity_end_);
  status_view_->OnAnimationEnded();
}

// StatusBubbleViews::StatusViewExpander ---------------------------------------
//
// Manages the expansion and contraction of the status bubble as it accommodates
// URLs too long to fit in the standard bubble. Changes are passed through the
// StatusView to paint.
class StatusBubbleViews::StatusViewExpander
    : public gfx::LinearAnimation,
      public views::AnimationDelegateViews {
 public:
  StatusViewExpander(StatusBubbleViews* status_bubble, StatusView* status_view)
      : gfx::LinearAnimation(this, kFramerate),
        views::AnimationDelegateViews(status_view),
        status_bubble_(status_bubble),
        status_view_(status_view) {}

  // Manage the expansion of the bubble.
  void StartExpansion(const base::string16& expanded_text,
                      int current_width,
                      int expansion_end);

 private:
  // Animation functions.
  int GetCurrentBubbleWidth();
  void SetBubbleWidth(int width);
  void AnimateToState(double state) override;
  void AnimationEnded(const gfx::Animation* animation) override;

  // The status bubble that manages the popup widget and this object.
  StatusBubbleViews* status_bubble_;

  // Change the bounds and text of this view.
  StatusView* status_view_;

  // Text elided (if needed) to fit maximum status bar width.
  base::string16 expanded_text_;

  // Widths at expansion start and end.
  int expansion_start_ = 0;
  int expansion_end_ = 0;

  DISALLOW_COPY_AND_ASSIGN(StatusViewExpander);
};

void StatusBubbleViews::StatusViewExpander::AnimateToState(double state) {
  SetBubbleWidth(GetCurrentBubbleWidth());
}

void StatusBubbleViews::StatusViewExpander::AnimationEnded(
    const gfx::Animation* animation) {
  status_view_->SetText(expanded_text_, false);
  SetBubbleWidth(expansion_end_);
  // WARNING: crash data seems to indicate |this| may be deleted by the time
  // SetBubbleWidth() returns.
}

void StatusBubbleViews::StatusViewExpander::StartExpansion(
    const base::string16& expanded_text,
    int expansion_start,
    int expansion_end) {
  expanded_text_ = expanded_text;
  expansion_start_ = expansion_start;
  expansion_end_ = expansion_end;
  base::TimeDelta min_duration = std::max(
      kMinExpansionStepDuration,
      kMaxExpansionStepDuration * (expansion_end - expansion_start) / 100.0);
  SetDuration(std::min(kMaxExpansionStepDuration, min_duration));
  Start();
}

int StatusBubbleViews::StatusViewExpander::GetCurrentBubbleWidth() {
  return static_cast<int>(expansion_start_ +
      (expansion_end_ - expansion_start_) *
          gfx::LinearAnimation::GetCurrentValue());
}

void StatusBubbleViews::StatusViewExpander::SetBubbleWidth(int width) {
  status_view_->SchedulePaint();
  status_bubble_->SetBubbleWidth(width);
  // WARNING: crash data seems to indicate |this| may be deleted by the time
  // SetBubbleWidth() returns.
}


// StatusBubbleViews -----------------------------------------------------------

const int StatusBubbleViews::kShadowThickness = 1;

StatusBubbleViews::StatusBubbleViews(views::View* base_view)
    : base_view_(base_view),
      task_runner_(base::ThreadTaskRunnerHandle::Get().get()) {}

StatusBubbleViews::~StatusBubbleViews() {
  DestroyPopup();
}

void StatusBubbleViews::InitPopup() {
  if (!popup_) {
    DCHECK(!view_);
    DCHECK(!expand_view_);
    popup_ = std::make_unique<views::Widget>();

    views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
#if defined(OS_WIN)
    // On Windows use the software compositor to ensure that we don't block
    // the UI thread blocking issue during command buffer creation. We can
    // revert this change once http://crbug.com/125248 is fixed.
    params.force_software_compositing = true;
#endif
    params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
    params.accept_events = false;
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    views::Widget* frame = base_view_->GetWidget();
    params.parent = frame->GetNativeView();
    params.context = frame->GetNativeWindow();
    params.name = "StatusBubble";
#if defined(OS_CHROMEOS)
    params.init_properties_container.SetProperty(ash::kHideInOverviewKey, true);
    params.init_properties_container.SetProperty(ash::kHideInDeskMiniViewKey,
                                                 true);
#endif  // defined(OS_CHROMEOS)
    popup_->Init(std::move(params));
    // We do our own animation and don't want any from the system.
    popup_->SetVisibilityChangedAnimationsEnabled(false);
    popup_->SetOpacity(0.f);
    view_ = popup_->SetContentsView(std::make_unique<StatusView>(this));
    expand_view_ = std::make_unique<StatusViewExpander>(this, view_);
#if !defined(OS_MAC)
    // Stack the popup above the base widget and below higher z-order windows.
    // This is unnecessary and even detrimental on Mac, see CreateBubbleWidget.
    popup_->StackAboveWidget(frame);
#endif
    RepositionPopup();
  }
}

void StatusBubbleViews::DestroyPopup() {
  CancelExpandTimer();
  expand_view_.reset();
  view_ = nullptr;
  // Move |popup_| to the stack to avoid reentrancy issues with CloseNow().
  if (std::unique_ptr<views::Widget> popup = std::move(popup_))
    popup->CloseNow();
}

void StatusBubbleViews::Reposition() {
  // Overlap the client edge that's shown in restored mode, or when there is no
  // client edge this makes the bubble snug with the corner of the window.
  int overlap = kShadowThickness;
  int height = GetPreferredHeight();
  int base_view_height = base_view_->bounds().height();
  gfx::Point origin(-overlap, base_view_height - height + overlap);
  SetBounds(origin.x(), origin.y(), base_view_->bounds().width() / 3, height);
}

void StatusBubbleViews::RepositionPopup() {
  if (popup_.get()) {
    gfx::Point top_left;
    // TODO(flackr): Get the non-transformed point so that the status bubble
    // popup window's position is consistent with the base_view_'s window.
    views::View::ConvertPointToScreen(base_view_, &top_left);
    popup_->SetBounds(gfx::Rect(top_left.x() + position_.x(),
                                top_left.y() + position_.y(),
                                size_.width(), size_.height()));
  }
}

int StatusBubbleViews::GetPreferredHeight() {
  return GetFont().GetHeight() + kTotalVerticalPadding;
}

void StatusBubbleViews::SetBounds(int x, int y, int w, int h) {
  original_position_.SetPoint(x, y);
  position_.SetPoint(base_view_->GetMirroredXWithWidthInView(x, w), y);
  size_.SetSize(w, h);
  RepositionPopup();
  if (popup_.get() && contains_mouse_)
    AvoidMouse(last_mouse_moved_location_);
}

int StatusBubbleViews::GetWidthForURL(const base::string16& url_string) {
  // Get the width of the elided url
  int elided_url_width = gfx::GetStringWidth(url_string, GetFont());
  // Add proper paddings
  return elided_url_width + (kShadowThickness + kTextHorizPadding) * 2 + 1;
}

void StatusBubbleViews::OnThemeChanged() {
  if (popup_)
    popup_->ThemeChanged();
}

void StatusBubbleViews::SetStatus(const base::string16& status_text) {
  if (size_.IsEmpty())
    return;  // We have no bounds, don't attempt to show the popup.

  if (status_text_ == status_text && !status_text.empty())
    return;

  if (!IsFrameVisible())
    return;  // Don't show anything if the parent isn't visible.

  status_text_ = status_text;
  if (status_text_.empty() && url_text_.empty() && !popup_)
    return;

  InitPopup();
  view_->SetText(!status_text_.empty() ? status_text_ : url_text_, true);
  if (!status_text_.empty()) {
    SetBubbleWidth(GetStandardStatusBubbleWidth());
    view_->ShowInstantly();
  }
}

void StatusBubbleViews::SetURL(const GURL& url) {
  url_ = url;
  if (size_.IsEmpty())
    return;  // We have no bounds, don't attempt to show the popup.

  if (url.is_empty() && status_text_.empty() && !popup_)
    return;

  InitPopup();

  // If we want to clear a displayed URL but there is a status still to
  // display, display that status instead.
  if (url.is_empty() && !status_text_.empty()) {
    url_text_ = base::string16();
    if (IsFrameVisible())
      view_->SetText(status_text_, true);
    return;
  }

  // Set Elided Text corresponding to the GURL object.
  int text_width = static_cast<int>(
      size_.width() - (kShadowThickness + kTextHorizPadding) * 2 - 1);
  url_text_ = url_formatter::ElideUrl(url, GetFont(), text_width);

  // Get the width of the URL if the bubble width is the maximum size.
  base::string16 full_size_elided_url =
      url_formatter::ElideUrl(url, GetFont(), GetMaxStatusBubbleWidth());
  int url_width = GetWidthForURL(full_size_elided_url);

  // Get the width for the url if it is unexpanded.
  int unexpanded_width = std::min(url_width, GetStandardStatusBubbleWidth());

  // Reset expansion state only when bubble is completely hidden.
  if (view_->state() == StatusView::BUBBLE_HIDDEN) {
    is_expanded_ = false;
    url_text_ = url_formatter::ElideUrl(url, GetFont(), unexpanded_width);
    SetBubbleWidth(unexpanded_width);
  }

  if (IsFrameVisible()) {
    // If bubble is not expanded & not empty, make it fit properly in the
    // unexpanded bubble
    if (!is_expanded_ & !url.is_empty()) {
      url_text_ = url_formatter::ElideUrl(url, GetFont(), unexpanded_width);
      SetBubbleWidth(unexpanded_width);
    }

    CancelExpandTimer();

    // If bubble is already in expanded state, shift to adjust to new text
    // size (shrinking or expanding). Otherwise delay.
    if (is_expanded_ && !url.is_empty()) {
      ExpandBubble();
    } else if (url_formatter::FormatUrl(url).length() >
               url_text_.length()) {
      task_runner_->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&StatusBubbleViews::ExpandBubble,
                         expand_timer_factory_.GetWeakPtr()),
          base::TimeDelta::FromMilliseconds(kExpandHoverDelayMS));
    }
    // An URL is always treated as a left-to-right string. On right-to-left UIs
    // we need to explicitly mark the URL as LTR to make sure it is displayed
    // correctly.
    view_->SetText(base::i18n::GetDisplayStringInLTRDirectionality(url_text_),
                   true);
  }
}

void StatusBubbleViews::Hide() {
  status_text_ = base::string16();
  url_text_ = base::string16();
  if (view_)
    view_->HideInstantly();
}

void StatusBubbleViews::MouseMoved(bool left_content) {
  MouseMovedAt(display::Screen::GetScreen()->GetCursorScreenPoint(),
               left_content);
}

void StatusBubbleViews::MouseMovedAt(const gfx::Point& location,
                                     bool left_content) {
  contains_mouse_ = !left_content;
  if (left_content) {
    RepositionPopup();
    return;
  }
  last_mouse_moved_location_ = location;

  if (view_) {
    view_->ResetTimer();

    if (view_->state() != StatusView::BUBBLE_HIDDEN &&
        view_->state() != StatusView::BUBBLE_HIDING_FADE &&
        view_->state() != StatusView::BUBBLE_HIDING_TIMER) {
      AvoidMouse(location);
    }
  }
}

void StatusBubbleViews::UpdateDownloadShelfVisibility(bool visible) {
  download_shelf_is_visible_ = visible;
}

void StatusBubbleViews::AvoidMouse(const gfx::Point& location) {
  DCHECK(view_);
  // Get the position of the frame.
  gfx::Point top_left;
  views::View::ConvertPointToScreen(base_view_, &top_left);
  // Border included.
  int window_width = base_view_->GetLocalBounds().width();

  // Get the cursor position relative to the popup.
  gfx::Point relative_location = location;
  if (base::i18n::IsRTL()) {
    int top_right_x = top_left.x() + window_width;
    relative_location.set_x(top_right_x - relative_location.x());
  } else {
    relative_location.set_x(
        relative_location.x() - (top_left.x() + position_.x()));
  }
  relative_location.set_y(
      relative_location.y() - (top_left.y() + position_.y()));

  // If the mouse is in a position where we think it would move the
  // status bubble, figure out where and how the bubble should be moved.
  if (relative_location.y() > -kMousePadding &&
      relative_location.x() < size_.width() + kMousePadding) {
    int offset = kMousePadding + relative_location.y();

    // Make the movement non-linear.
    offset = offset * offset / kMousePadding;

    // When the mouse is entering from the right, we want the offset to be
    // scaled by how horizontally far away the cursor is from the bubble.
    if (relative_location.x() > size_.width()) {
      offset = static_cast<int>(static_cast<float>(offset) * (
          static_cast<float>(kMousePadding -
              (relative_location.x() - size_.width())) /
          static_cast<float>(kMousePadding)));
    }

    // Cap the offset and change the visual presentation of the bubble
    // depending on where it ends up (so that rounded corners square off
    // and mate to the edges of the tab content).
    if (offset >= size_.height() - kShadowThickness * 2) {
      offset = size_.height() - kShadowThickness * 2;
      view_->SetStyle(StatusView::STYLE_BOTTOM);
    } else if (offset > kBubbleCornerRadius / 2 - kShadowThickness) {
      view_->SetStyle(StatusView::STYLE_FLOATING);
    } else {
      view_->SetStyle(StatusView::STYLE_STANDARD);
    }

    // Check if the bubble sticks out from the monitor or will obscure
    // download shelf.
    gfx::NativeView view = base_view_->GetWidget()->GetNativeView();
    gfx::Rect monitor_rect =
        display::Screen::GetScreen()->GetDisplayNearestView(view).work_area();
    const int bubble_bottom_y = top_left.y() + position_.y() + size_.height();

    if (bubble_bottom_y + offset > monitor_rect.height() ||
        (download_shelf_is_visible_ &&
         (view_->style() == StatusView::STYLE_FLOATING ||
          view_->style() == StatusView::STYLE_BOTTOM))) {
      // The offset is still too large. Move the bubble to the right and reset
      // Y offset_ to zero.
      view_->SetStyle(StatusView::STYLE_STANDARD_RIGHT);
      offset_ = 0;

      // Subtract border width + bubble width.
      int right_position_x = window_width - (position_.x() + size_.width());
      popup_->SetBounds(gfx::Rect(top_left.x() + right_position_x,
                                  top_left.y() + position_.y(),
                                  size_.width(), size_.height()));
    } else {
      offset_ = offset;
      popup_->SetBounds(gfx::Rect(top_left.x() + position_.x(),
                                  top_left.y() + position_.y() + offset_,
                                  size_.width(), size_.height()));
    }
  } else if (offset_ != 0 ||
      view_->style() == StatusView::STYLE_STANDARD_RIGHT) {
    offset_ = 0;
    view_->SetStyle(StatusView::STYLE_STANDARD);
    popup_->SetBounds(gfx::Rect(top_left.x() + position_.x(),
                                top_left.y() + position_.y(),
                                size_.width(), size_.height()));
  }
}

bool StatusBubbleViews::IsFrameVisible() {
  views::Widget* frame = base_view_->GetWidget();
  if (!frame->IsVisible())
    return false;

  views::Widget* window = frame->GetTopLevelWidget();
  return !window || !window->IsMinimized();
}

bool StatusBubbleViews::IsFrameMaximized() {
  views::Widget* frame = base_view_->GetWidget();
  views::Widget* window = frame->GetTopLevelWidget();
  return window && window->IsMaximized();
}

void StatusBubbleViews::ExpandBubble() {
  // Elide URL to maximum possible size, then check actual length (it may
  // still be too long to fit) before expanding bubble.
  url_text_ =
      url_formatter::ElideUrl(url_, GetFont(), GetMaxStatusBubbleWidth());
  int expanded_bubble_width =
      std::min(GetWidthForURL(url_text_), GetMaxStatusBubbleWidth());
  is_expanded_ = true;
  expand_view_->StartExpansion(url_text_, size_.width(), expanded_bubble_width);
}

int StatusBubbleViews::GetStandardStatusBubbleWidth() {
  return base_view_->bounds().width() / 3;
}

int StatusBubbleViews::GetMaxStatusBubbleWidth() {
  const ui::NativeTheme* theme = base_view_->GetNativeTheme();
  return static_cast<int>(
      std::max(0, base_view_->bounds().width() -
                      (kShadowThickness + kTextHorizPadding) * 2 - 1 -
                      views::ScrollBarViews::GetVerticalScrollBarWidth(theme)));
}

void StatusBubbleViews::SetBubbleWidth(int width) {
  size_.set_width(width);
  SetBounds(original_position_.x(), original_position_.y(),
            size_.width(), size_.height());
}

void StatusBubbleViews::CancelExpandTimer() {
  if (expand_timer_factory_.HasWeakPtrs())
    expand_timer_factory_.InvalidateWeakPtrs();
}

gfx::Animation* StatusBubbleViews::GetShowHideAnimationForTest() {
  return view_ ? view_->animation() : nullptr;
}

bool StatusBubbleViews::IsDestroyPopupTimerRunningForTest() {
  return view_ && view_->IsDestroyPopupTimerRunning();
}
