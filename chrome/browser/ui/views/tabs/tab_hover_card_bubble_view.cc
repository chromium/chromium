// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_hover_card_bubble_view.h"

#include <algorithm>
#include <memory>

#include "base/containers/mru_cache.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/metrics/tab_count_metrics.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/tabs/tab_renderer_data.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/thumbnails/thumbnail_image.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/grit/generated_resources.h"
#include "components/tab_count_metrics/tab_count_metrics.h"
#include "components/url_formatter/url_formatter.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/native_theme/native_theme.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/animation/animation_delegate_views.h"
#include "ui/views/background.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

#if defined(OS_WIN)
#include "ui/base/win/shell.h"
#endif

namespace {
// Maximum number of lines that a title label occupies.
int kTitleMaxLines = 2;

bool AreHoverCardImagesEnabled() {
  return base::FeatureList::IsEnabled(features::kTabHoverCardImages);
}

// Get delay threshold based on flag settings option selected. This is for
// user testing.
// TODO(corising): remove this after user study is completed.
base::TimeDelta GetMinimumTriggerDelay() {
  int delay_group = base::GetFieldTrialParamByFeatureAsInt(
      features::kTabHoverCards, features::kTabHoverCardsFeatureParameterName,
      2);
  switch (delay_group) {
    case 2:
      return base::TimeDelta::FromMilliseconds(150);
    case 1:
      return base::TimeDelta::FromMilliseconds(300);
    case 0:
    default:
      return base::TimeDelta::FromMilliseconds(0);
  }
}

base::TimeDelta GetMaximumTriggerDelay() {
  int delay_group = base::GetFieldTrialParamByFeatureAsInt(
      features::kTabHoverCards, features::kTabHoverCardsFeatureParameterName,
      2);
  switch (delay_group) {
    case 2:
      return base::TimeDelta::FromMilliseconds(500);
    case 1:
      return base::TimeDelta::FromMilliseconds(800);
    case 0:
    default:
      return base::TimeDelta::FromMilliseconds(0);
  }
}

bool CustomShadowsSupported() {
#if defined(OS_WIN)
  return ui::win::IsAeroGlassEnabled();
#else
  return true;
#endif
}

std::unique_ptr<views::View> CreateAlertView(const TabAlertState& state) {
  auto alert_state_label = std::make_unique<views::Label>(
      base::string16(), CONTEXT_BODY_TEXT_LARGE, views::style::STYLE_PRIMARY);
  alert_state_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  alert_state_label->SetMultiLine(true);
  alert_state_label->SetVisible(true);
  alert_state_label->SetText(chrome::GetTabAlertStateText(state));
  return alert_state_label;
}

}  // namespace

// static
bool TabHoverCardBubbleView::disable_animations_for_testing_ = false;

