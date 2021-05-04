// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_hover_card_controller.h"

#include "base/bind.h"
#include "base/callback_list.h"
#include "base/feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/metrics/tab_count_metrics.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/tabs/tab_hover_card_bubble_view.h"
#include "chrome/browser/ui/views/tabs/tab_hover_card_thumbnail_observer.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#include "ui/gfx/native_widget_types.h"
#endif

namespace {

base::TimeDelta GetPreviewImageCaptureDelay(
    ThumbnailImage::CaptureReadiness readiness) {
  int ms = 0;
  switch (readiness) {
    case ThumbnailImage::CaptureReadiness::kNotReady: {
      static const int not_ready_delay = base::GetFieldTrialParamByFeatureAsInt(
          features::kTabHoverCardImages,
          features::kTabHoverCardImagesNotReadyDelayParameterName, 0);
      ms = not_ready_delay;
      break;
    }
    case ThumbnailImage::CaptureReadiness::kReadyForInitialCapture: {
      static const int loading_delay = base::GetFieldTrialParamByFeatureAsInt(
          features::kTabHoverCardImages,
          features::kTabHoverCardImagesLoadingDelayParameterName, 0);
      ms = loading_delay;
      break;
    }
    case ThumbnailImage::CaptureReadiness::kReadyForFinalCapture: {
      static const int loaded_delay = base::GetFieldTrialParamByFeatureAsInt(
          features::kTabHoverCardImages,
          features::kTabHoverCardImagesLoadedDelayParameterName, 0);
      ms = loaded_delay;
      break;
    }
  }
  DCHECK_GE(ms, 0);
  return base::TimeDelta::FromMilliseconds(ms);
}

base::TimeDelta GetShowDelay(int tab_width) {
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
  constexpr base::TimeDelta kMinimumTriggerDelay =
      base::TimeDelta::FromMilliseconds(300);
  if (tab_width < TabStyle::GetPinnedWidth())
    return kMinimumTriggerDelay;
  constexpr base::TimeDelta kMaximumTriggerDelay =
      base::TimeDelta::FromMilliseconds(800);
  double logarithmic_fraction =
      std::log(tab_width - TabStyle::GetPinnedWidth() + 1) /
      std::log(TabStyle::GetStandardWidth() - TabStyle::GetPinnedWidth() + 1);
  base::TimeDelta scaling_factor = kMaximumTriggerDelay - kMinimumTriggerDelay;
  base::TimeDelta delay =
      logarithmic_fraction * scaling_factor + kMinimumTriggerDelay;
  return delay;
}

}  // anonymous namespace

//-------------------------------------------------------------------
// TabHoverCardController::EventSniffer

