// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_hover_card_controller.h"

#include "base/bind.h"
#include "base/callback_list.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/metrics/tab_count_metrics.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_contents_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/browser/ui/views/tabs/tab_hover_card_bubble_view.h"
#include "chrome/browser/ui/views/tabs/tab_hover_card_thumbnail_observer.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_popup_view.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/events/event_observer.h"
#include "ui/events/types/event_type.h"
#include "ui/views/event_monitor.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace {

// Fetches the Omnibox drop-down widget, or returns null if the drop-down is
// not visible.
void FixWidgetStackOrder(views::Widget* widget, const Browser* browser) {
#if defined(OS_LINUX)
  // Ensure the hover card Widget assumes the highest z-order to avoid occlusion
  // by other secondary UI Widgets (such as the omnibox Widget, see
  // crbug.com/1226536).
  widget->StackAtTop();
#else   // !deifned(OS_LINUX)
  // Hover card should always render above omnibox (see crbug.com/1272106).
  if (!browser || !widget)
    return;
  BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(browser);
  if (!browser_view)
    return;
  auto* const popup_view = browser_view->GetLocationBarView()
                               ->omnibox_view()
                               ->model()
                               ->get_popup_view();
  if (!popup_view || !popup_view->IsOpen())
    return;
  widget->StackAboveWidget(
      static_cast<OmniboxPopupContentsView*>(popup_view)->GetWidget());
#endif  // !defined(OS_LINUX)
}

base::TimeDelta GetPreviewImageCaptureDelay(
    ThumbnailImage::CaptureReadiness readiness) {
  int ms = 0;
  switch (readiness) {
    case ThumbnailImage::CaptureReadiness::kNotReady: {
      static const int not_ready_delay = base::GetFieldTrialParamByFeatureAsInt(
          features::kTabHoverCardImages,
          features::kTabHoverCardImagesNotReadyDelayParameterName, 800);
      ms = not_ready_delay;
      break;
    }
    case ThumbnailImage::CaptureReadiness::kReadyForInitialCapture: {
      static const int loading_delay = base::GetFieldTrialParamByFeatureAsInt(
          features::kTabHoverCardImages,
          features::kTabHoverCardImagesLoadingDelayParameterName, 300);
      ms = loading_delay;
      break;
    }
    case ThumbnailImage::CaptureReadiness::kReadyForFinalCapture: {
      static const int loaded_delay = base::GetFieldTrialParamByFeatureAsInt(
          features::kTabHoverCardImages,
          features::kTabHoverCardImagesLoadedDelayParameterName, 300);
      ms = loaded_delay;
      break;
    }
  }
  DCHECK_GE(ms, 0);
  return base::Milliseconds(ms);
}

base::TimeDelta GetShowDelay(int tab_width) {
  static const int max_width_additiona_delay =
      base::GetFieldTrialParamByFeatureAsInt(
          features::kTabHoverCardImages,
          features::kTabHoverCardAdditionalMaxWidthDelay, 500);

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
  constexpr base::TimeDelta kMinimumTriggerDelay = base::Milliseconds(300);
  if (tab_width < TabStyle::GetPinnedWidth())
    return kMinimumTriggerDelay;
  constexpr base::TimeDelta kMaximumTriggerDelay = base::Milliseconds(800);
  double logarithmic_fraction =
      std::log(tab_width - TabStyle::GetPinnedWidth() + 1) /
      std::log(TabStyle::GetStandardWidth() - TabStyle::GetPinnedWidth() + 1);
  base::TimeDelta scaling_factor = kMaximumTriggerDelay - kMinimumTriggerDelay;
  base::TimeDelta delay =
      logarithmic_fraction * scaling_factor + kMinimumTriggerDelay;
  if (tab_width >= TabStyle::GetStandardWidth())
    delay += base::Milliseconds(max_width_additiona_delay);
  return delay;
}

}  // anonymous namespace

