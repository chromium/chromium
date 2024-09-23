// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/views/status_bubble_views.h"

#include <algorithm>
#include <utility>

#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/timer/timer.h"
#include "base/types/cxx23_to_underlying.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "cc/paint/paint_flags.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/chrome_widget_sublevel.h"
#include "components/url_formatter/elide_url.h"
#include "components/url_formatter/url_formatter.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/pathops/SkPathOps.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rrect_f.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/text_elider.h"
#include "ui/gfx/text_utils.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/animation/animation_delegate_views.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scrollbar/scroll_bar_views.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/public/cpp/window_properties.h"
#include "ui/aura/window.h"
#endif

namespace {

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
constexpr auto kShowDelay = base::Milliseconds(80);
constexpr auto kHideDelay = base::Milliseconds(250);

// How long each fade should last for.
constexpr auto kShowFadeDuration = base::Milliseconds(120);
constexpr auto kHideFadeDuration = base::Milliseconds(200);
constexpr int kFramerate = 25;

// How long each expansion step should take.
constexpr auto kMinExpansionStepDuration = base::Milliseconds(20);
constexpr auto kMaxExpansionStepDuration = base::Milliseconds(150);

// How long to delay before destroying an unused status bubble widget.
constexpr auto kDestroyPopupDelay = base::Seconds(10);

const gfx::FontList& GetFont() {
  return views::TypographyProvider::Get().GetFont(views::style::CONTEXT_LABEL,
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
  StatusViewAnimation(const StatusViewAnimation&) = delete;
  StatusViewAnimation& operator=(const StatusViewAnimation&) = delete;
  ~StatusViewAnimation() override;

  float GetCurrentOpacity();

 private:
  // gfx::LinearAnimation:
  void AnimateToState(double state) override;

  // gfx::AnimationDelegate:
  void AnimationEnded(const Animation* animation) override;

  raw_ptr<StatusView> status_view_;

  // Start and end opacities for the current transition - note that as a
  // fade-in can easily turn into a fade out, opacity_start_ is sometimes
  // a value between 0 and 1.
  float opacity_start_;
  float opacity_end_;
};

// StatusBubbleViews::StatusView -----------------------------------------------
//
// StatusView manages the display of the bubble, applying text changes and
// fading in or out the bubble as required.
class StatusBubbleViews::StatusView : public views::View {
  METADATA_HEADER(StatusView, views::View)

 public:
  // The bubble can be in one of many states:
  enum class BubbleState {
    kHidden,
    kPreFadeIn,
    kFadingIn,
    kShown,
    kPreFadeOut,
    kFadingOut,
  };

  enum class BubbleStyle {
    kBottom,
    kFloating,
    kStandard,
    kStandardRight,
  };

  explicit StatusView(StatusBubbleViews* status_bubble);
  StatusView(const StatusView&) = delete;
  StatusView& operator=(const StatusView&) = delete;
  ~StatusView() override;

  // views::View:
  gfx::Insets GetInsets() const override;

  const std::u16string& GetText() const;
  void SetText(const std::u16string& text);

  BubbleState GetState() const { return state_; }

  BubbleStyle GetStyle() const { return style_; }
  void SetStyle(BubbleStyle style);

  // If |text| is empty, hides the bubble; otherwise, sets the bubble text to
  // |text| and shows the bubble.
  void AnimateForText(const std::u16string& text);

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
  void OnPaint(gfx::Canvas* canvas) override;

  BubbleState state_ = BubbleState::kHidden;
  BubbleStyle style_ = BubbleStyle::kStandard;

  std::unique_ptr<StatusViewAnimation> animation_;

  // The status bubble that manages the popup widget and this view.
  raw_ptr<StatusBubbleViews> status_bubble_;

  // The currently-displayed text.
  raw_ptr<views::Label> text_;

  // A timer used to delay destruction of the popup widget. This is meant to
  // balance the performance tradeoffs of rapid creation/destruction and the
  // memory savings of closing the widget when it's hidden and unused.
  base::OneShotTimer destroy_popup_timer_;

  base::CallbackListSubscription paint_as_active_subscription_;

  base::WeakPtrFactory<StatusBubbleViews::StatusView> timer_factory_{this};
};
using StatusView = StatusBubbleViews::StatusView;

StatusView::StatusView(StatusBubbleViews* status_bubble)
    : status_bubble_(status_bubble) {
  animation_ = std::make_unique<StatusViewAnimation>(this, 0, 0);

  SetUseDefaultFillLayout(true);

  std::unique_ptr<views::Label> text = std::make_unique<views::Label>();
  // Don't move this after AddChildView() since this function would trigger
  // repaint which should not happen in the constructor.
  SetTextLabelColors(text.get());
  text->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  text_ = AddChildView(std::move(text));

  paint_as_active_subscription_ =
      status_bubble_->base_view()
          ->GetWidget()
          ->RegisterPaintAsActiveChangedCallback(base::BindRepeating(
              &StatusView::SetTextLabelColors, base::Unretained(this), text_));
}

StatusView::~StatusView() {
  animation_->Stop();
  CancelTimer();
}

gfx::Insets StatusView::GetInsets() const {
  return gfx::Insets::VH(kShadowThickness,
                         kShadowThickness + kTextHorizPadding);
}

const std::u16string& StatusView::GetText() const {
  return text_->GetText();
}

void StatusView::SetText(const std::u16string& text) {
  if (text == GetText())
    return;

  text_->SetText(text);
  OnPropertyChanged(&text_, views::kPropertyEffectsNone);
}

void StatusView::AnimateForText(const std::u16string& text) {
  if (text.empty()) {
    StartHiding();
  } else {
    SetText(text);
    StartShowing();
  }
}

void StatusView::SetStyle(BubbleStyle style) {
  if (style_ == style)
    return;

  style_ = style;
  OnPropertyChanged(&style_, views::kPropertyEffectsPaint);
}

void StatusView::ShowInstantly() {
  animation_->Stop();
  CancelTimer();
  SetOpacity(1.0);
  state_ = BubbleState::kShown;
  GetWidget()->ShowInactive();
  destroy_popup_timer_.Stop();
}

void StatusView::HideInstantly() {
  animation_->Stop();
  CancelTimer();
  SetOpacity(0.0);
  SetText(std::u16string());
  state_ = BubbleState::kHidden;
  // Don't orderOut: the window on macOS. Doing so for a child window requires
  // it to be detached/reattached, which may trigger a space switch. Instead,
  // just leave the window fully transparent and unclickable.
  GetWidget()->Hide();
  destroy_popup_timer_.Stop();
  // This isn't done in the constructor as tests may change the task runner
  // after the fact.
  destroy_popup_timer_.SetTaskRunner(status_bubble_->task_runner_.get());
  destroy_popup_timer_.Start(FROM_HERE, kDestroyPopupDelay,
                             status_bubble_.get(),
                             &StatusBubbleViews::DestroyPopup);
}

void StatusView::ResetTimer() {
  if (state_ == BubbleState::kPreFadeIn) {
    // We hadn't yet begun showing anything when we received a new request
    // for something to show, so we start from scratch.
    RestartTimer(kShowDelay);
  }
}

void StatusView::SetOpacity(float opacity) {
  GetWidget()->SetOpacity(opacity);
}

void StatusView::OnAnimationEnded() {
  if (state_ == BubbleState::kFadingIn)
    state_ = BubbleState::kShown;
  else if (state_ == BubbleState::kFadingOut)
    HideInstantly();  // This view may be destroyed after calling HideInstantly.
}

bool StatusView::IsDestroyPopupTimerRunning() const {
  return destroy_popup_timer_.IsRunning();
}

void StatusView::OnThemeChanged() {
  views::View::OnThemeChanged();
  SetTextLabelColors(text_);
}

void StatusView::StartTimer(base::TimeDelta time) {
  if (timer_factory_.HasWeakPtrs())
    timer_factory_.InvalidateWeakPtrs();

  status_bubble_->task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&StatusView::OnTimer, timer_factory_.GetWeakPtr()), time);
}