// Listens in on the browser event stream (as a pre target event handler) and
// hides an associated hover card on any keypress.
class TabHoverCardController::EventSniffer : public ui::EventHandler,
                                             public views::WidgetObserver {
// On Mac, events should be added to the root view.
#if defined(USE_AURA)
  using OwnerView = gfx::NativeWindow;
#else
  using OwnerView = views::View*;
#endif

 public:
  explicit EventSniffer(TabHoverCardController* controller)
      : controller_(controller),
#if defined(USE_AURA)
        owner_view_(controller->tab_strip_->GetWidget()->GetNativeWindow()) {
#else
        owner_view_(controller->tab_strip_->GetWidget()->GetRootView()) {
#endif
    AddPreTargetHandler();
  }

  ~EventSniffer() override { RemovePreTargetHandler(); }

 protected:
  void AddPreTargetHandler() {
    widget_observation_.Observe(controller_->tab_strip_->GetWidget());
    if (owner_view_)
      owner_view_->AddPreTargetHandler(this);
  }

  void RemovePreTargetHandler() {
    widget_observation_.Reset();
    if (owner_view_) {
      owner_view_->RemovePreTargetHandler(this);
      owner_view_ = nullptr;
    }
  }

  // views::WidgetObesrver:
  void OnWidgetClosing(views::Widget* widget) override {
    // We can't wait until destruction to do this if the widget is going away
    // because we might try to access the NativeWidget after it's disposed.
    RemovePreTargetHandler();
  }

  // ui::EventTarget:
  void OnKeyEvent(ui::KeyEvent* event) override {
    // Hover card needs to be dismissed (and regenerated) if the keypress would
    // select the tab (this also takes focus out of the tabstrip).
    const bool is_select = event->key_code() == ui::VKEY_RETURN ||
                           event->key_code() == ui::VKEY_ESCAPE;
    if (is_select || !controller_->tab_strip_->IsFocusInTabs()) {
      controller_->UpdateHoverCard(nullptr,
                                   TabController::HoverCardUpdateType::kEvent);
    }
  }

  void OnMouseEvent(ui::MouseEvent* event) override {
    if (event->IsAnyButton()) {
      controller_->UpdateHoverCard(nullptr,
                                   TabController::HoverCardUpdateType::kEvent);
    }
  }

  void OnGestureEvent(ui::GestureEvent* event) override {
    controller_->UpdateHoverCard(nullptr,
                                 TabController::HoverCardUpdateType::kEvent);
  }

 private:
  TabHoverCardController* const controller_;
  OwnerView owner_view_;
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};
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
  if (hover_card_ || target_tab_ != intended_tab)
    return;

  CreateHoverCard(target_tab_);
  UpdateCardContent(target_tab_);
  MaybeStartThumbnailObservation(target_tab_, is_initial);

  if (!is_initial || !UseAnimations()) {
    metrics_->CardFullyVisibleOnTab(target_tab_, target_tab_->IsActive());
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
    waiting_for_preview_ = false;
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

// static
bool TabHoverCardController::UseAnimations() {
  return !disable_animations_for_testing_ &&
         gfx::Animation::ShouldRenderRichAnimation();
}

void TabHoverCardController::OnViewIsDeleting(views::View* observed_view) {
  DCHECK_EQ(hover_card_, observed_view);
  hover_card_observation_.Reset();
  event_sniffer_.reset();
  slide_progressed_subscription_ = base::CallbackListSubscription();
  slide_complete_subscription_ = base::CallbackListSubscription();
  fade_complete_subscription_ = base::CallbackListSubscription();
  slide_animator_.reset();
  fade_animator_.reset();
  hover_card_ = nullptr;
}

size_t TabHoverCardController::GetTabCount() const {
  return tab_count_metrics::TabCount();
}

bool TabHoverCardController::ArePreviewsEnabled() const {
  return static_cast<bool>(thumbnail_observer_);
}

bool TabHoverCardController::HasPreviewImage() const {
  return ArePreviewsEnabled() && hover_card_ && !waiting_for_preview_;
}

views::Widget* TabHoverCardController::GetHoverCardWidget() {
  return hover_card_ ? hover_card_->GetWidget() : nullptr;
}

void TabHoverCardController::CreateHoverCard(Tab* tab) {
  hover_card_ = new TabHoverCardBubbleView(tab);
  hover_card_observation_.Observe(hover_card_);
  event_sniffer_ = std::make_unique<EventSniffer>(this);
  slide_animator_ = std::make_unique<views::BubbleSlideAnimator>(hover_card_);
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
    hover_card_->ClearPreviewImage();
    return;
  }

  if (thumbnail == thumbnail_observer_->current_image())
    return;

  // We're definitely going to wait for an image at some point.
  waiting_for_preview_ = true;
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
    hover_card_->ClearPreviewImage();
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
  DCHECK(waiting_for_preview_);

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
      base::TimeDelta::FromMilliseconds(300);
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

void TabHoverCardController::OnFadeAnimationEnded(
    views::WidgetFadeAnimator* animator,
    views::WidgetFadeAnimator::FadeType fade_type) {
  // There's a potential race condition where we get the fade in complete signal
  // just as we've decided to fade out, so check for null.
  // See: crbug.com/1192451
  if (target_tab_ && fade_type == views::WidgetFadeAnimator::FadeType::kFadeIn)
    metrics_->CardFullyVisibleOnTab(target_tab_, target_tab_->IsActive());

  metrics_->CardFadeComplete();
  if (fade_type == views::WidgetFadeAnimator::FadeType::kFadeOut)
    hover_card_->GetWidget()->Close();
}

void TabHoverCardController::OnSlideAnimationProgressed(
    views::BubbleSlideAnimator* animator,
    double value) {
  if (hover_card_)
    hover_card_->SetTextFade(value);
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
  if (waiting_for_preview_)
    hover_card_->ClearPreviewImage();

  metrics_->CardFullyVisibleOnTab(target_tab_, target_tab_->IsActive());
}

void TabHoverCardController::OnPreviewImageAvaialble(
    TabHoverCardThumbnailObserver* observer,
    gfx::ImageSkia thumbnail_image) {
  DCHECK_EQ(thumbnail_observer_.get(), observer);

  const bool was_waiting_for_preview = waiting_for_preview_;
  waiting_for_preview_ = false;

  // The hover card could be destroyed before the preview image is delivered.
  if (!hover_card_)
    return;
  if (was_waiting_for_preview && target_tab_)
    metrics_->ImageLoadedForTab(target_tab_);
  // Can still set image on a fading-out hover card (we can change this behavior
  // later if we want).
  hover_card_->SetPreviewImage(thumbnail_image);
}