//-------------------------------------------------------------------
// TabHoverCardController::EventSniffer

// Listens in on the browser event stream and hides an associated hover card
// on any keypress, mouse click, or gesture.
class TabHoverCardController::EventSniffer : public ui::EventObserver {
 public:
  explicit EventSniffer(TabHoverCardController* controller)
      : controller_(controller) {
    // Note that null is a valid value for the second parameter here; if for
    // some reason there is no native window it simply falls back to
    // application-wide event-sniffing, which for this case is better than not
    // watching events at all.
    event_monitor_ = views::EventMonitor::CreateWindowMonitor(
        this, controller_->tab_strip_->GetWidget()->GetNativeWindow(),
        {ui::ET_KEY_PRESSED, ui::ET_KEY_RELEASED, ui::ET_MOUSE_PRESSED,
         ui::ET_MOUSE_RELEASED, ui::ET_GESTURE_BEGIN, ui::ET_GESTURE_END});
  }

  ~EventSniffer() override = default;

 protected:
  // ui::EventObserver:
  void OnEvent(const ui::Event& event) override {
    bool close_hover_card = true;
    if (event.IsKeyEvent()) {
      // Hover card needs to be dismissed (and regenerated) if the keypress
      // would select the tab (this also takes focus out of the tabstrip).
      close_hover_card = event.AsKeyEvent()->key_code() == ui::VKEY_RETURN ||
                         event.AsKeyEvent()->key_code() == ui::VKEY_ESCAPE ||
                         !controller_->tab_strip_->IsFocusInTabs();
    }
    if (close_hover_card) {
      controller_->UpdateHoverCard(nullptr,
                                   TabController::HoverCardUpdateType::kEvent);
    }
  }

 private:
  const raw_ptr<TabHoverCardController> controller_;
  std::unique_ptr<views::EventMonitor> event_monitor_;
};

//-------------------------------------------------------------------
// TabHoverCardController

// static
bool TabHoverCardController::disable_animations_for_testing_ = false;

TabHoverCardController::TabHoverCardController(TabStrip* tab_strip)
    : tab_strip_(tab_strip),
      metrics_(std::make_unique<TabHoverCardMetrics>(this)) {}

TabHoverCardController::~TabHoverCardController() = default;

// static
bool TabHoverCardController::AreHoverCardImagesEnabled() {
  return base::FeatureList::IsEnabled(features::kTabHoverCardImages);
}

// static
bool TabHoverCardController::UseAnimations() {
  return !disable_animations_for_testing_ &&
         gfx::Animation::ShouldRenderRichAnimation();
}

bool TabHoverCardController::IsHoverCardVisible() const {
  return hover_card_ != nullptr && hover_card_->GetWidget() &&
         !hover_card_->GetWidget()->IsClosed();
}

bool TabHoverCardController::IsHoverCardShowingForTab(Tab* tab) const {
  return IsHoverCardVisible() && !fade_animator_->IsFadingOut() &&
         GetTargetAnchorView() == tab;
}

void TabHoverCardController::UpdateHoverCard(
    Tab* tab,
    TabController::HoverCardUpdateType update_type) {
  // Never display a hover card for a closing tab.
  if (tab && tab->closing())
    tab = nullptr;

  // Update this ASAP so that if we try to fade-in and we have the wrong target
  // then when the fade timer elapses we won't incorrectly try to fade in on the
  // wrong tab.
  if (target_tab_ != tab) {
    target_tab_observation_.Reset();
    if (tab)
      target_tab_observation_.Observe(tab);
    target_tab_ = tab;
    delayed_show_timer_.Stop();
  }

  // If there's nothing to attach to then there's no point in creating a card.
  if (!hover_card_ && (!tab || !tab_strip_->GetWidget()))
    return;

  switch (update_type) {
    case TabController::HoverCardUpdateType::kSelectionChanged:
      metrics_->TabSelectionChanged();
      break;
    case TabController::HoverCardUpdateType::kHover:
      if (!tab)
        last_mouse_exit_timestamp_ = base::TimeTicks::Now();
      break;
    case TabController::HoverCardUpdateType::kTabDataChanged:
      DCHECK(tab && IsHoverCardShowingForTab(tab));
      break;
    case TabController::HoverCardUpdateType::kTabRemoved:
    case TabController::HoverCardUpdateType::kAnimating:
      // Neither of these cases should have a tab associated.
      DCHECK(!tab);
      break;
    case TabController::HoverCardUpdateType::kEvent:
    case TabController::HoverCardUpdateType::kFocus:
      // No special action taken for this type of even (yet).
      break;
  }

  if (tab)
    UpdateOrShowCard(tab, update_type);
  else
    HideHoverCard();
}