// TODO(corising): Move this to a place where it could be used for all widgets.
class TabHoverCardBubbleView::WidgetFadeAnimationDelegate
    : public views::AnimationDelegateViews {
 public:
  explicit WidgetFadeAnimationDelegate(views::Widget* hover_card)
      : AnimationDelegateViews(hover_card->GetRootView()),
        widget_(hover_card),
        fade_animation_(std::make_unique<gfx::LinearAnimation>(this)) {}
  ~WidgetFadeAnimationDelegate() override {}

  enum class FadeAnimationState {
    // No animation is running.
    IDLE,
    FADE_IN,
    FADE_OUT,
  };

  void set_animation_state(FadeAnimationState state) {
    animation_state_ = state;
  }

  bool IsFadingIn() const {
    return animation_state_ == FadeAnimationState::FADE_IN;
  }

  bool IsFadingOut() const {
    return animation_state_ == FadeAnimationState::FADE_OUT;
  }

  void FadeIn() {
    if (IsFadingIn())
      return;
    constexpr base::TimeDelta kFadeInDuration =
        base::TimeDelta::FromMilliseconds(200);
    set_animation_state(FadeAnimationState::FADE_IN);
    // Widgets cannot be shown when visible and fully transparent.
    widget_->SetOpacity(0.01f);
    widget_->Show();
    fade_animation_ = std::make_unique<gfx::LinearAnimation>(this);
    fade_animation_->SetDuration(kFadeInDuration);
    fade_animation_->Start();
  }

  void FadeOut() {
    if (IsFadingOut())
      return;
    constexpr base::TimeDelta kFadeOutDuration =
        base::TimeDelta::FromMilliseconds(150);
    fade_animation_ = std::make_unique<gfx::LinearAnimation>(this);
    set_animation_state(FadeAnimationState::FADE_OUT);
    fade_animation_->SetDuration(kFadeOutDuration);
    fade_animation_->Start();
  }

  void CancelFadeOut() {
    if (!IsFadingOut())
      return;

    fade_animation_->Stop();
    set_animation_state(FadeAnimationState::IDLE);
    widget_->SetOpacity(1.0f);
  }

 private:
  void AnimationProgressed(const gfx::Animation* animation) override {
    // Get the value of the animation with a material ease applied.
    double value = gfx::Tween::CalculateValue(gfx::Tween::FAST_OUT_SLOW_IN,
                                              animation->GetCurrentValue());
    float opaqueness = 0.0f;
    if (IsFadingOut()) {
      opaqueness = gfx::Tween::FloatValueBetween(value, 1.0f, 0.0f);
    } else if (animation_state_ == FadeAnimationState::FADE_IN) {
      opaqueness = gfx::Tween::FloatValueBetween(value, 0.0f, 1.0f);
    }

    if (IsFadingOut() && opaqueness == 0.0f) {
      widget_->Hide();
    } else {
      widget_->SetOpacity(opaqueness);
    }
  }

  void AnimationEnded(const gfx::Animation* animation) override {
    AnimationProgressed(animation);
    set_animation_state(FadeAnimationState::IDLE);
  }

  views::Widget* const widget_;
  std::unique_ptr<gfx::LinearAnimation> fade_animation_;
  FadeAnimationState animation_state_ = FadeAnimationState::IDLE;

  DISALLOW_COPY_AND_ASSIGN(WidgetFadeAnimationDelegate);
};

class TabHoverCardBubbleView::WidgetSlideAnimationDelegate
    : public views::AnimationDelegateViews {
 public:
  explicit WidgetSlideAnimationDelegate(
      TabHoverCardBubbleView* hover_card_delegate)
      : AnimationDelegateViews(hover_card_delegate),
        bubble_delegate_(hover_card_delegate),
        slide_animation_(std::make_unique<gfx::LinearAnimation>(this)) {
    slide_animation_->SetDuration(base::TimeDelta::FromMilliseconds(75));
  }
  ~WidgetSlideAnimationDelegate() override {}

  void AnimateToAnchorView(views::View* desired_anchor_view) {
    DCHECK(!current_bubble_bounds_.IsEmpty());
    desired_anchor_view_ = desired_anchor_view;
    starting_bubble_bounds_ = current_bubble_bounds_;
    target_bubble_bounds_ = CalculateTargetBounds(desired_anchor_view);
    slide_animation_->SetCurrentValue(0);
    slide_animation_->Start();
  }

  void StopAnimation() { AnimationCanceled(slide_animation_.get()); }

  // Stores the current bubble bounds now to be used when animating to a new
  // view. We do this now since the anchor view is needed to get bubble bounds
  // and could be deleting later when using the bounds to animate.
  void SetCurrentBounds() {
    current_bubble_bounds_ = bubble_delegate_->GetBubbleBounds();
  }

  gfx::Rect CalculateTargetBounds(views::View* desired_anchor_view) const {
    gfx::Rect anchor_bounds = desired_anchor_view->GetAnchorBoundsInScreen();
    anchor_bounds.Inset(bubble_delegate_->anchor_view_insets());
    return bubble_delegate_->GetBubbleFrameView()->GetUpdatedWindowBounds(
        anchor_bounds, bubble_delegate_->arrow(),
        bubble_delegate_->GetWidget()->client_view()->GetPreferredSize(), true);
  }

  bool is_animating() const { return slide_animation_->is_animating(); }
  views::View* desired_anchor_view() { return desired_anchor_view_; }
  const views::View* desired_anchor_view() const {
    return desired_anchor_view_;
  }

 private:
  void AnimationProgressed(const gfx::Animation* animation) override {
    double value = gfx::Tween::CalculateValue(
        gfx::Tween::FAST_OUT_SLOW_IN, slide_animation_->GetCurrentValue());
    current_bubble_bounds_ = gfx::Tween::RectValueBetween(
        value, starting_bubble_bounds_, target_bubble_bounds_);

    if (current_bubble_bounds_ == target_bubble_bounds_) {
      if (desired_anchor_view_)
        bubble_delegate_->SetAnchorView(desired_anchor_view_);
    }
    bubble_delegate_->GetWidget()->SetBounds(current_bubble_bounds_);
    bubble_delegate_->UpdateTextFade(value);
  }

  void AnimationEnded(const gfx::Animation* animation) override {
    desired_anchor_view_ = nullptr;
    bubble_delegate_->OnHoverCardLanded();
  }

  void AnimationCanceled(const gfx::Animation* animation) override {
    AnimationEnded(animation);
  }

  TabHoverCardBubbleView* const bubble_delegate_;
  std::unique_ptr<gfx::LinearAnimation> slide_animation_;
  views::View* desired_anchor_view_ = nullptr;
  gfx::Rect starting_bubble_bounds_;
  gfx::Rect target_bubble_bounds_;
  gfx::Rect current_bubble_bounds_;

  DISALLOW_COPY_AND_ASSIGN(WidgetSlideAnimationDelegate);
};

