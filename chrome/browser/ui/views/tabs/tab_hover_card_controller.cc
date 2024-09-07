// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_hover_card_controller.h"

#include <optional>

#include "base/callback_list.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/memory_pressure_monitor.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/chrome_widget_sublevel.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_view_views.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/browser/ui/views/tabs/tab_hover_card_bubble_view.h"
#include "chrome/browser/ui/views/tabs/tab_hover_card_thumbnail_observer.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/common/pref_names.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_popup_view.h"
#include "components/user_education/common/help_bubble_factory_registry.h"
#include "components/user_education/views/help_bubble_factory_views.h"
#include "components/user_education/views/help_bubble_view.h"
#include "ui/events/event.h"
#include "ui/events/event_observer.h"
#include "ui/events/types/event_type.h"
#include "ui/views/event_monitor.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace {

constexpr base::TimeDelta kMemoryPressureCaptureDelay = base::Milliseconds(500);

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
  const TabStyle* tab_style = TabStyle::Get();

  static const int max_width_additional_delay =
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
  if (tab_width < tab_style->GetPinnedWidth()) {
    return kMinimumTriggerDelay;
  }
  constexpr base::TimeDelta kMaximumTriggerDelay = base::Milliseconds(800);
  double logarithmic_fraction =
      std::log(tab_width - tab_style->GetPinnedWidth() + 1) /
      std::log(tab_style->GetStandardWidth() - tab_style->GetPinnedWidth() + 1);
  base::TimeDelta scaling_factor = kMaximumTriggerDelay - kMinimumTriggerDelay;
  base::TimeDelta delay =
      logarithmic_fraction * scaling_factor + kMinimumTriggerDelay;
  if (tab_width >= tab_style->GetStandardWidth()) {
    delay += base::Milliseconds(max_width_additional_delay);
  }
  return delay;
}

bool IsBrowserForSystemWebApp(const Browser* browser) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  const auto* const app_controller = browser->app_controller();
  if (app_controller && app_controller->system_app()) {
    return true;
  }
#endif
  return false;
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
        this,
        controller_->tab_strip_->GetWidget()
            ->GetTopLevelWidget()
            ->GetNativeWindow(),
        {ui::EventType::kKeyPressed, ui::EventType::kKeyReleased,
         ui::EventType::kMousePressed, ui::EventType::kMouseReleased,
         ui::EventType::kGestureBegin, ui::EventType::kGestureEnd});
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
      controller_->UpdateHoverCard(
          nullptr, TabSlotController::HoverCardUpdateType::kEvent);
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
      tab_resource_usage_collector_(TabResourceUsageCollector::Get()) {
  if (PrefService* pref_service = g_browser_process->local_state()) {
    // Hovercard image previews are still not fully rolled out to all platforms
    // so we default the pref to the state of the feature rollout.
    pref_service->SetDefaultPrefValue(prefs::kHoverCardImagesEnabled,
                                      base::Value(base::FeatureList::IsEnabled(
                                          features::kTabHoverCardImages)));

    pref_change_registrar_.Init(pref_service);

    // Register for previews enabled pref change events.
    hover_card_image_previews_enabled_ = AreHoverCardImagesEnabled();
    pref_change_registrar_.Add(
        prefs::kHoverCardImagesEnabled,
        base::BindRepeating(
            &TabHoverCardController::OnHovercardImagesEnabledChanged,
            base::Unretained(this)));

    // Register for memory usage enabled pref change events. Exclude
    // tracking them for system web apps (e.g. ChromeOS terminal app).
    if (!IsBrowserForSystemWebApp(tab_strip_->GetBrowser())) {
      OnHovercardMemoryUsageEnabledChanged();
      pref_change_registrar_.Add(
          prefs::kHoverCardMemoryUsageEnabled,
          base::BindRepeating(
              &TabHoverCardController::OnHovercardMemoryUsageEnabledChanged,
              base::Unretained(this)));
    }
  }
}

TabHoverCardController::~TabHoverCardController() = default;