void StatusView::OnTimer() {
  if (state_ == BubbleState::kPreFadeOut) {
    state_ = BubbleState::kFadingOut;
    StartFade(1.0f, 0.0f, kHideFadeDuration);
  } else if (state_ == BubbleState::kPreFadeIn) {
    state_ = BubbleState::kFadingIn;
    StartFade(0.0f, 1.0f, kShowFadeDuration);
  }
}

void StatusView::CancelTimer() {
  if (timer_factory_.HasWeakPtrs())
    timer_factory_.InvalidateWeakPtrs();
}

void StatusView::RestartTimer(base::TimeDelta delay) {
  CancelTimer();
  StartTimer(delay);
}

void StatusView::StartFade(float start, float end, base::TimeDelta duration) {
  animation_ = std::make_unique<StatusViewAnimation>(this, start, end);

  // This will also reset the currently-occurring animation.
  animation_->SetDuration(duration);
  animation_->Start();
}

void StatusView::StartHiding() {
  if (state_ == BubbleState::kShown) {
    state_ = BubbleState::kPreFadeOut;
    StartTimer(kHideDelay);
  } else if (state_ == BubbleState::kFadingIn) {
    state_ = BubbleState::kFadingOut;
    // Figure out where we are in the current fade.
    float current_opacity = animation_->GetCurrentOpacity();

    // Start a fade in the opposite direction.
    StartFade(current_opacity, 0.0f, kHideFadeDuration * current_opacity);
  } else if (state_ == BubbleState::kPreFadeIn) {
    HideInstantly();  // This view may be destroyed after calling HideInstantly.
  }
}