// This is a label with two tweaks:
// - a solid background color, which can have alpha
// - a function to make the foreground and background color fade away (via
//   alpha) to zero as an animation progresses
//
// It is used to overlay the old title and domain values as a hover card slide
// animation happens.
class TabHoverCardBubbleView::FadeLabel : public views::Label {
 public:
  using Label::Label;
  ~FadeLabel() override = default;

  // Sets the fade-out of the label as |percent| in the range [0, 1]. Since
  // FadeLabel is designed to mask new text with the old and then fade away, the
  // higher the percentage the less opaque the label.
  void SetFade(double percent) {
    if (percent >= 1.0)
      SetText(base::string16());
    const SkAlpha alpha = base::saturated_cast<SkAlpha>(
        std::numeric_limits<SkAlpha>::max() * (1.0 - percent));
    SetBackgroundColor(SkColorSetA(GetBackgroundColor(), alpha));
    SetEnabledColor(SkColorSetA(GetEnabledColor(), alpha));
  }

 protected:
  // views::Label:
  void OnPaintBackground(gfx::Canvas* canvas) override {
    canvas->DrawColor(GetBackgroundColor());
  }
};

// Maintains a set of thumbnails to watch, ensuring the capture count on the
// associated WebContents stays nonzero until a valid thumbnail has been
// captured.
class TabHoverCardBubbleView::ThumbnailWatcher {
 public:
  explicit ThumbnailWatcher(TabHoverCardBubbleView* hover_card)
      : hover_card_(hover_card) {}
  ~ThumbnailWatcher() = default;

  // Begin watching the specified thumbnail image for updates. Ideally, should
  // trigger the associated WebContents to load (if not loaded already) and
  // retrieve a valid thumbnail. If too many thumbnails are being watched, the
  // least-recently watched will be unwatched.
  void Watch(scoped_refptr<ThumbnailImage> thumbnail_image) {
    ThumbnailImage* const ptr = thumbnail_image.get();
    auto it = recent_observers_.Get(ptr);
    if (it == recent_observers_.end()) {
      recent_observers_.Put(ptr, std::make_unique<ThumbnailObserver>(
                                     this, std::move(thumbnail_image)));
    }
    ptr->RequestThumbnailImage();
  }

  // Returns the current (most recent) thumbnail being watched.
  ThumbnailImage* current_image() const {
    return recent_observers_.empty() ? nullptr
                                     : recent_observers_.begin()->first;
  }

  void OnNewImage(const ThumbnailImage* thumbnail, gfx::ImageSkia image) {
    DCHECK(!recent_observers_.empty());
    if (recent_observers_.begin()->first == thumbnail)
      hover_card_->OnThumbnailImageAvailable(std::move(image));
  }

