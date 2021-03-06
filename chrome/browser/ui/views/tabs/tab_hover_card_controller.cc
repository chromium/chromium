// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_hover_card_controller.h"

#include "base/bind.h"
#include "base/callback_list.h"
#include "base/feature_list.h"
#include "base/memory/checked_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/metrics/tab_count_metrics.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/tabs/tab_hover_card_bubble_view.h"
#include "chrome/browser/ui/views/tabs/tab_hover_card_thumbnail_observer.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "components/tab_count_metrics/tab_count_metrics.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#include "ui/gfx/native_widget_types.h"
#endif

namespace {

#if BUILDFLAG(IS_CHROMEOS_ASH)
// UMA histograms that record animation smoothness for fade-in and fade-out
// animations of tab hover card.
constexpr char kHoverCardFadeInSmoothnessHistogramName[] =
    "Chrome.Tabs.AnimationSmoothness.HoverCard.FadeIn";
constexpr char kHoverCardFadeOutSmoothnessHistogramName[] =
    "Chrome.Tabs.AnimationSmoothness.HoverCard.FadeOut";

void RecordFadeInSmoothness(int smoothness) {
  UMA_HISTOGRAM_PERCENTAGE(kHoverCardFadeInSmoothnessHistogramName, smoothness);
}

void RecordFadeOutSmoothness(int smoothness) {
  UMA_HISTOGRAM_PERCENTAGE(kHoverCardFadeOutSmoothnessHistogramName,
                           smoothness);
}
#endif

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
// TabHoverCardController::CardCounter

// Tracks cards seen from the time the user enters the tabstrip until they
// select a tab with the mouse.
class TabHoverCardController::CardCounter {
 public:
  CardCounter() = default;
  CardCounter(const CardCounter& other) = delete;
  ~CardCounter() = default;
  void operator=(const CardCounter& other) = delete;

  // Resets the counter; called when a selection is finalized.
  void OnSelectionCommitted() {
    cards_seen_count_ = 0;
    last_tab_ = nullptr;
  }

  // Notes that a card was shown for |tab|. If the card is being shown right
  // away (either because the mouse only briefly left the tabstrip, or because
  // the user is sliding between tabs), |continuation| should be true; if it's
  // a fresh show after a break, |continuation| should be set to false.
  void CardShownForTab(const views::View* tab, bool continuation) {
    if (!continuation) {
      cards_seen_count_ = 1;
      last_tab_ = tab;
      return;
    }

    if (tab == last_tab_)
      return;

    last_tab_ = tab;
    ++cards_seen_count_;
  }

  // Records the number of cards seen before a mouse selection. Should be called
  // when the mouse is clicked on a tab, but before the selection is committed.
  void TabSelectedViaMouse() {
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
        Add(cards_seen_count_),
        base::Histogram::FactoryGet(
            tab_count_metrics::HistogramName(
                kHistogramPrefixHoverCardsSeenBeforeSelection,
                /* live_tabs_only */ false, bucket),
            kMinHoverCardsSeen, kMaxHoverCardsSeen, kHistogramBucketCount,
            base::HistogramBase::kUmaTargetedHistogramFlag));
  }

  int cards_seen_count() const { return cards_seen_count_; }

 private:
  int cards_seen_count_ = 0;

  // Keep this as an opaque pointer to avoid the temptation to dereference it;
  // there's a chance it could be dead.
  const void* last_tab_ = nullptr;
};

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
  const CheckedPtr<TabHoverCardController> controller_;
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
      cards_seen_counter_(std::make_unique<CardCounter>()) {}

TabHoverCardController::~TabHoverCardController() = default;

// static
bool TabHoverCardController::AreHoverCardImagesEnabled() {
  return base::FeatureList::IsEnabled(features::kTabHoverCardImages);
}

bool TabHoverCardController::IsHoverCardVisible() const {
  return hover_card_ != nullptr;
}

bool TabHoverCardController::IsHoverCardShowingForTab(Tab* tab) const {
  return IsHoverCardVisible() && !fade_animator_->IsFadingOut() &&
         GetTargetAnchorView() == tab;
}