void StatusView::StartShowing() {
  destroy_popup_timer_.Stop();

  if (state_ == BubbleState::kHidden) {
    GetWidget()->ShowInactive();
    state_ = BubbleState::kPreFadeIn;
    StartTimer(kShowDelay);
  } else if (state_ == BubbleState::kPreFadeOut) {
    state_ = BubbleState::kShown;
    CancelTimer();
  } else if (state_ == BubbleState::kFadingOut) {
    // We're partway through a fade.
    state_ = BubbleState::kFadingIn;

    // Figure out where we are in the current fade.
    float current_opacity = animation_->GetCurrentOpacity();

    // Start a fade in the opposite direction.
    StartFade(current_opacity, 1.0f, kShowFadeDuration * current_opacity);
  } else if (state_ == BubbleState::kPreFadeIn) {
    // We hadn't yet begun showing anything when we received a new request
    // for something to show, so we start from scratch.
    ResetTimer();
  }
}

void StatusView::SetTextLabelColors(views::Label* text) {
  const auto* color_provider = status_bubble_->base_view()->GetColorProvider();
  const bool active =
      status_bubble_->base_view()->GetWidget()->ShouldPaintAsActive();
  SkColor bubble_color = color_provider->GetColor(
      active ? kColorStatusBubbleBackgroundFrameActive
             : kColorStatusBubbleBackgroundFrameInactive);
  text->SetBackgroundColor(bubble_color);
  // Text color is the background tab text color, adjusted if required.
  text->SetEnabledColor(color_provider->GetColor(
      active ? kColorStatusBubbleForegroundFrameActive
             : kColorStatusBubbleForegroundFrameInactive));
}