 private:
  // Actually does the work of watching a single thumbnail. Cleans itself up
  // (including unregistering as an observer) on destruction.
  class ThumbnailObserver : public ThumbnailImage::Observer {
   public:
    ThumbnailObserver(ThumbnailWatcher* thumbnail_watcher,
                      scoped_refptr<ThumbnailImage> thumbnail_image)
        : thumbnail_watcher_(thumbnail_watcher),
          thumbnail_image_(std::move(thumbnail_image)) {
      scoped_observer_.Add(thumbnail_image_.get());
    }
    ~ThumbnailObserver() override = default;

    base::Optional<gfx::Size> GetThumbnailSizeHint() const override {
      return TabStyle::GetPreviewImageSize();
    }

    void OnThumbnailImageAvailable(gfx::ImageSkia preview_image) override {
      thumbnail_watcher_->OnNewImage(thumbnail_image_.get(),
                                     std::move(preview_image));
    }

   private:
    ThumbnailWatcher* const thumbnail_watcher_;
    scoped_refptr<ThumbnailImage> thumbnail_image_;
    ScopedObserver<ThumbnailImage, ThumbnailImage::Observer> scoped_observer_{
        this};
  };

  TabHoverCardBubbleView* const hover_card_;
  base::MRUCache<ThumbnailImage*, std::unique_ptr<ThumbnailObserver>>
      recent_observers_{5};
};