void TabHoverCardController::PreventImmediateReshow() {
  last_mouse_exit_timestamp_ = base::TimeTicks();
}

void TabHoverCardController::TabSelectedViaMouse(Tab* tab) {
  metrics_->TabSelectedViaMouse(tab);
}

void TabHoverCardController::UpdateOrShowCard(
    Tab* tab,
    TabController::HoverCardUpdateType update_type) {
  // Close is asynchronous, so make sure that if we're closing we clear out all
  // of our data *now* rather than waiting for the deletion message.
  if (hover_card_ && hover_card_->GetWidget()->IsClosed())
    OnViewIsDeleting(hover_card_);

  // If a hover card is being updated because of a data change, the hover card
  // had better already be showing for the affected tab.
  if (update_type == TabController::HoverCardUpdateType::kTabDataChanged) {
    DCHECK(IsHoverCardShowingForTab(tab));
    UpdateCardContent(tab);
    slide_animator_->UpdateTargetBounds();
    return;
  }

  // Cancel any pending fades.
  if (hover_card_ && fade_animator_->IsFadingOut()) {
    fade_animator_->CancelFadeOut();
    metrics_->CardFadeCanceled();
  }

  if (hover_card_) {
    // Card should never exist without an anchor.
    DCHECK(hover_card_->GetAnchorView());

    // If the card was visible we need to update the card now, before any slide
    // or snap occurs.
    UpdateCardContent(tab);
    MaybeStartThumbnailObservation(tab, /* is_initial_show */ false);

    // If widget is already visible and anchored to the correct tab we should
    // not try to reset the anchor view or reshow.
    if (!UseAnimations() || (hover_card_->GetAnchorView() == tab &&
                             !slide_animator_->is_animating())) {
      slide_animator_->SnapToAnchorView(tab);
    } else {
      slide_animator_->AnimateToAnchorView(tab);
    }
    return;
  }

  // Maybe make hover card visible. Disabling animations for testing also
  // eliminates the show timer, lest the tests have to be significantly more
  // complex and time-consuming.
  const bool is_initial = !ShouldShowImmediately(tab);
  if (is_initial)
    metrics_->InitialCardBeingShown();
  if (is_initial && !disable_animations_for_testing_) {
    delayed_show_timer_.Start(
        FROM_HERE, GetShowDelay(tab->width()),
        base::BindOnce(&TabHoverCardController::ShowHoverCard,
                       base::Unretained(this), true, tab));
  } else {
    DCHECK_EQ(target_tab_, tab);
    ShowHoverCard(is_initial, tab);
  }
}

void TabHoverCardController::ShowHoverCard(bool is_initial,
                                           const Tab* intended_tab) {
  // Make sure the hover card isn't accidentally shown if it's already visible
  // or if the anchor is gone or changed.
  if (hover_card_ || !TargetTabIsValid() || target_tab_ != intended_tab)
    return;

  CreateHoverCard(target_tab_);
  UpdateCardContent(target_tab_);
  slide_animator_->UpdateTargetBounds();
  MaybeStartThumbnailObservation(target_tab_, is_initial);
  FixWidgetStackOrder(hover_card_->GetWidget(),
                      tab_strip_->controller()->GetBrowser());

  if (!is_initial || !UseAnimations()) {
    OnCardFullyVisible();
    hover_card_->GetWidget()->Show();
    return;
  }

  metrics_->CardFadingIn();
  fade_animator_->FadeIn();
}

