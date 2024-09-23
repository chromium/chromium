// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/toolbar_action_hover_card_controller.h"

#include "base/callback_list.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/ui/toolbar/toolbar_action_hover_card_types.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/browser/ui/views/toolbar/toolbar_action_hover_card_bubble_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_action_view.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/extension_features.h"
#include "ui/events/event_observer.h"
#include "ui/views/event_monitor.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace {

constexpr base::TimeDelta kTriggerDelay = base::Milliseconds(300);
constexpr base::TimeDelta kHoverCardSlideDuration = base::Milliseconds(200);

}  // namespace

// static
bool ToolbarActionHoverCardController::disable_animations_for_testing_ = false;

//-------------------------------------------------------------------
// ToolbarActionHoverCardController::EventSniffer

// Listens in on the browser event stream and hides an associated hover card
// on any keypress, mouse click, or gesture.
class ToolbarActionHoverCardController::EventSniffer
    : public ui::EventObserver {
 public:
  explicit EventSniffer(ToolbarActionHoverCardController* controller)
      : controller_(controller) {
    // Note that null is a valid value for the second parameter here; if for
    // some reason there is no native window it simply falls back to
    // application-wide event-sniffing, which for this case is better than not
    // watching events at all.
    event_monitor_ = views::EventMonitor::CreateWindowMonitor(
        this,
        controller_->extensions_container_->GetWidget()->GetNativeWindow(),
        {ui::EventType::kKeyPressed, ui::EventType::kKeyReleased,
         ui::EventType::kMousePressed, ui::EventType::kMouseReleased,
         ui::EventType::kGestureBegin, ui::EventType::kGestureEnd});
  }

  ~EventSniffer() override = default;

 protected:
  // ui::EventObserver:
  void OnEvent(const ui::Event& event) override {
    controller_->UpdateHoverCard(nullptr,
                                 ToolbarActionHoverCardUpdateType::kEvent);
  }

 private:
  const raw_ptr<ToolbarActionHoverCardController> controller_;
  std::unique_ptr<views::EventMonitor> event_monitor_;
};

//-------------------------------------------------------------------
// ToolbarActionHoverCardController

ToolbarActionHoverCardController::ToolbarActionHoverCardController(
    ExtensionsToolbarContainer* extensions_container)
    : extensions_container_(extensions_container) {}

ToolbarActionHoverCardController::~ToolbarActionHoverCardController() = default;

// static
bool ToolbarActionHoverCardController::UseAnimations() {
  return gfx::Animation::ShouldRenderRichAnimation();
}

bool ToolbarActionHoverCardController::IsHoverCardVisible() const {
  return hover_card_ != nullptr && hover_card_->GetWidget() &&
         !hover_card_->GetWidget()->IsClosed();
}

bool ToolbarActionHoverCardController::IsHoverCardShowingForAction(
    ToolbarActionView* action_view) const {
  DCHECK(action_view);
  return action_view->GetCurrentWebContents() && IsHoverCardVisible() &&
         !fade_animator_->IsFadingOut() && GetTargetAnchorView() == action_view;
}

void ToolbarActionHoverCardController::UpdateHoverCard(
    ToolbarActionView* action_view,
    ToolbarActionHoverCardUpdateType update_type) {
  if (!base::FeatureList::IsEnabled(
          extensions_features::kExtensionsMenuAccessControl)) {
    return;
  }

  // TODO(crbug.com/40857356): Check if we need to handle never displaying a
  // hover card for a toolbar action that is closing (pin was removed).

  // Update this ASAP so that if we try to fade-in and we have the wrong target
  // then when the fade timer elapses we won't incorrectly try to fade in on the
  // wrong action view.
  if (target_action_view_ != action_view) {
    delayed_show_timer_.Stop();
    target_action_view_observation_.Reset();
    if (action_view)
      target_action_view_observation_.Observe(action_view);
    target_action_view_ = action_view;
  }

  // If there's nothing to attach to then there's no point in creating a card.
  if (!hover_card_ && (!action_view || !action_view->GetCurrentWebContents() ||
                       !extensions_container_->GetWidget())) {
    return;
  }

  switch (update_type) {
    case ToolbarActionHoverCardUpdateType::kHover:
      if (!action_view)
        last_mouse_exit_timestamp_ = base::TimeTicks::Now();
      break;
    case ToolbarActionHoverCardUpdateType::kToolbarActionUpdated:
      DCHECK(action_view);
      DCHECK(IsHoverCardShowingForAction(action_view));
      break;
    case ToolbarActionHoverCardUpdateType::kToolbarActionRemoved:
      // Should not have an action view associated.
      DCHECK(!action_view);
      break;
    case ToolbarActionHoverCardUpdateType::kEvent:
      // No special action taken for this type of event.
      break;
  }

  if (action_view && action_view->GetCurrentWebContents())
    UpdateOrShowHoverCard(action_view, update_type);
  else
    HideHoverCard();
}