TabHoverCardBubbleView::TabHoverCardBubbleView(Tab* tab)
    : BubbleDialogDelegateView(tab, views::BubbleBorder::TOP_LEFT) {
  DialogDelegate::set_buttons(ui::DIALOG_BUTTON_NONE);

  // We'll do all of our own layout inside the bubble, so no need to inset this
  // view inside the client view.
  set_margins(gfx::Insets());

  // Inset the tab hover cards anchor rect to bring the card closer to the tab.
  constexpr gfx::Insets kTabHoverCardAnchorInsets(2, 0);
  set_anchor_view_insets(kTabHoverCardAnchorInsets);

  // Set so that when hovering over a tab in a inactive window that window will
  // not become active. Setting this to false creates the need to explicitly
  // hide the hovercard on press, touch, and keyboard events.
  SetCanActivate(false);

  // Set so that the tab hover card is not focus traversable when keyboard
  // navigating through the tab strip.
  set_focus_traversable_from_anchor_view(false);

  title_label_ = AddChildView(std::make_unique<views::Label>(
      base::string16(), CONTEXT_TAB_HOVER_CARD_TITLE,
      views::style::STYLE_PRIMARY));
  title_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_label_->SetVerticalAlignment(gfx::ALIGN_TOP);
  title_label_->SetMultiLine(true);
  title_label_->SetMaxLines(kTitleMaxLines);
  title_label_->SetProperty(views::kFlexBehaviorKey,
                            views::FlexSpecification::ForSizeRule(
                                views::MinimumFlexSizeRule::kPreferred,
                                views::MaximumFlexSizeRule::kPreferred,
                                /* adjust_height_for_width */ true));

  title_fade_label_ = AddChildView(std::make_unique<FadeLabel>(
      base::string16(), CONTEXT_TAB_HOVER_CARD_TITLE,
      views::style::STYLE_PRIMARY));
  title_fade_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_fade_label_->SetVerticalAlignment(gfx::ALIGN_TOP);
  title_fade_label_->SetMultiLine(true);
  title_fade_label_->SetMaxLines(kTitleMaxLines);

  domain_label_ = AddChildView(std::make_unique<views::Label>(
      base::string16(), CONTEXT_BODY_TEXT_LARGE, views::style::STYLE_SECONDARY,
      gfx::DirectionalityMode::DIRECTIONALITY_AS_URL));
  domain_label_->SetElideBehavior(gfx::ELIDE_HEAD);
  domain_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  domain_label_->SetMultiLine(false);

  domain_fade_label_ = AddChildView(std::make_unique<FadeLabel>(
      base::string16(), CONTEXT_BODY_TEXT_LARGE, views::style::STYLE_SECONDARY,
      gfx::DirectionalityMode::DIRECTIONALITY_AS_URL));
  domain_fade_label_->SetElideBehavior(gfx::ELIDE_HEAD);
  domain_fade_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  domain_fade_label_->SetMultiLine(false);

  if (AreHoverCardImagesEnabled()) {
    using Alignment = views::ImageView::Alignment;
    const gfx::Size preview_size = TabStyle::GetPreviewImageSize();
    preview_image_ = AddChildView(std::make_unique<views::ImageView>());
    preview_image_->SetVisible(AreHoverCardImagesEnabled());
    preview_image_->SetHorizontalAlignment(Alignment::kCenter);
    preview_image_->SetVerticalAlignment(Alignment::kCenter);
    preview_image_->SetImageSize(preview_size);
    preview_image_->SetPreferredSize(preview_size);
  }

  views::FlexLayout* const layout =
      SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kVertical);
  layout->SetMainAxisAlignment(views::LayoutAlignment::kStart);
  layout->SetCrossAxisAlignment(views::LayoutAlignment::kStretch);
  layout->SetCollapseMargins(true);
  layout->SetChildViewIgnoredByLayout(title_fade_label_, true);
  layout->SetChildViewIgnoredByLayout(domain_fade_label_, true);

  constexpr int kVerticalMargin = 10;
  constexpr int kHorizontalMargin = 18;
  constexpr int kLineSpacing = 0;
  title_label_->SetProperty(views::kMarginsKey,
                            gfx::Insets(kVerticalMargin, kHorizontalMargin,
                                        kLineSpacing, kHorizontalMargin));
  title_label_->SetProperty(views::kFlexBehaviorKey,
                            views::FlexSpecification::ForSizeRule(
                                views::MinimumFlexSizeRule::kScaleToMinimum,
                                views::MaximumFlexSizeRule::kPreferred));
  domain_label_->SetProperty(views::kMarginsKey,
                             gfx::Insets(kLineSpacing, kHorizontalMargin,
                                         kVerticalMargin, kHorizontalMargin));

  widget_ = views::BubbleDialogDelegateView::CreateBubble(this);
  set_adjust_if_offscreen(true);

  slide_animation_delegate_ =
      std::make_unique<WidgetSlideAnimationDelegate>(this);
  fade_animation_delegate_ =
      std::make_unique<WidgetFadeAnimationDelegate>(widget_);
  thumbnail_watcher_ = std::make_unique<ThumbnailWatcher>(this);

  constexpr int kFootnoteVerticalMargin = 8;
  GetBubbleFrameView()->set_footnote_margins(
      gfx::Insets(kFootnoteVerticalMargin, kHorizontalMargin,
                  kFootnoteVerticalMargin, kHorizontalMargin));
  GetBubbleFrameView()->set_preferred_arrow_adjustment(
      views::BubbleFrameView::PreferredArrowAdjustment::kOffset);
  GetBubbleFrameView()->set_hit_test_transparent(true);

  if (CustomShadowsSupported()) {
    GetBubbleFrameView()->SetCornerRadius(
        ChromeLayoutProvider::Get()->GetCornerRadiusMetric(
            views::EMPHASIS_HIGH));
  }
}

TabHoverCardBubbleView::~TabHoverCardBubbleView() = default;