void TabHoverCardController::HideHoverCard() {
  if (!hover_card_ || hover_card_->GetWidget()->IsClosed())
    return;

  if (thumbnail_observer_) {
    thumbnail_observer_->Observe(nullptr);
    thumbnail_wait_state_ = ThumbnailWaitState::kNotWaiting;
  }
  // This needs to be called whether we're doing a fade or a pop out.
  metrics_->CardWillBeHidden();
  slide_animator_->StopAnimation();
  if (!UseAnimations()) {
    hover_card_->GetWidget()->Close();
    return;
  }
  if (fade_animator_->IsFadingOut())
    return;

  metrics_->CardFadingOut();
  fade_animator_->FadeOut();
}

void TabHoverCardController::OnViewIsDeleting(views::View* observed_view) {
  if (hover_card_ == observed_view) {
    hover_card_observation_.Reset();
    event_sniffer_.reset();
    slide_progressed_subscription_ = base::CallbackListSubscription();
    slide_complete_subscription_ = base::CallbackListSubscription();
    fade_complete_subscription_ = base::CallbackListSubscription();
    slide_animator_.reset();
    fade_animator_.reset();
    hover_card_ = nullptr;
  } else if (target_tab_ == observed_view) {
    UpdateHoverCard(nullptr, TabController::HoverCardUpdateType::kTabRemoved);
    // These postconditions should always be met after calling
    // UpdateHoverCard(nullptr, ...)
    DCHECK(!target_tab_);
    DCHECK(!target_tab_observation_.IsObserving());
  }
}

size_t TabHoverCardController::GetTabCount() const {
  return tab_count_metrics::TabCount();
}

bool TabHoverCardController::ArePreviewsEnabled() const {
  return static_cast<bool>(thumbnail_observer_);
}

views::Widget* TabHoverCardController::GetHoverCardWidget() {
  return hover_card_ ? hover_card_->GetWidget() : nullptr;
}

void TabHoverCardController::CreateHoverCard(Tab* tab) {
  hover_card_ = new TabHoverCardBubbleView(tab);
  hover_card_observation_.Observe(hover_card_.get());
  event_sniffer_ = std::make_unique<EventSniffer>(this);
  slide_animator_ = std::make_unique<views::BubbleSlideAnimator>(hover_card_);
  slide_animator_->SetSlideDuration(
      TabHoverCardBubbleView::kHoverCardSlideDuration);
  slide_progressed_subscription_ = slide_animator_->AddSlideProgressedCallback(
      base::BindRepeating(&TabHoverCardController::OnSlideAnimationProgressed,
                          base::Unretained(this)));
  slide_complete_subscription_ = slide_animator_->AddSlideCompleteCallback(
      base::BindRepeating(&TabHoverCardController::OnSlideAnimationComplete,
                          base::Unretained(this)));
  fade_animator_ =
      std::make_unique<views::WidgetFadeAnimator>(hover_card_->GetWidget());
  fade_complete_subscription_ = fade_animator_->AddFadeCompleteCallback(
      base::BindRepeating(&TabHoverCardController::OnFadeAnimationEnded,
                          base::Unretained(this)));

  if (!thumbnail_observer_ && AreHoverCardImagesEnabled()) {
    thumbnail_observer_ = std::make_unique<TabHoverCardThumbnailObserver>();
    thumbnail_subscription_ = thumbnail_observer_->AddCallback(
        base::BindRepeating(&TabHoverCardController::OnPreviewImageAvaialble,
                            base::Unretained(this)));
  }
}