void ToolbarActionHoverCardController::UpdateOrShowHoverCard(
    ToolbarActionView* action_view,
    ToolbarActionHoverCardUpdateType update_type) {
  DCHECK(action_view);

  // Close is asynchronous, so make sure that if we're closing we clear out all
  // of our data *now* rather than waiting for the deletion message.
  if (hover_card_ && hover_card_->GetWidget()->IsClosed())
    OnViewIsDeleting(hover_card_);

  // Cancel any pending fades.
  if (hover_card_ && fade_animator_->IsFadingOut()) {
    fade_animator_->CancelFadeOut();
  }

  if (hover_card_) {
    // Card should never exist without an anchor or web contents
    DCHECK(hover_card_->GetAnchorView());
    UpdateHoverCardContent(action_view);

    // If widget is already visible and anchored to the correct action view we
    // should not try to reset the anchor view or reshow.
    if (!UseAnimations() || (hover_card_->GetAnchorView() == action_view &&
                             !slide_animator_->is_animating())) {
      slide_animator_->SnapToAnchorView(action_view);
    } else {
      slide_animator_->AnimateToAnchorView(action_view);
    }
    return;
  }

  // Maybe make hover card visible. Disabling animations for testing also
  // eliminates the show timer, lest the tests have to be significantly more
  // complex and time-consuming.
  const bool is_initial = !ShouldShowImmediately(action_view);
  if (is_initial && !disable_animations_for_testing_) {
    delayed_show_timer_.Start(
        FROM_HERE, kTriggerDelay,
        base::BindOnce(&ToolbarActionHoverCardController::ShowHoverCard,
                       weak_ptr_factory_.GetWeakPtr(), true, action_view));
  } else {
    // Just in case, cancel the timer. This shouldn't cancel a delayed capture
    // since delayed capture only happens when the hover card already exists,
    // and this code is only invoked if there is no hover card yet.
    delayed_show_timer_.Stop();
    DCHECK_EQ(target_action_view_, action_view);
    ShowHoverCard(is_initial, action_view);
  }
}

void ToolbarActionHoverCardController::UpdateHoverCardContent(
    ToolbarActionView* action_view) {
  DCHECK(action_view);
  content::WebContents* web_contents = action_view->GetCurrentWebContents();
  DCHECK(web_contents);

  // If the hover card is transitioning between extensions, we need to do a
  // cross-fade.
  if (hover_card_->GetAnchorView() != action_view) {
    hover_card_->SetTextFade(0.0);
  }

  std::u16string extension_name =
      action_view->view_controller()->GetActionName();
  std::u16string action_title =
      action_view->view_controller()->GetActionTitle(web_contents);
  // Hover card only uses the action title when it's different than the
  // extension name.
  action_title =
      extension_name == action_title ? std::u16string() : action_title;
  ToolbarActionViewController::HoverCardState state =
      action_view->view_controller()->GetHoverCardState(web_contents);

  hover_card_->UpdateCardContent(extension_name, action_title, state,
                                 web_contents);
}

void ToolbarActionHoverCardController::CreateHoverCard(
    ToolbarActionView* action_view) {
  DCHECK(action_view);

  hover_card_ = new ToolbarActionHoverCardBubbleView(action_view);
  hover_card_observation_.Observe(hover_card_.get());
  event_sniffer_ = std::make_unique<EventSniffer>(this);

  slide_animator_ = std::make_unique<views::BubbleSlideAnimator>(hover_card_);
  slide_animator_->SetSlideDuration(kHoverCardSlideDuration);
  slide_progressed_subscription_ =
      slide_animator_->AddSlideProgressedCallback(base::BindRepeating(
          &ToolbarActionHoverCardController::OnSlideAnimationProgressed,
          weak_ptr_factory_.GetWeakPtr()));
  slide_complete_subscription_ =
      slide_animator_->AddSlideCompleteCallback(base::BindRepeating(
          &ToolbarActionHoverCardController::OnSlideAnimationComplete,
          weak_ptr_factory_.GetWeakPtr()));

  fade_animator_ =
      std::make_unique<views::WidgetFadeAnimator>(hover_card_->GetWidget());
  fade_complete_subscription_ =
      fade_animator_->AddFadeCompleteCallback(base::BindRepeating(
          &ToolbarActionHoverCardController::OnFadeAnimationEnded,
          weak_ptr_factory_.GetWeakPtr()));
}