void TabHoverCardBubbleView::UpdateAndShow(Tab* tab) {
  RecordTimeSinceLastSeenMetric(base::TimeTicks::Now() -
                                last_visible_timestamp_);
  // If less than |kShowWithoutDelayTimeBuffer| time has passed since the hover
  // card was last visible then it is shown immediately. This is to account for
  // if hover unintentionally leaves the tab strip.
  constexpr base::TimeDelta kShowWithoutDelayTimeBuffer =
      base::TimeDelta::FromMilliseconds(300);
  base::TimeDelta elapsed_time =
      base::TimeTicks::Now() - last_mouse_exit_timestamp_;

  bool within_delay_time_buffer = !last_mouse_exit_timestamp_.is_null() &&
                                  elapsed_time <= kShowWithoutDelayTimeBuffer;
  // Hover cards should be shown without delay if triggered within the time
  // buffer or if the tab or its children have focus which indicates that the
  // tab is keyboard focused.
  const views::FocusManager* tab_focus_manager = tab->GetFocusManager();
  bool show_immediately =
      within_delay_time_buffer || tab->HasFocus() ||
      (tab_focus_manager && tab->Contains(tab_focus_manager->GetFocusedView()));

  fade_animation_delegate_->CancelFadeOut();

  if (preview_image_)
    preview_image_->SetVisible(!tab->IsActive());

  // If we're not anchored, need to do this before updating card content.
  bool anchor_view_set = false;
  if (!GetAnchorView()) {
    SetAnchorView(tab);
    anchor_view_set = true;
  }

  UpdateCardContent(tab);

  // If widget is already visible and anchored to the correct tab we should not
  // try to reset the anchor view or reshow.
  if (widget_->IsVisible() && GetAnchorView() == tab &&
      !slide_animation_delegate_->is_animating()) {
    widget_->SetBounds(slide_animation_delegate_->CalculateTargetBounds(tab));
    slide_animation_delegate_->SetCurrentBounds();
    OnHoverCardLanded();
    return;
  }

  if (widget_->IsVisible())
    ++hover_cards_seen_count_;

  if (widget_->IsVisible() && !disable_animations_for_testing_) {
    slide_animation_delegate_->AnimateToAnchorView(tab);
  } else {
    if (!anchor_view_set)
      SetAnchorView(tab);
    widget_->SetBounds(slide_animation_delegate_->CalculateTargetBounds(tab));
    slide_animation_delegate_->SetCurrentBounds();
    OnHoverCardLanded();
  }

  if (!widget_->IsVisible()) {
    if (disable_animations_for_testing_ || show_immediately) {
      widget_->SetOpacity(1.0f);
      widget_->Show();
    } else {
      // Note that this will restart the timer if it is already running. If the
      // hover cards are not yet visible, moving the cursor within the tabstrip
      // will not trigger the hover cards.
      delayed_show_timer_.Start(FROM_HERE, GetDelay(tab->width()), this,
                                &TabHoverCardBubbleView::FadeInToShow);
    }
  }
}

bool TabHoverCardBubbleView::IsVisible() {
  return widget_->IsVisible();
}

void TabHoverCardBubbleView::FadeOutToHide() {
  delayed_show_timer_.Stop();
  if (!widget_->IsVisible())
    return;
  slide_animation_delegate_->StopAnimation();
  last_visible_timestamp_ = base::TimeTicks::Now();
  if (disable_animations_for_testing_) {
    widget_->Hide();
  } else {
    fade_animation_delegate_->FadeOut();
  }
}

bool TabHoverCardBubbleView::IsFadingOut() const {
  return fade_animation_delegate_->IsFadingOut();
}

views::View* TabHoverCardBubbleView::GetDesiredAnchorView() {
  return slide_animation_delegate_->is_animating()
             ? slide_animation_delegate_->desired_anchor_view()
             : GetAnchorView();
}

void TabHoverCardBubbleView::RecordHoverCardsSeenRatioMetric() {
  const char kHistogramPrefixHoverCardsSeenBeforeSelection[] =
      "TabHoverCards.TabHoverCardsSeenBeforeTabSelection";
  const size_t tab_count = tab_count_metrics::TabCount();
  const size_t bucket = tab_count_metrics::BucketForTabCount(tab_count);
  constexpr int kMinHoverCardsSeen = 0;
  constexpr int kMaxHoverCardsSeen = 100;
  constexpr int kHistogramBucketCount = 50;
  STATIC_HISTOGRAM_POINTER_GROUP(
      tab_count_metrics::HistogramName(
          kHistogramPrefixHoverCardsSeenBeforeSelection,
          /* live_tabs_only */ false, bucket),
      static_cast<int>(bucket),
      static_cast<int>(tab_count_metrics::kNumTabCountBuckets),
      Add(hover_cards_seen_count_),
      base::Histogram::FactoryGet(
          tab_count_metrics::HistogramName(
              kHistogramPrefixHoverCardsSeenBeforeSelection,
              /* live_tabs_only */ false, bucket),
          kMinHoverCardsSeen, kMaxHoverCardsSeen, kHistogramBucketCount,
          base::HistogramBase::kUmaTargetedHistogramFlag));
}