void TabHoverCardController::UpdateCardContent(Tab* tab) {
  // If the hover card is transitioning between tabs, we need to do a
  // cross-fade.
  if (hover_card_->GetAnchorView() != tab)
    hover_card_->SetTextFade(0.0);

  hover_card_->UpdateCardContent(tab);
}

void TabHoverCardController::MaybeStartThumbnailObservation(
    Tab* tab,
    bool is_initial_show) {
  // If the preview image feature is not enabled, |thumbnail_observer_| will be
  // null.
  if (!thumbnail_observer_)
    return;

  // Active tabs don't get thumbnails.
  if (tab->IsActive()) {
    thumbnail_observer_->Observe(nullptr);
    return;
  }

  auto thumbnail = tab->data().thumbnail;
  if (!thumbnail) {
    hover_card_->SetPlaceholderImage();
    thumbnail_wait_state_ = ThumbnailWaitState::kNotWaiting;
    return;
  }

  if (thumbnail == thumbnail_observer_->current_image())
    return;

  // We're definitely going to wait for an image at some point.
  const auto crossfade_at =
      TabHoverCardBubbleView::GetPreviewImageCrossfadeStart();
  if (crossfade_at.has_value() && crossfade_at.value() == 0.0) {
    hover_card_->SetPlaceholderImage();
    thumbnail_wait_state_ = ThumbnailWaitState::kWaitingWithPlaceholder;
  } else {
    thumbnail_wait_state_ = ThumbnailWaitState::kWaitingWithoutPlaceholder;
  }
  // For the first show there has already been a delay, so it's fine to ask for
  // the image immediately; same is true if we already have a thumbnail.
  //  Otherwise the delay is based on the capture readiness.
  const base::TimeDelta capture_delay =
      is_initial_show || thumbnail->has_data()
          ? base::TimeDelta()
          : GetPreviewImageCaptureDelay(thumbnail->GetCaptureReadiness());
  if (capture_delay.is_zero()) {
    thumbnail_observer_->Observe(thumbnail);
  } else if (!delayed_show_timer_.IsRunning()) {
    // Stop updating the preview image unless/until we re-enable capture.
    thumbnail_observer_->Observe(nullptr);
    if (thumbnail_wait_state_ ==
        ThumbnailWaitState::kWaitingWithoutPlaceholder) {
      hover_card_->SetPlaceholderImage();
      thumbnail_wait_state_ = ThumbnailWaitState::kWaitingWithPlaceholder;
    }
    delayed_show_timer_.Start(
        FROM_HERE, capture_delay,
        base::BindOnce(&TabHoverCardController::StartThumbnailObservation,
                       base::Unretained(this), tab));
  }
}

void TabHoverCardController::StartThumbnailObservation(Tab* tab) {
  if (tab != target_tab_)
    return;

  DCHECK(tab);
  DCHECK(hover_card_);
  DCHECK(waiting_for_preview());

  auto thumbnail = tab->data().thumbnail;
  if (!thumbnail || thumbnail == thumbnail_observer_->current_image())
    return;

  thumbnail_observer_->Observe(thumbnail);
}

bool TabHoverCardController::ShouldShowImmediately(const Tab* tab) const {
  // If less than |kShowWithoutDelayTimeBuffer| time has passed since the hover
  // card was last visible then it is shown immediately. This is to account for
  // if hover unintentionally leaves the tab strip.
  constexpr base::TimeDelta kShowWithoutDelayTimeBuffer =
      base::Milliseconds(300);
  base::TimeDelta elapsed_time =
      base::TimeTicks::Now() - last_mouse_exit_timestamp_;

  bool within_delay_time_buffer = !last_mouse_exit_timestamp_.is_null() &&
                                  elapsed_time <= kShowWithoutDelayTimeBuffer;
  // Hover cards should be shown without delay if triggered within the time
  // buffer or if the tab or its children have focus which indicates that the
  // tab is keyboard focused.
  const views::FocusManager* const tab_focus_manager = tab->GetFocusManager();
  return within_delay_time_buffer || tab->HasFocus() ||
         (tab_focus_manager &&
          tab->Contains(tab_focus_manager->GetFocusedView()));
}