void ToolbarActionHoverCardController::ShowHoverCard(
    bool is_initial,
    const ToolbarActionView* intended_action_view) {
  // Make sure the hover card isn't accidentally shown if it's already visible
  // or if the anchor is gone or changed.
  if (hover_card_ || target_action_view_ != intended_action_view ||
      !TargetActionViewIsValid())
    return;

  CreateHoverCard(target_action_view_);
  UpdateHoverCardContent(target_action_view_);
  slide_animator_->UpdateTargetBounds();
  // TODO(crbug.com/40857356): Do we need to fix widget stack order? Revisit
  // this, specially after adding IPH.

  if (!is_initial || !UseAnimations()) {
    hover_card_->GetWidget()->Show();
    return;
  }

  fade_animator_->FadeIn();
}

void ToolbarActionHoverCardController::HideHoverCard() {
  if (!hover_card_ || hover_card_->GetWidget()->IsClosed())
    return;

  // Cancel any pending fade-in.
  if (fade_animator_->IsFadingIn()) {
    fade_animator_->CancelFadeIn();
  }

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

bool ToolbarActionHoverCardController::ShouldShowImmediately(
    const ToolbarActionView* action_view) const {
  // If less than `kShowWithoutDelayTimeBuffer` time has passed since the hover
  // card was last visible then it is shown immediately. This is to account for
  // if hover unintentionally leaves the extensions container.
  constexpr base::TimeDelta kShowWithoutDelayTimeBuffer =
      base::Milliseconds(300);
  base::TimeDelta elapsed_time =
      base::TimeTicks::Now() - last_mouse_exit_timestamp_;

  bool within_delay_time_buffer = !last_mouse_exit_timestamp_.is_null() &&
                                  elapsed_time <= kShowWithoutDelayTimeBuffer;
  // Hover cards should be shown without delay if triggered within the time
  // buffer.
  // TODO(crbug.com/40857356): Should hover cards be shown if the action view
  // is keyboard focused?
  return within_delay_time_buffer;
}

const views::View* ToolbarActionHoverCardController::GetTargetAnchorView()
    const {
  if (!hover_card_)
    return nullptr;
  if (slide_animator_->is_animating())
    return slide_animator_->desired_anchor_view();
  return hover_card_->GetAnchorView();
}

bool ToolbarActionHoverCardController::TargetActionViewIsValid() const {
  // TODO(crbug.com/40857356): Explore more conditions where an action view is
  // no longer valid.
  return target_action_view_ && target_action_view_->GetVisible();
}

void ToolbarActionHoverCardController::OnFadeAnimationEnded(
    views::WidgetFadeAnimator* animator,
    views::WidgetFadeAnimator::FadeType fade_type) {
  if (fade_type == views::WidgetFadeAnimator::FadeType::kFadeOut)
    hover_card_->GetWidget()->Close();
}

void ToolbarActionHoverCardController::OnSlideAnimationProgressed(
    views::BubbleSlideAnimator* animator,
    double value) {
  if (hover_card_)
    hover_card_->SetTextFade(value);
}

void ToolbarActionHoverCardController::OnSlideAnimationComplete(
    views::BubbleSlideAnimator* animator) {
  DCHECK(hover_card_);
  // Make sure we're displaying the new text at 100% opacity, and none of the
  // old text.
  hover_card_->SetTextFade(1.0);
}

void ToolbarActionHoverCardController::OnViewIsDeleting(
    views::View* observed_view) {
  if (hover_card_ == observed_view) {
    delayed_show_timer_.Stop();
    hover_card_observation_.Reset();
    event_sniffer_.reset();
    slide_progressed_subscription_ = base::CallbackListSubscription();
    slide_complete_subscription_ = base::CallbackListSubscription();
    fade_complete_subscription_ = base::CallbackListSubscription();
    slide_animator_.reset();
    fade_animator_.reset();
    hover_card_ = nullptr;
  } else if (target_action_view_ == observed_view) {
    UpdateHoverCard(nullptr,
                    ToolbarActionHoverCardUpdateType::kToolbarActionRemoved);
    // These postconditions should always be met after calling
    // UpdateHoverCard(nullptr, ...)
    DCHECK(!target_action_view_);
    DCHECK(!target_action_view_observation_.IsObserving());
  }
}

void ToolbarActionHoverCardController::OnViewVisibilityChanged(
    views::View* observed_view,
    views::View* starting_view) {
  // Only care about target action view becoming invisible.
  if (observed_view != target_action_view_)
    return;
  // Visibility comes from `starting_view` or the widget, if no starting view;
  // see documentation for ViewObserver::OnViewVisibilityChanged().
  const bool visible = starting_view
                           ? starting_view->GetVisible()
                           : (observed_view->GetWidget() &&
                              observed_view->GetWidget()->IsVisible());
  // If visibility changed to false, treat it as if the target action view had
  // gone away.
  if (!visible)
    OnViewIsDeleting(observed_view);
}