void TabHoverCardBubbleView::OnWidgetVisibilityChanged(views::Widget* widget,
                                                       bool visible) {
  if (visible)
    ++hover_cards_seen_count_;
}

ax::mojom::Role TabHoverCardBubbleView::GetAccessibleWindowRole() {
  // Override the role so that hover cards are not read when they appear because
  // tabs handle accessibility text.
  return ax::mojom::Role::kIgnored;
}

void TabHoverCardBubbleView::Layout() {
  View::Layout();
  title_fade_label_->SetBoundsRect(title_label_->bounds());
  domain_fade_label_->SetBoundsRect(domain_label_->bounds());
}

base::TimeDelta TabHoverCardBubbleView::GetDelay(int tab_width) const {
  // Delay is calculated as a logarithmic scale and bounded by a minimum width
  // based on the width of a pinned tab and a maximum of the standard width.
  //
  //  delay (ms)
  //           |
  // max delay-|                                    *
  //           |                          *
  //           |                    *
  //           |                *
  //           |            *
  //           |         *
  //           |       *
  //           |     *
  //           |    *
  // min delay-|****
  //           |___________________________________________ tab width
  //               |                                |
  //       pinned tab width               standard tab width
  base::TimeDelta minimum_trigger_delay = GetMinimumTriggerDelay();
  if (tab_width < TabStyle::GetPinnedWidth())
    return minimum_trigger_delay;
  base::TimeDelta maximum_trigger_delay = GetMaximumTriggerDelay();
  double logarithmic_fraction =
      std::log(tab_width - TabStyle::GetPinnedWidth() + 1) /
      std::log(TabStyle::GetStandardWidth() - TabStyle::GetPinnedWidth() + 1);
  base::TimeDelta scaling_factor =
      maximum_trigger_delay - minimum_trigger_delay;
  base::TimeDelta delay =
      logarithmic_fraction * scaling_factor + minimum_trigger_delay;
  return delay;
}

void TabHoverCardBubbleView::FadeInToShow() {
  fade_animation_delegate_->FadeIn();
}

void TabHoverCardBubbleView::UpdateCardContent(const Tab* tab) {
  base::string16 title;
  base::Optional<TabAlertState> old_alert_state = alert_state_;
  GURL domain_url;
  // Use committed URL to determine if no page has yet loaded, since the title
  // can be blank for some web pages.
  if (tab->data().last_committed_url.is_empty()) {
    domain_url = tab->data().visible_url;
    title = tab->data().IsCrashed()
                ? l10n_util::GetStringUTF16(IDS_HOVER_CARD_CRASHED_TITLE)
                : l10n_util::GetStringUTF16(IDS_TAB_LOADING_TITLE);
    alert_state_ = base::nullopt;
  } else {
    domain_url = tab->data().last_committed_url;
    title = tab->data().title;
    alert_state_ = tab->data().alert_state;
  }
  base::string16 domain;
  if (domain_url.SchemeIsFile()) {
    title_label_->SetMultiLine(false);
    title_label_->SetElideBehavior(gfx::ELIDE_MIDDLE);
    domain = l10n_util::GetStringUTF16(IDS_HOVER_CARD_FILE_URL_SOURCE);
  } else {
    title_label_->SetElideBehavior(gfx::ELIDE_TAIL);
    title_label_->SetMultiLine(true);
    domain = url_formatter::FormatUrl(
        domain_url,
        url_formatter::kFormatUrlOmitDefaults |
            url_formatter::kFormatUrlOmitHTTPS |
            url_formatter::kFormatUrlOmitTrivialSubdomains |
            url_formatter::kFormatUrlTrimAfterHost,
        net::UnescapeRule::NORMAL, nullptr, nullptr, nullptr);
  }
  UpdateTextFade(0.0);
  title_fade_label_->SetText(title_label_->GetText());
  title_label_->SetText(title);

  if (alert_state_ != old_alert_state) {
    GetBubbleFrameView()->SetFootnoteView(
        alert_state_.has_value() ? CreateAlertView(*alert_state_) : nullptr);
  }

  domain_fade_label_->SetText(domain_label_->GetText());
  domain_label_->SetText(domain);

  // If the preview image feature is not enabled, |preview_image_| will be null.
  if (preview_image_ && preview_image_->GetVisible()) {
    auto thumbnail = tab->data().thumbnail;
    if (!thumbnail) {
      ClearPreviewImage();
    } else if (thumbnail != thumbnail_watcher_->current_image()) {
      waiting_for_decompress_ = true;
      thumbnail_watcher_->Watch(thumbnail);
    }
  }
}