void StatusView::OnPaint(gfx::Canvas* canvas) {
  gfx::ScopedCanvas scoped(canvas);
  float scale = canvas->UndoDeviceScaleFactor();
  const float radius = kBubbleCornerRadius * scale;

  SkScalar rad[8] = {};
  auto round_corner = [&rad, radius](gfx::RRectF::Corner c) {
    int index = base::to_underlying(c);
    rad[2 * index] = radius;
    rad[2 * index + 1] = radius;
  };

  // Top Edges - if the bubble is in its bottom position (sticking downwards),
  // then we square the top edges. Otherwise, we square the edges based on the
  // position of the bubble within the window (the bubble is positioned in the
  // southeast corner in RTL and in the southwest corner in LTR).
  if (style_ != BubbleStyle::kBottom) {
    if (base::i18n::IsRTL() != (style_ == BubbleStyle::kStandardRight)) {
      // The text is RtL or the bubble is on the right side (but not both).
      round_corner(gfx::RRectF::Corner::kUpperLeft);
    } else {
      round_corner(gfx::RRectF::Corner::kUpperRight);
    }
  }

  // Bottom edges - Keep these squared off if the bubble is in its standard
  // position (sticking upward).
  if (style_ != BubbleStyle::kStandard &&
      style_ != BubbleStyle::kStandardRight) {
    round_corner(gfx::RRectF::Corner::kLowerRight);
    round_corner(gfx::RRectF::Corner::kLowerLeft);
  } else {
#if BUILDFLAG(IS_MAC)
    // Mac's window has rounded corners, but the corner radius might be
    // different on different versions. Status bubble will use its own round
    // corner on Mac when there is no download shelf beneath.
    if (!status_bubble_->download_shelf_is_visible_) {
      if (base::i18n::IsRTL() != (style_ == BubbleStyle::kStandard))
        round_corner(gfx::RRectF::Corner::kLowerLeft);
      else
        round_corner(gfx::RRectF::Corner::kLowerRight);
    }
#endif
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
  int clip_left =
      style_ == BubbleStyle::kStandard ? shadow_thickness_pixels : 0;
  int clip_right =
      style_ == BubbleStyle::kStandardRight ? shadow_thickness_pixels : 0;
  if (base::i18n::IsRTL())
    std::swap(clip_left, clip_right);

  const int clip_bottom = clip_left || clip_right ? shadow_thickness_pixels : 0;
  gfx::Rect clip_rect(scaled_size);
  clip_rect.Inset(gfx::Insets::TLBR(0, clip_left, clip_bottom, clip_right));
  canvas->ClipRect(clip_rect);

  gfx::RectF bubble_rect{gfx::SizeF(scaled_size)};
  // Reposition() moves the bubble down and to the left in order to overlap the
  // client edge (or window frame when there's no client edge) by 1 DIP. We want
  // a 1 pixel shadow on the innermost pixel of that overlap. So we inset the
  // bubble bounds by 1 DIP minus 1 pixel. Failing to do this results in drawing
  // further and further outside the window as the scale increases.
  const int inset = shadow_thickness_pixels - 1;
  bubble_rect.Inset(
      gfx::InsetsF()
          .set_left(style_ == BubbleStyle::kStandardRight ? 0 : inset)
          .set_right(style_ == BubbleStyle::kStandardRight ? inset : 0)
          .set_bottom(inset));
  // Align to pixel centers now that the layout is correct.
  bubble_rect.Inset(0.5);

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

  const auto* color_provider = status_bubble_->base_view()->GetColorProvider();
  const auto id =
      status_bubble_->base_view()->GetWidget()->ShouldPaintAsActive()
          ? kColorStatusBubbleBackgroundFrameActive
          : kColorStatusBubbleBackgroundFrameInactive;
  flags.setColor(color_provider->GetColor(id));
  canvas->sk_canvas()->drawPath(fill_path, flags);

  flags.setColor(color_provider->GetColor(kColorStatusBubbleShadow));
  canvas->sk_canvas()->drawPath(stroke_path, flags);
}

DEFINE_ENUM_CONVERTERS(StatusView::BubbleState,
                       {StatusView::BubbleState::kHidden, u"kHidden"},
                       {StatusView::BubbleState::kPreFadeIn, u"kPreFadeIn"},
                       {StatusView::BubbleState::kFadingIn, u"kFadingIn"},
                       {StatusView::BubbleState::kShown, u"kShown"},
                       {StatusView::BubbleState::kPreFadeOut, u"kPreFadeOut"},
                       {StatusView::BubbleState::kFadingOut, u"kFadingOut"})

DEFINE_ENUM_CONVERTERS(StatusView::BubbleStyle,
                       {StatusView::BubbleStyle::kBottom, u"kBottom"},
                       {StatusView::BubbleStyle::kFloating, u"kFloating"},
                       {StatusView::BubbleStyle::kStandard, u"kStandard"},
                       {StatusView::BubbleStyle::kStandardRight,
                        u"kStandardRight"})

BEGIN_METADATA(StatusView)
ADD_PROPERTY_METADATA(std::u16string, Text)
ADD_READONLY_PROPERTY_METADATA(StatusView::BubbleState, State)
ADD_PROPERTY_METADATA(StatusView::BubbleStyle, Style)
END_METADATA

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
  set_delegate(nullptr);
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
  StatusViewExpander(const StatusViewExpander&) = delete;
  StatusViewExpander& operator=(const StatusViewExpander&) = delete;

  // Manage the expansion of the bubble.
  void StartExpansion(const std::u16string& expanded_text,
                      int current_width,
                      int expansion_end);

 private:
  // Animation functions.
  int GetCurrentBubbleWidth();
  void SetBubbleWidth(int width);
  void AnimateToState(double state) override;
  void AnimationEnded(const gfx::Animation* animation) override;

  // The status bubble that manages the popup widget and this object.
  raw_ptr<StatusBubbleViews> status_bubble_;

  // Change the bounds and text of this view.
  raw_ptr<StatusView> status_view_;

  // Text elided (if needed) to fit maximum status bar width.
  std::u16string expanded_text_;

  // Widths at expansion start and end.
  int expansion_start_ = 0;
  int expansion_end_ = 0;
};