void TabHoverCardController::UpdateHoverCard(
    Tab* tab,
    TabController::HoverCardUpdateType update_type) {
  // Update this ASAP so that if we try to fade-in and we have the wrong target
  // then when the fade timer elapses we won't incorrectly try to fade in or
  // fade in on the wrong tab.
  target_tab_ = tab;

  // If there's nothing to attach to then there's no point in creating a card.
  if (!hover_card_ && (!tab || !tab_strip_->GetWidget()))
    return;

  switch (update_type) {
    case TabController::HoverCardUpdateType::kSelectionChanged:
      cards_seen_counter_->OnSelectionCommitted();
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
    UpdateOrShowCard(tab);
  else
    HideHoverCard();
}

void TabHoverCardController::PreventImmediateReshow() {
  last_mouse_exit_timestamp_ = base::TimeTicks();
}

void TabHoverCardController::TabSelectedViaMouse() {
  cards_seen_counter_->TabSelectedViaMouse();
}

void TabHoverCardController::UpdateOrShowCard(Tab* tab) {
  RecordTimeSinceLastSeenMetric(base::TimeTicks::Now() -
                                last_visible_timestamp_);

  // Close is asynchronous, so make sure that if we're closing we clear out all
  // of our data *now* rather than waiting for the deletion message.
  if (hover_card_ && hover_card_->GetWidget()->IsClosed())
    OnViewIsDeleting(hover_card_);

  // Cancel any pending fades.
  if (hover_card_ && fade_animator_->IsFadingOut()) {
    fade_animator_->CancelFadeOut();
#if BUILDFLAG(IS_CHROMEOS_ASH)
    if (throughput_tracker_.has_value())
      throughput_tracker_->Cancel();
#endif
  }

  if (hover_card_) {
    // Card should never exist without an anchor.
    DCHECK(hover_card_->GetAnchorView());

    // We're showing a hover card for a new tab, so increment the count.
    cards_seen_counter_->CardShownForTab(tab, true);

    // If the card was visible we need to update the card now, before any slide
    // or snap occurs.
    UpdateCardContent(tab);

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
  const bool show_immediate = ShouldShowImmediately(tab);
  if (disable_animations_for_testing_ || show_immediate) {
    DCHECK_EQ(target_tab_, tab);
    ShowHoverCard(show_immediate);
  } else {
    delayed_show_timer_.Start(
        FROM_HERE, GetShowDelay(tab->width()),
        base::BindOnce(&TabHoverCardController::ShowHoverCard,
                       base::Unretained(this), false));
  }
}

void TabHoverCardController::ShowHoverCard(bool is_immediate) {
  // Make sure the hover card isn't accidentally shown if it's already visible
  // or if the anchor is gone.
  if (hover_card_ || !target_tab_)
    return;

  // We're showing a hover card for a new tab, so increment the count.
  cards_seen_counter_->CardShownForTab(target_tab_, is_immediate);

  CreateHoverCard(target_tab_);
  UpdateCardContent(target_tab_);

  if (is_immediate || !UseAnimations()) {
    hover_card_->GetWidget()->Show();
    return;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  throughput_tracker_.emplace(
      hover_card_->GetWidget()->GetCompositor()->RequestNewThroughputTracker());
  throughput_tracker_->Start(ash::metrics_util::ForSmoothness(
      base::BindRepeating(&RecordFadeInSmoothness)));
#endif
  fade_animator_->FadeIn();
}

void TabHoverCardController::HideHoverCard() {
  delayed_show_timer_.Stop();
  if (!hover_card_ || hover_card_->GetWidget()->IsClosed())
    return;

  if (thumbnail_observer_)
    thumbnail_observer_->Observe(nullptr);
  slide_animator_->StopAnimation();
  last_visible_timestamp_ = base::TimeTicks::Now();
  if (!UseAnimations()) {
    hover_card_->GetWidget()->Close();
    return;
  }
  if (fade_animator_->IsFadingOut())
    return;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  throughput_tracker_.emplace(
      hover_card_->GetWidget()->GetCompositor()->RequestNewThroughputTracker());
  throughput_tracker_->Start(ash::metrics_util::ForSmoothness(
      base::BindRepeating(&RecordFadeOutSmoothness)));
#endif
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

void TabHoverCardController::CreateHoverCard(Tab* tab) {
  hover_card_ = new TabHoverCardBubbleView(tab);
  hover_card_observation_.Observe(hover_card_.get());
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
  MaybeStartThumbnailObservation(tab);
}

void TabHoverCardController::MaybeStartThumbnailObservation(Tab* tab) {
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
  } else if (thumbnail != thumbnail_observer_->current_image()) {
    waiting_for_decompress_ = true;
    thumbnail_observer_->Observe(thumbnail);
  }
}

void TabHoverCardController::RecordTimeSinceLastSeenMetric(
    base::TimeDelta elapsed_time) {
  if (hover_card_ && !fade_animator_->IsFadingOut())
    return;
  constexpr base::TimeDelta kMaxHoverCardReshowTimeDelta =
      base::TimeDelta::FromSeconds(5);
  if (elapsed_time > kMaxHoverCardReshowTimeDelta)
    return;

  constexpr base::TimeDelta kMinHoverCardReshowTimeDelta =
      base::TimeDelta::FromMilliseconds(1);
  constexpr int kHoverCardHistogramBucketCount = 50;
  UMA_HISTOGRAM_CUSTOM_TIMES("TabHoverCards.TimeSinceLastVisible", elapsed_time,
                             kMinHoverCardReshowTimeDelta,
                             kMaxHoverCardReshowTimeDelta,
                             kHoverCardHistogramBucketCount);
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
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (throughput_tracker_.has_value())
    throughput_tracker_->Stop();
#endif
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
  if (waiting_for_decompress_)
    hover_card_->ClearPreviewImage();
}

void TabHoverCardController::OnPreviewImageAvaialble(
    TabHoverCardThumbnailObserver* observer,
    gfx::ImageSkia thumbnail_image) {
  DCHECK_EQ(thumbnail_observer_.get(), observer);
  waiting_for_decompress_ = false;
  hover_card_->SetPreviewImage(thumbnail_image);
}

int TabHoverCardController::GetCardsSeenCountForTesting() const {
  return cards_seen_counter_->cards_seen_count();
}