void TabHoverCardBubbleView::UpdateTextFade(double percent) {
  title_fade_label_->SetFade(percent);
  domain_fade_label_->SetFade(percent);
}

void TabHoverCardBubbleView::ClearPreviewImage() {
  // Check the no-preview color and size to see if it needs to be
  // regenerated. DPI or theme change can cause a regeneration.
  const SkColor foreground_color = GetThemeProvider()->GetColor(
      ThemeProperties::COLOR_HOVER_CARD_NO_PREVIEW_FOREGROUND);

  // Set the no-preview placeholder image. All sizes are in DIPs.
  // gfx::CreateVectorIcon() caches its result so there's no need to store
  // images here; if a particular size/color combination has already been
  // requested it will be low-cost to request it again.
  constexpr gfx::Size kNoPreviewImageSize{64, 64};
  const gfx::ImageSkia no_preview_image = gfx::CreateVectorIcon(
      kGlobeIcon, kNoPreviewImageSize.width(), foreground_color);
  preview_image_->SetImage(no_preview_image);
  preview_image_->SetImageSize(kNoPreviewImageSize);
  preview_image_->SetPreferredSize(TabStyle::GetPreviewImageSize());

  // Also possibly regenerate the background if it has changed.
  const SkColor background_color = GetThemeProvider()->GetColor(
      ThemeProperties::COLOR_HOVER_CARD_NO_PREVIEW_BACKGROUND);
  if (!preview_image_->background() ||
      preview_image_->background()->get_color() != background_color) {
    preview_image_->SetBackground(
        views::CreateSolidBackground(background_color));
  }

  waiting_for_decompress_ = false;
}

void TabHoverCardBubbleView::OnHoverCardLanded() {
  // Make sure we're displaying the new text at 100% opacity, and none of the
  // old text.
  UpdateTextFade(1.0);

  // If we were waiting for a preview image with data to load, we don't want to
  // keep showing the old image while hovering on the new tab, so clear it. This
  // shouldn't happen very often for slide animations, but could on slower
  // computers.
  if (waiting_for_decompress_)
    ClearPreviewImage();
}

void TabHoverCardBubbleView::OnThumbnailImageAvailable(
    gfx::ImageSkia preview_image) {
  const gfx::Size preview_size = TabStyle::GetPreviewImageSize();
  preview_image_->SetImage(preview_image);
  preview_image_->SetImageSize(preview_size);
  preview_image_->SetPreferredSize(preview_size);
  preview_image_->SetBackground(nullptr);
  waiting_for_decompress_ = false;
}

gfx::Size TabHoverCardBubbleView::CalculatePreferredSize() const {
  gfx::Size preferred_size = GetLayoutManager()->GetPreferredSize(this);
  preferred_size.set_width(TabStyle::GetPreviewImageSize().width());
  DCHECK(!preferred_size.IsEmpty());
  return preferred_size;
}

void TabHoverCardBubbleView::RecordTimeSinceLastSeenMetric(
    base::TimeDelta elapsed_time) {
  constexpr base::TimeDelta kMaxHoverCardReshowTimeDelta =
      base::TimeDelta::FromSeconds(5);
  if ((!widget_->IsVisible() || IsFadingOut()) &&
      elapsed_time <= kMaxHoverCardReshowTimeDelta) {
    constexpr base::TimeDelta kMinHoverCardReshowTimeDelta =
        base::TimeDelta::FromMilliseconds(1);
    constexpr int kHoverCardHistogramBucketCount = 50;
    UMA_HISTOGRAM_CUSTOM_TIMES("TabHoverCards.TimeSinceLastVisible",
                               elapsed_time, kMinHoverCardReshowTimeDelta,
                               kMaxHoverCardReshowTimeDelta,
                               kHoverCardHistogramBucketCount);
  }
}