void StatusBubbleViews::StatusViewExpander::AnimateToState(double state) {
  SetBubbleWidth(GetCurrentBubbleWidth());
}

void StatusBubbleViews::StatusViewExpander::AnimationEnded(
    const gfx::Animation* animation) {
  status_view_->SetText(expanded_text_);
  SetBubbleWidth(expansion_end_);
  // WARNING: crash data seems to indicate |this| may be deleted by the time
  // SetBubbleWidth() returns.
}

void StatusBubbleViews::StatusViewExpander::StartExpansion(
    const std::u16string& expanded_text,
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
      task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault().get()) {}

StatusBubbleViews::~StatusBubbleViews() {
  DestroyPopup();
}

void StatusBubbleViews::InitPopup() {
  if (!popup_) {
    DCHECK(!view_);
    DCHECK(!expand_view_);
    popup_ = std::make_unique<views::Widget>();

    views::Widget::InitParams params(
        views::Widget::InitParams::CLIENT_OWNS_WIDGET,
#if BUILDFLAG(IS_MAC)
        views::Widget::InitParams::TYPE_TOOLTIP);
#else
        views::Widget::InitParams::TYPE_POPUP);
#endif

#if BUILDFLAG(IS_WIN)
    // On Windows use the software compositor to ensure that we don't block
    // the UI thread blocking issue during command buffer creation. We can
    // revert this change once http://crbug.com/125248 is fixed.
    params.force_software_compositing = true;
#endif
    params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
    params.accept_events = false;
    views::Widget* frame = base_view_->GetWidget();
    params.parent = frame->GetNativeView();
    params.context = frame->GetNativeWindow();
    params.name = "StatusBubble";
#if BUILDFLAG(IS_CHROMEOS_ASH)
    params.init_properties_container.SetProperty(ash::kHideInOverviewKey, true);
    params.init_properties_container.SetProperty(ash::kHideInDeskMiniViewKey,
                                                 true);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    popup_->Init(std::move(params));
    // We do our own animation and don't want any from the system.
    popup_->SetVisibilityChangedAnimationsEnabled(false);
    popup_->SetOpacity(0.f);
    view_ = popup_->SetContentsView(std::make_unique<StatusView>(this));
    expand_view_ = std::make_unique<StatusViewExpander>(this, view_);
#if !BUILDFLAG(IS_MAC)
    // Stack the popup above the base widget and below higher z-order windows.
    // This is unnecessary and even detrimental on Mac, see CreateBubbleWidget.
    popup_->SetZOrderSublevel(ChromeWidgetSublevel::kSublevelHoverable);
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

  // Initializing the `popup_` views::Widget can trigger a window manager work
  // area change that calls into this function while `view_` is still null, so
  // check both `popup_` and `view_`.
  if (popup_.get() && view_ && contains_mouse_)
    AvoidMouse(last_mouse_moved_location_);
}