// static
bool TabHoverCardController::AreHoverCardImagesEnabled() {
  if (base::FeatureList::IsEnabled(features::kTabHoverCardImages)) {
    PrefService* pref_service = g_browser_process->local_state();
    return pref_service->GetBoolean(prefs::kHoverCardImagesEnabled);
  }
  return false;
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
    TabSlotController::HoverCardUpdateType update_type) {
  // Never display a hover card for a closing tab.
  if (tab && tab->closing())
    tab = nullptr;

  // Update this ASAP so that if we try to fade-in and we have the wrong target
  // then when the fade timer elapses we won't incorrectly try to fade in on the
  // wrong tab.
  if (target_tab_ != tab) {
    delayed_show_timer_.Stop();
    target_tab_observation_.Reset();
    if (tab)
      target_tab_observation_.Observe(tab);
    target_tab_ = tab;
  }

  // If there's nothing to attach to then there's no point in creating a card.
  if (!hover_card_ && (!tab || !tab_strip_->GetWidget()))
    return;

  switch (update_type) {
    case TabSlotController::HoverCardUpdateType::kSelectionChanged:
      ResetCardsSeenCount();
      break;
    case TabSlotController::HoverCardUpdateType::kHover:
      if (!tab)
        last_mouse_exit_timestamp_ = base::TimeTicks::Now();
      break;
    case TabSlotController::HoverCardUpdateType::kTabDataChanged:
      DCHECK(tab && IsHoverCardShowingForTab(tab));
      break;
    case TabSlotController::HoverCardUpdateType::kTabRemoved:
    case TabSlotController::HoverCardUpdateType::kAnimating:
      // Neither of these cases should have a tab associated.
      DCHECK(!tab);
      break;
    case TabSlotController::HoverCardUpdateType::kEvent:
    case TabSlotController::HoverCardUpdateType::kFocus:
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

void TabHoverCardController::UpdateOrShowCard(
    Tab* tab,
    TabSlotController::HoverCardUpdateType update_type) {
  // Close is asynchronous, so make sure that if we're closing we clear out all
  // of our data *now* rather than waiting for the deletion message.
  if (hover_card_ && hover_card_->GetWidget()->IsClosed()) {
    OnViewIsDeleting(hover_card_);
  }

  // If a hover card is being updated because of a data change, the hover card
  // had better already be showing for the affected tab.
  if (update_type == TabSlotController::HoverCardUpdateType::kTabDataChanged) {
    if (!IsHoverCardShowingForTab(tab)) {
      return;
    }

    UpdateCardContent(tab);

    // When a tab has been discarded, the thumbnail is moved to a new
    // ThumbnailTabHelper so it must be observed again.
    if (tab->data().is_tab_discarded) {
      MaybeStartThumbnailObservation(tab, /* is_initial_show */ false);
    }

    slide_animator_->UpdateTargetBounds();
    return;
  }

  // Cancel any pending fades.
  if (hover_card_ && fade_animator_->IsFadingOut()) {
    fade_animator_->CancelFadeOut();
  }

  if (hover_card_) {
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
    ResetCardsSeenCount();
  if (is_initial && !disable_animations_for_testing_) {
    delayed_show_timer_.Start(
        FROM_HERE, GetShowDelay(tab->width()),
        base::BindOnce(&TabHoverCardController::ShowHoverCard,
                       weak_ptr_factory_.GetWeakPtr(), true, tab));
  } else {
    // Just in case, cancel the timer. This shouldn't cancel a delayed capture
    // since delayed capture only happens when the hover card already exists,
    // and this code is only invoked if there is no hover card yet.
    delayed_show_timer_.Stop();
    DCHECK_EQ(target_tab_, tab);
    ShowHoverCard(is_initial, tab);
  }
}

void TabHoverCardController::ShowHoverCard(bool is_initial,
                                           const Tab* intended_tab) {
  // Make sure the hover card isn't accidentally shown if it's already visible
  // or if the anchor is gone or changed.
  if (hover_card_ || target_tab_ != intended_tab || !TargetTabIsValid())
    return;

  CreateHoverCard(target_tab_);

  // For some reason, |target_tab_| can be rendered invalid before the next
  // call. There may be an asynchronous operation buried deep within
  // CreateHoverCard() above. Regardless, the validity needs to be checked
  // before the next call.
  // See: crbug.com/1295601, crbug.com/1322117, crbug.com/1348956
  // TODO(crbug.com/40865488): look into this and figure out what is actually
  // happening.
  if (!TargetTabIsValid()) {
    HideHoverCard();
    return;
  }

  UpdateCardContent(target_tab_);
  slide_animator_->UpdateTargetBounds();
  MaybeStartThumbnailObservation(target_tab_, is_initial);
  hover_card_->GetWidget()->SetZOrderSublevel(
      ChromeWidgetSublevel::kSublevelHoverable);

  if (!is_initial || !UseAnimations()) {
    OnCardFullyVisible();
    hover_card_->GetWidget()->Show();
    return;
  }

  fade_animator_->FadeIn();
}

void TabHoverCardController::HideHoverCard() {
  if (!hover_card_ || hover_card_->GetWidget()->IsClosed())
    return;

  // Required for test metrics.
  hover_card_last_seen_on_tab_ = nullptr;

  if (thumbnail_observer_) {
    thumbnail_observer_->Observe(nullptr);
    thumbnail_wait_state_ = ThumbnailWaitState::kNotWaiting;
  }

  // Cancel any pending fade-in.
  if (fade_animator_->IsFadingIn())
    fade_animator_->CancelFadeIn();

  // This needs to be called whether we're doing a fade or a pop out.
  slide_animator_->StopAnimation();
  if (!UseAnimations()) {
    hover_card_->GetWidget()->Close();
    return;
  }
  if (fade_animator_->IsFadingOut())
    return;

  fade_animator_->FadeOut();
}

void TabHoverCardController::OnViewIsDeleting(views::View* observed_view) {
  if (hover_card_ == observed_view) {
    tab_resource_usage_collector_->RemoveObserver(this);
    delayed_show_timer_.Stop();
    hover_card_observation_.Reset();
    event_sniffer_.reset();
    slide_progressed_subscription_ = base::CallbackListSubscription();
    slide_complete_subscription_ = base::CallbackListSubscription();
    fade_complete_subscription_ = base::CallbackListSubscription();
    slide_animator_.reset();
    fade_animator_.reset();
    hover_card_ = nullptr;
  } else if (target_tab_ == observed_view) {
    UpdateHoverCard(nullptr,
                    TabSlotController::HoverCardUpdateType::kTabRemoved);
    // These postconditions should always be met after calling
    // UpdateHoverCard(nullptr, ...)
    DCHECK(!target_tab_);
    DCHECK(!target_tab_observation_.IsObserving());
  }
}

void TabHoverCardController::OnViewVisibilityChanged(
    views::View* observed_view,
    views::View* starting_view) {
  // Only care about target tab becoming invisible.
  if (observed_view != target_tab_) {
    return;
  }
  // Visibility comes from `starting_view` or the widget, if no starting view;
  // see documentation for ViewObserver::OnViewVisibilityChanged().
  const bool visible = starting_view
                           ? starting_view->GetVisible()
                           : (observed_view->GetWidget() &&
                              observed_view->GetWidget()->IsVisible());
  // If visibility changed to false, treat it as if the target tab had gone
  // away.
  if (!visible) {
    OnViewIsDeleting(observed_view);
  }
}

void TabHoverCardController::OnTabResourceMetricsRefreshed() {
  if (hover_card_ != nullptr && target_tab_ != nullptr) {
    UpdateHoverCard(target_tab_,
                    TabSlotController::HoverCardUpdateType::kTabDataChanged);
  }
}

bool TabHoverCardController::ArePreviewsEnabled() const {
  return static_cast<bool>(thumbnail_observer_);
}

void TabHoverCardController::CreateHoverCard(Tab* tab) {
  TabHoverCardBubbleView::InitParams params;
  params.use_animation = UseAnimations();
  // In some browser types (e.g. ChromeOS terminal app) hide the domain label.
  params.show_domain = !IsBrowserForSystemWebApp(tab_strip_->GetBrowser());
  params.show_memory_usage = hover_card_memory_usage_enabled_;
  params.show_image_preview = hover_card_image_previews_enabled_;

  hover_card_ = new TabHoverCardBubbleView(tab, params);
  hover_card_observation_.Observe(hover_card_.get());
  event_sniffer_ = std::make_unique<EventSniffer>(this);
  slide_animator_ = std::make_unique<views::BubbleSlideAnimator>(hover_card_);
  slide_animator_->SetSlideDuration(
      TabHoverCardBubbleView::kHoverCardSlideDuration);
  slide_progressed_subscription_ = slide_animator_->AddSlideProgressedCallback(
      base::BindRepeating(&TabHoverCardController::OnSlideAnimationProgressed,
                          weak_ptr_factory_.GetWeakPtr()));
  slide_complete_subscription_ = slide_animator_->AddSlideCompleteCallback(
      base::BindRepeating(&TabHoverCardController::OnSlideAnimationComplete,
                          weak_ptr_factory_.GetWeakPtr()));
  fade_animator_ =
      std::make_unique<views::WidgetFadeAnimator>(hover_card_->GetWidget());
  fade_complete_subscription_ = fade_animator_->AddFadeCompleteCallback(
      base::BindRepeating(&TabHoverCardController::OnFadeAnimationEnded,
                          weak_ptr_factory_.GetWeakPtr()));

  if (!thumbnail_observer_ && hover_card_image_previews_enabled_) {
    thumbnail_observer_ = std::make_unique<TabHoverCardThumbnailObserver>();
    thumbnail_subscription_ = thumbnail_observer_->AddCallback(
        base::BindRepeating(&TabHoverCardController::OnPreviewImageAvailable,
                            weak_ptr_factory_.GetWeakPtr()));
  }

  tab_resource_usage_collector_->AddObserver(this);
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

  // Discarded tabs that don't already have a thumbnail won't get one.
  if (tab->IsDiscarded() && !tab->HasThumbnail()) {
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

  // We're probably going ask for a preview image, so figure out whether we
  // want to capture now, later, or at all, and whether to show a placeholder
  // in the meantime.

  // The crossfade parameter determines when a placeholder image is displayed.
  const auto crossfade_at =
      TabHoverCardBubbleView::GetPreviewImageCrossfadeStart();
  if (UseAnimations() && crossfade_at.has_value() &&
      crossfade_at.value() == 0.0) {
    hover_card_->SetPlaceholderImage();
    thumbnail_wait_state_ = ThumbnailWaitState::kWaitingWithPlaceholder;
  } else {
    thumbnail_wait_state_ = ThumbnailWaitState::kWaitingWithoutPlaceholder;
  }

  // For the first show there has already been a delay, so it's fine to ask for
  // the image immediately; same is true if we already have a thumbnail.
  // Otherwise the delay is based on the capture readiness.
  base::TimeDelta capture_delay =
      is_initial_show || thumbnail->has_data()
          ? base::TimeDelta()
          : GetPreviewImageCaptureDelay(thumbnail->GetCaptureReadiness());

  // Under memory pressure, we will additionally delay the initial capture, so
  // that generating the image is a more deliberate choice from the user. The
  // memory pressure monitor is disabled in tests.
  if (const auto* const monitor = base::MemoryPressureMonitor::Get()) {
    switch (monitor->GetCurrentPressureLevel()) {
      case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL:
        capture_delay = base::TimeDelta::Max();
        break;
      case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE:
        capture_delay += kMemoryPressureCaptureDelay;
        break;
      case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE:
        break;
    }
  }

  if (capture_delay.is_zero()) {
    thumbnail_observer_->Observe(thumbnail);
    return;
  }

  // If we've already waiting on this tab, we're done.
  if (delayed_show_timer_.IsRunning())
    return;

  // Stop updating the preview image unless/until we re-enable capture.
  thumbnail_observer_->Observe(nullptr);
  if (thumbnail_wait_state_ == ThumbnailWaitState::kWaitingWithoutPlaceholder) {
    hover_card_->SetPlaceholderImage();
    thumbnail_wait_state_ = ThumbnailWaitState::kWaitingWithPlaceholder;
  }

  // If we've elected to put off capture indefinitely (likely due to memory
  // pressure), there's no additional work to do.
  if (capture_delay.is_inf())
    return;

  // Start a delayed capture.
  delayed_show_timer_.Start(
      FROM_HERE, capture_delay,
      base::BindOnce(&TabHoverCardController::StartThumbnailObservation,
                     base::Unretained(this), tab));
}

void TabHoverCardController::StartThumbnailObservation(Tab* tab) {
  if (tab != target_tab_)
    return;

  // If the preview image feature is not enabled, |thumbnail_observer_| will be
  // null.
  if (!thumbnail_observer_) {
    return;
  }

  DCHECK(tab);
  DCHECK(hover_card_);
  DCHECK(waiting_for_preview());

  // Do not capture thumbnails during critical memory pressure.
  const auto* const monitor = base::MemoryPressureMonitor::Get();
  if (monitor &&
      monitor->GetCurrentPressureLevel() ==
          base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL) {
    // Because we're blocked, we'll show a placeholder instead of nothing or
    // the wrong image.
    if (thumbnail_wait_state_ ==
        ThumbnailWaitState::kWaitingWithoutPlaceholder) {
      hover_card_->SetPlaceholderImage();
      thumbnail_wait_state_ = ThumbnailWaitState::kWaitingWithPlaceholder;
    }
    return;
  }

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
  // There are a bunch of conditions under which a tab may no longer be valid,
  // including no longer belonging to the same tabstrip, being dragged or
  // detached, or just not being visible. We need to be vigilant about invalid
  // tabs due to e.g. crbug.com/1295601.
  return target_tab_ && tab_strip_->GetModelIndexOf(target_tab_).has_value() &&
         !target_tab_->closing() && !target_tab_->detached() &&
         !target_tab_->dragging() && target_tab_->GetVisible();
}

void TabHoverCardController::OnCardFullyVisible() {
  DCHECK(target_tab_);
  if (target_tab_ == hover_card_last_seen_on_tab_.get()) {
    return;
  }
  hover_card_last_seen_on_tab_ = target_tab_;
  ++hover_cards_seen_count_;
}

void TabHoverCardController::ResetCardsSeenCount() {
  hover_card_last_seen_on_tab_ = nullptr;
  hover_cards_seen_count_ = 0;
}

void TabHoverCardController::OnFadeAnimationEnded(
    views::WidgetFadeAnimator* animator,
    views::WidgetFadeAnimator::FadeType fade_type) {
  // There's a potential race condition where we get the fade in complete signal
  // just as we've decided to fade out, so check for null.
  // See: crbug.com/1192451
  if (target_tab_ && fade_type == views::WidgetFadeAnimator::FadeType::kFadeIn)
    OnCardFullyVisible();

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

void TabHoverCardController::OnPreviewImageAvailable(
    TabHoverCardThumbnailObserver* observer,
    gfx::ImageSkia thumbnail_image) {
  DCHECK_EQ(thumbnail_observer_.get(), observer);

  thumbnail_wait_state_ = ThumbnailWaitState::kNotWaiting;

  // The hover card could be destroyed before the preview image is delivered.
  if (!hover_card_)
    return;
  // Can still set image on a fading-out hover card (we can change this behavior
  // later if we want).
  hover_card_->SetTargetTabImage(thumbnail_image);
}

void TabHoverCardController::OnHovercardImagesEnabledChanged() {
  hover_card_image_previews_enabled_ = AreHoverCardImagesEnabled();
  if (!hover_card_image_previews_enabled_) {
    thumbnail_subscription_ = base::CallbackListSubscription();
    thumbnail_observer_.reset();
  }
}

void TabHoverCardController::OnHovercardMemoryUsageEnabledChanged() {
  hover_card_memory_usage_enabled_ =
      g_browser_process->local_state()->GetBoolean(
          prefs::kHoverCardMemoryUsageEnabled);
}