const views::View* TabHoverCardController::GetTargetAnchorView() const {
  if (!hover_card_)
    return nullptr;
  if (slide_animator_->is_animating())
    return slide_animator_->desired_anchor_view();
  return hover_card_->GetAnchorView();
}

bool TabHoverCardController::TargetTabIsValid() const {
  return target_tab_ && tab_strip_->GetModelIndexOf(target_tab_) >= 0 &&
         !target_tab_->closing();
}

void TabHoverCardController::OnCardFullyVisible() {
  // We have to do a bunch of validity checks here because this happens on a
  // callback and so the tab may no longer be valid (or part of the original
  // tabstrip).
  const bool has_preview = ArePreviewsEnabled() && TargetTabIsValid() &&
                           !target_tab_->IsActive() && !waiting_for_preview();
  metrics_->CardFullyVisibleOnTab(target_tab_, has_preview);
}

void TabHoverCardController::OnFadeAnimationEnded(
    views::WidgetFadeAnimator* animator,
    views::WidgetFadeAnimator::FadeType fade_type) {
  // There's a potential race condition where we get the fade in complete signal
  // just as we've decided to fade out, so check for null.
  // See: crbug.com/1192451
  if (target_tab_ && fade_type == views::WidgetFadeAnimator::FadeType::kFadeIn)
    OnCardFullyVisible();

  metrics_->CardFadeComplete();
  if (fade_type == views::WidgetFadeAnimator::FadeType::kFadeOut)
    hover_card_->GetWidget()->Close();
}

void TabHoverCardController::OnSlideAnimationProgressed(
    views::BubbleSlideAnimator* animator,
    double value) {
  if (hover_card_)
    hover_card_->SetTextFade(value);
  if (thumbnail_wait_state_ == ThumbnailWaitState::kWaitingWithoutPlaceholder) {
    const auto crossfade_start =
        TabHoverCardBubbleView::GetPreviewImageCrossfadeStart();
    if (crossfade_start.has_value() && value >= crossfade_start.value()) {
      hover_card_->SetPlaceholderImage();
      thumbnail_wait_state_ = ThumbnailWaitState::kWaitingWithPlaceholder;
    }
  }
}

void TabHoverCardController::OnSlideAnimationComplete(
    views::BubbleSlideAnimator* animator) {
  // Make sure we're displaying the new text at 100% opacity, and none of the
  // old text.
  hover_card_->SetTextFade(1.0);

  // If we were waiting for a preview image with data to load, we don't want to
  // keep showing the old image while hovering on the new tab, so clear it. This
  // shouldn't happen very often for slide animations, but could on slower
  // computers.
  if (thumbnail_wait_state_ == ThumbnailWaitState::kWaitingWithoutPlaceholder) {
    hover_card_->SetPlaceholderImage();
    thumbnail_wait_state_ = ThumbnailWaitState::kWaitingWithPlaceholder;
  }

  OnCardFullyVisible();
}

void TabHoverCardController::OnPreviewImageAvaialble(
    TabHoverCardThumbnailObserver* observer,
    gfx::ImageSkia thumbnail_image) {
  DCHECK_EQ(thumbnail_observer_.get(), observer);

  const bool was_waiting_for_preview = waiting_for_preview();
  thumbnail_wait_state_ = ThumbnailWaitState::kNotWaiting;

  // The hover card could be destroyed before the preview image is delivered.
  if (!hover_card_)
    return;
  if (was_waiting_for_preview && target_tab_)
    metrics_->ImageLoadedForTab(target_tab_);
  // Can still set image on a fading-out hover card (we can change this behavior
  // later if we want).
  hover_card_->SetTargetTabImage(thumbnail_image);
}