int StatusBubbleViews::GetWidthForURL(const std::u16string& url_string) {
  // Get the width of the elided url
  int elided_url_width = gfx::GetStringWidth(url_string, GetFont());
  // Add proper paddings
  return elided_url_width + (kShadowThickness + kTextHorizPadding) * 2 + 1;
}

void StatusBubbleViews::SetStatus(const std::u16string& status_text) {
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
  if (status_text_.empty()) {
    view_->AnimateForText(url_text_);
  } else {
    view_->SetText(status_text_);
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
    url_text_ = std::u16string();
    if (IsFrameVisible())
      view_->AnimateForText(status_text_);
    return;
  }

  // Set Elided Text corresponding to the GURL object.
  int text_width = static_cast<int>(
      size_.width() - (kShadowThickness + kTextHorizPadding) * 2 - 1);
  url_text_ = url_formatter::ElideUrl(url, GetFont(), text_width);

  // Get the width of the URL if the bubble width is the maximum size.
  std::u16string full_size_elided_url =
      url_formatter::ElideUrl(url, GetFont(), GetMaxStatusBubbleWidth());
  int url_width = GetWidthForURL(full_size_elided_url);

  // Get the width for the url if it is unexpanded.
  int unexpanded_width = std::min(url_width, GetStandardStatusBubbleWidth());

  // Reset expansion state only when bubble is completely hidden.
  if (view_->GetState() == StatusView::BubbleState::kHidden) {
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
          base::Milliseconds(kExpandHoverDelayMS));
    }
    // An URL is always treated as a left-to-right string. On right-to-left UIs
    // we need to explicitly mark the URL as LTR to make sure it is displayed
    // correctly.
    view_->AnimateForText(
        base::i18n::GetDisplayStringInLTRDirectionality(url_text_));
  }
}

void StatusBubbleViews::Hide() {
  status_text_ = std::u16string();
  url_text_ = std::u16string();
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

    if (view_->GetState() != StatusView::BubbleState::kHidden &&
        view_->GetState() != StatusView::BubbleState::kFadingOut &&
        view_->GetState() != StatusView::BubbleState::kPreFadeOut) {
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
      view_->SetStyle(StatusView::BubbleStyle::kBottom);
    } else if (offset > kBubbleCornerRadius / 2 - kShadowThickness) {
      view_->SetStyle(StatusView::BubbleStyle::kFloating);
    } else {
      view_->SetStyle(StatusView::BubbleStyle::kStandard);
    }

    // Check if the bubble sticks out from the monitor or will obscure
    // download shelf.
    gfx::NativeView view = base_view_->GetWidget()->GetNativeView();
    gfx::Rect monitor_rect =
        display::Screen::GetScreen()->GetDisplayNearestView(view).work_area();
    const int bubble_bottom_y = top_left.y() + position_.y() + size_.height();

    if (bubble_bottom_y + offset > monitor_rect.height() ||
        (download_shelf_is_visible_ &&
         (view_->GetStyle() == StatusView::BubbleStyle::kFloating ||
          view_->GetStyle() == StatusView::BubbleStyle::kBottom))) {
      // The offset is still too large. Move the bubble to the right and reset
      // Y offset_ to zero.
      view_->SetStyle(StatusView::BubbleStyle::kStandardRight);
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
             view_->GetStyle() == StatusView::BubbleStyle::kStandardRight) {
    offset_ = 0;
    view_->SetStyle(StatusView::BubbleStyle::kStandard);
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
