// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/chip/chip_controller.h"

#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/page_info/page_info_dialog.h"
#include "chrome/browser/ui/views/content_setting_bubble_contents.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view.h"
#include "chrome/browser/ui/views/permissions/chip/permission_dashboard_controller.h"
#include "chrome/browser/ui/views/permissions/chip/permission_dashboard_view.h"
#include "chrome/browser/ui/views/permissions/chip/permission_prompt_chip_model.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_bubble_view_factory.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_style.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/common/features.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_prompt.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/permission_util.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/visibility.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/widget/widget.h"

namespace {

constexpr auto kConfirmationDisplayDuration = base::Seconds(4);
constexpr auto kCollapseDelay = base::Seconds(12);
constexpr auto kDismissDelay = base::Seconds(6);
// Abusive origins do not support expand animation, hence the dismiss timer
// should be longer.
constexpr auto kDismissDelayForAbusiveOrigins = kDismissDelay * 3;

constexpr auto kLHSIndicatorCollapseAnimationDuration = base::Milliseconds(250);

base::TimeDelta GetAnimationDuration(base::TimeDelta duration) {
  return gfx::Animation::ShouldRenderRichAnimation() ? duration
                                                     : base::TimeDelta();
}
}  // namespace

class BubbleButtonController : public views::ButtonController {
 public:
  BubbleButtonController(
      views::Button* button,
      BubbleOwnerDelegate* bubble_owner,
      std::unique_ptr<views::ButtonControllerDelegate> delegate)
      : views::ButtonController(button, std::move(delegate)),
        bubble_owner_(bubble_owner) {}

  // TODO(crbug.com/40205454): Add keyboard support.
  void OnMouseEntered(const ui::MouseEvent& event) override {
    if (bubble_owner_->IsBubbleShowing() || bubble_owner_->IsAnimating()) {
      return;
    }
    bubble_owner_->RestartTimersOnMouseHover();
  }

 private:
  raw_ptr<BubbleOwnerDelegate, DanglingUntriaged> bubble_owner_ = nullptr;
};

ChipController::ChipController(
    LocationBarView* location_bar_view,
    PermissionChipView* chip_view,
    PermissionDashboardView* permission_dashboard_view,
    PermissionDashboardController* permission_dashboard_controller)
    : location_bar_view_(location_bar_view),
      chip_(chip_view),
      permission_dashboard_view_(permission_dashboard_view),
      permission_dashboard_controller_(permission_dashboard_controller) {
  chip_->SetVisible(false);
}

ChipController::~ChipController() {
  views::Widget* current = GetBubbleWidget();
  if (current) {
    current->RemoveObserver(this);
    current->Close();
  }
  if (active_chip_permission_request_manager_.has_value()) {
    active_chip_permission_request_manager_.value()->RemoveObserver(this);
  }
  observation_.Reset();
}

void ChipController::OnPermissionRequestManagerDestructed() {
  ResetPermissionPromptChip();
  if (active_chip_permission_request_manager_.has_value()) {
    active_chip_permission_request_manager_.value()->RemoveObserver(this);
    active_chip_permission_request_manager_.reset();
  }
}

void ChipController::OnNavigation(
    content::NavigationHandle* navigation_handle) {
  // TODO(crbug.com/40256881): Refactor this so that this observer method is
  // only called when a non-same-document navigation starts in the primary main
  // frame.
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  ResetPermissionPromptChip();
}

void ChipController::OnTabVisibilityChanged(content::Visibility visibility) {
  if (visibility == content::Visibility::HIDDEN) {
    ResetPermissionPromptChip();
  }
}

void ChipController::OnRequestsFinalized() {
  // Due to a permissions requests queue reordering, currently active
  // permission request may get finalized without a user deciding on a
  // permission prompt. That means `OnRequestDecided` will not be executed.
  ResetPermissionRequestChip();
}

void ChipController::OnPromptRemoved() {
  ResetPermissionRequestChip();
}

void ChipController::OnRequestDecided(
    permissions::PermissionAction permission_action) {
  RemoveBubbleObserverAndResetTimersAndChipCallbacks();
  if (!GetLocationBarView()->IsDrawn() ||
      GetLocationBarView()->GetWidget()->GetTopLevelWidget()->IsFullscreen() ||
      permission_action == permissions::PermissionAction::IGNORED ||
      permission_action == permissions::PermissionAction::DISMISSED ||
      permission_action == permissions::PermissionAction::REVOKED ||
      // Do not show the confirmation chip for Camera and Mic because they will
      // be displayed as LHS indicator.
      (base::FeatureList::IsEnabled(
           content_settings::features::kLeftHandSideActivityIndicators) &&
       (permission_prompt_model_->content_settings_type() ==
            ContentSettingsType::MEDIASTREAM_CAMERA ||
        permission_prompt_model_->content_settings_type() ==
            ContentSettingsType::MEDIASTREAM_MIC))) {
    // Reset everything and hide chip if:
    // - `LocationBarView` isn't visible
    // - Permission request was ignored or dismissed as we do not confirm such
    // actions.
    // - LHS indicator is displayed.
    ResetPermissionPromptChip();
  } else {
    HandleConfirmation(permission_action);
  }
}

bool ChipController::IsBubbleShowing() {
  return chip_ != nullptr && (GetBubbleWidget() != nullptr);
}

bool ChipController::IsAnimating() const {
  return chip_->is_animating();
}

void ChipController::RestartTimersOnMouseHover() {
  ResetTimers();
  if (!permission_prompt_model_ || IsBubbleShowing() || IsAnimating()) {
    return;
  }

  if (is_confirmation_showing_) {
    collapse_timer_.Start(FROM_HERE, kConfirmationDisplayDuration, this,
                          &ChipController::CollapseConfirmation);
  } else if (chip_->is_fully_collapsed()) {
    // Quiet chip can collapse from a verbose state to an icon state. After it
    // is collapsed, it should be dismissed.
    StartDismissTimer();
  } else {
    StartCollapseTimer();
  }
}

void ChipController::OnWidgetDestroying(views::Widget* widget) {
  DCHECK_EQ(GetBubbleWidget(), widget);
  ResetTimers();

  disallowed_custom_cursors_scope_.RunAndReset();

  if (!prompt_decision_.has_value()) {
    observation_.Reset();
    widget->RemoveObserver(this);
    bubble_tracker_.SetView(nullptr);
    // This method will be called only if a user dismissed permission prompt
    // popup bubble. In case the user made decision, prompt_decision_ will not
    // be empty.
    OnPromptBubbleDismissed();
  }
}

void ChipController::OnWidgetDestroyed(views::Widget* widget) {
  widget->RemoveObserver(this);
  bubble_tracker_.SetView(nullptr);

  if (!prompt_decision_.has_value()) {
    return;
  }

  permissions::PermissionAction action = prompt_decision_.value();
  prompt_decision_.reset();

  if (!active_chip_permission_request_manager_.has_value() ||
      !active_chip_permission_request_manager_.value()->IsRequestInProgress()) {
    return;
  }

  switch (action) {
    case permissions::PermissionAction::GRANTED:
      active_chip_permission_request_manager_.value()->Accept();
      break;
    case permissions::PermissionAction::GRANTED_ONCE:
      active_chip_permission_request_manager_.value()->AcceptThisTime();
      break;
    case permissions::PermissionAction::DENIED:
      active_chip_permission_request_manager_.value()->Deny();
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
}

void ChipController::OnWidgetActivationChanged(views::Widget* widget,
                                               bool active) {
  // This logic prevents clickjacking. See https://crbug.com/1160485
  auto* prompt_bubble_widget = GetBubbleWidget();
  if (active && !parent_was_visible_when_activation_changed_) {
    // If the widget is active and the primary window wasn't active the last
    // time activation changed, we know that the window just came to the
    // foreground and trigger input protection.
    GetPromptBubbleView()->AsDialogDelegate()->TriggerInputProtection(
        /*force_early*/ true);
  }
  parent_was_visible_when_activation_changed_ =
      prompt_bubble_widget->GetPrimaryWindowWidget()->IsVisible();
}

bool ChipController::ShouldWaitForConfirmationToComplete() const {
  return is_confirmation_showing_ && collapse_timer_.IsRunning();
}

bool ChipController::ShouldWaitForLHSIndicatorToCollapse() const {
  return permission_dashboard_controller_->is_verbose();
}

void ChipController::InitializePermissionPrompt(
    base::WeakPtr<permissions::PermissionPrompt::Delegate> delegate,
    base::OnceCallback<void()> callback) {
  if (ShouldWaitForConfirmationToComplete()) {
    delay_prompt_timer_.Start(
        FROM_HERE, collapse_timer_.GetCurrentDelay(),
        base::BindOnce(&ChipController::InitializePermissionPrompt,
                       weak_factory_.GetWeakPtr(), delegate,
                       std::move(callback)));
    return;
  }

  ResetPermissionPromptChip();

  if (!delegate) {
    return;
  }

  // Here we just initialize the controller with the current request. We might
  // not yet want to display the chip, for example when a prompt bubble without
  // a request chip is shown --> only once a confirmation should be displayed,
  // the chip should become visible.
  chip_->SetVisible(false);
  if (permission_dashboard_view_ &&
      !permission_dashboard_view_->GetIndicatorChip()->GetVisible()) {
    permission_dashboard_view_->SetVisible(false);
  }
  permission_prompt_model_ =
      std::make_unique<PermissionPromptChipModel>(delegate);

  if (active_chip_permission_request_manager_.has_value()) {
    active_chip_permission_request_manager_.value()->RemoveObserver(this);
  }

  active_chip_permission_request_manager_ =
      permissions::PermissionRequestManager::FromWebContents(
          delegate->GetAssociatedWebContents());
  active_chip_permission_request_manager_.value()->AddObserver(this);
  observation_.Observe(chip_);
  std::move(callback).Run();
}

void ChipController::ShowPermissionUi(
    base::WeakPtr<permissions::PermissionPrompt::Delegate> delegate) {
  if (permission_dashboard_controller_ &&
      permission_dashboard_controller_->SuppressVerboseIndicator()) {
    delay_prompt_timer_.Start(
        FROM_HERE, kLHSIndicatorCollapseAnimationDuration,
        base::BindOnce(&ChipController::ShowPermissionPrompt,
                       weak_factory_.GetWeakPtr(), delegate));
    return;
  }

  if (ShouldWaitForConfirmationToComplete()) {
    delay_prompt_timer_.Start(
        FROM_HERE, collapse_timer_.GetCurrentDelay(),
        base::BindOnce(&ChipController::ShowPermissionPrompt,
                       weak_factory_.GetWeakPtr(), delegate));
    return;
  }

  if (!delegate) {
    return;
  }

  InitializePermissionPrompt(delegate);

  // HaTS surveys may be triggered while a quiet chip is displayed. If that
  // happens, the quiet chip should not collapse anymore, because otherwise a
  // user answering a survey would no longer be able to click on the chip. To
  // enable the PRM to handle this case, we pass a callback to stop the timers.
  if (delegate->ReasonForUsingQuietUi().has_value()) {
    delegate->SetHatsShownCallback(base::BindOnce(&ChipController::ResetTimers,
                                                  weak_factory_.GetWeakPtr()));
  }

  request_chip_shown_time_ = base::TimeTicks::Now();

  AnnouncePermissionRequestForAccessibility(
      permission_prompt_model_->GetAccessibilityChipText());

  chip_->SetVisible(true);

  if (permission_dashboard_view_) {
    permission_dashboard_view_->SetVisible(true);
    permission_dashboard_view_->UpdateDividerViewVisibility();
  }

  SyncChipWithModel();

  chip_->SetButtonController(std::make_unique<BubbleButtonController>(
      chip_.get(), this,
      std::make_unique<views::Button::DefaultButtonControllerDelegate>(
          chip_.get())));
  chip_->SetCallback(base::BindRepeating(
      &ChipController::OnRequestChipButtonPressed, weak_factory_.GetWeakPtr()));
  chip_->ResetAnimation();
  ObservePromptBubble();

  if (permission_prompt_model_->IsExpandAnimationAllowed()) {
    AnimateExpand();
  } else {
    StartDismissTimer();
  }
}

void ChipController::ShowPermissionChip(
    base::WeakPtr<permissions::PermissionPrompt::Delegate> delegate) {
  is_bubble_suppressed_ = true;
  ShowPermissionUi(delegate);
}

void ChipController::ShowPermissionPrompt(
    base::WeakPtr<permissions::PermissionPrompt::Delegate> delegate) {
  is_bubble_suppressed_ = false;
  ShowPermissionUi(delegate);
}

void ChipController::ClosePermissionPrompt() {
  views::Widget* const bubble_widget = GetBubbleWidget();
  // If a user decided on a prompt, the widget should not be `nullptr` as it is
  // 1:1 with the prompt.
  CHECK(bubble_widget);
  bubble_widget->Close();
}

void ChipController::PromptDecided(permissions::PermissionAction action) {
  prompt_decision_ = action;
  // Keep prompt decision inside ChipController and wait for `widget` to be
  // closed.
  ClosePermissionPrompt();
}

void ChipController::RemoveBubbleObserverAndResetTimersAndChipCallbacks() {
  views::Widget* const bubble_widget = GetBubbleWidget();
  if (bubble_widget) {
    disallowed_custom_cursors_scope_.RunAndReset();
    bubble_widget->RemoveObserver(this);
    bubble_widget->Close();
  }

  // Reset button click callback
  chip_->SetCallback(base::RepeatingCallback<void()>(base::DoNothing()));

  ResetTimers();
}

void ChipController::ResetPermissionPromptChip() {
  RemoveBubbleObserverAndResetTimersAndChipCallbacks();
  observation_.Reset();
  if (permission_prompt_model_) {
    // permission_request_manager_ is empty if the PermissionRequestManager
    // instance has destructed, which triggers the observer method
    // OnPermissionRequestManagerDestructed() implemented by this controller.
    if (active_chip_permission_request_manager_.has_value()) {
      active_chip_permission_request_manager_.value()->RemoveObserver(this);

      // When the user starts typing into the location bar we need to inform the
      // PermissionRequestManager to update the PermissionPrompt reference it
      // is holding. The typical update is for it to destruct the
      // PermissionPrompt instance and not to hold any PermissionPrompt instance
      // during the edit time.
      if (GetLocationBarView()->IsEditingOrEmpty() &&
          (active_chip_permission_request_manager_.value()
               ->IsRequestInProgress() &&
           (active_chip_permission_request_manager_.value()
                ->web_contents()
                ->GetVisibleURL() != GURL(chrome::kChromeUINewTabURL)))) {
        active_chip_permission_request_manager_.value()->RecreateView();
      }
      active_chip_permission_request_manager_.reset();
    }
    permission_prompt_model_.reset();
  }

  HideChip();
  is_confirmation_showing_ = false;
}

void ChipController::ResetPermissionRequestChip() {
  if (!is_confirmation_showing_) {
    ResetPermissionPromptChip();
  }
}

void ChipController::ShowPageInfoDialog() {
  content::WebContents* contents = GetLocationBarView()->GetWebContents();
  if (!contents)
    return;

  content::NavigationEntry* entry = contents->GetController().GetVisibleEntry();
  if (entry->IsInitialEntry())
    return;

  // Prevent chip from collapsing while prompt bubble is open.
  ResetTimers();

  auto initialized_callback =
      GetPageInfoDialogCreatedCallbackForTesting()
          ? std::move(GetPageInfoDialogCreatedCallbackForTesting())
          : base::DoNothing();

  views::BubbleDialogDelegateView* bubble =
      PageInfoBubbleView::CreatePageInfoBubble(
          chip_, gfx::Rect(), chip_->GetWidget()->GetNativeWindow(), contents,
          entry->GetVirtualURL(), std::move(initialized_callback),
          base::BindOnce(&ChipController::OnPageInfoBubbleClosed,
                         weak_factory_.GetWeakPtr()),
          /*allow_about_this_site=*/true);
  bubble->GetWidget()->Show();
  bubble_tracker_.SetView(bubble);
  permissions::PermissionUmaUtil::RecordPageInfoDialogAccessType(
      permissions::PageInfoDialogAccessType::CONFIRMATION_CHIP_CLICK);
}

void ChipController::OnPageInfoBubbleClosed(
    views::Widget::ClosedReason closed_reason,
    bool reload_prompt) {
  GetLocationBarView()->ResetConfirmationChipShownTime();
  HideChip();
}

void ChipController::CollapseConfirmation() {
  is_confirmation_showing_ = false;
  is_waiting_for_confirmation_collapse_ = true;
  GetLocationBarView()->ResetConfirmationChipShownTime();
  chip_->AnimateCollapse(GetAnimationDuration(base::Milliseconds(75)));
}

bool ChipController::should_expand_for_testing() {
  CHECK_IS_TEST();
  return permission_prompt_model_->ShouldExpand();
}

void ChipController::AnimateExpand() {
  chip_->ResetAnimation();
  chip_->AnimateExpand(GetAnimationDuration(base::Milliseconds(350)));
  chip_->SetVisible(true);
  if (permission_dashboard_view_) {
    permission_dashboard_view_->SetVisible(true);
  }
}

void ChipController::HandleConfirmation(
    permissions::PermissionAction user_decision) {
  DCHECK(permission_prompt_model_);
  permission_prompt_model_->UpdateWithUserDecision(user_decision);
  SyncChipWithModel();
  if (active_chip_permission_request_manager_.has_value() &&
      !active_chip_permission_request_manager_.value()
           ->has_pending_requests() &&
      permission_prompt_model_->CanDisplayConfirmation()) {
    is_confirmation_showing_ = true;

    if (chip_->GetVisible()) {
      chip_->AnimateToFit(GetAnimationDuration(base::Milliseconds(200)));
    } else {
      // No request chip was shown, always expand independently of what contents
      // were stored in the previous chip.
      AnimateExpand();
    }

    chip_->SetCallback(base::BindRepeating(&ChipController::ShowPageInfoDialog,
                                           weak_factory_.GetWeakPtr()));
    AnnouncePermissionRequestForAccessibility(
        permission_prompt_model_->GetAccessibilityChipText());

    if (!do_no_collapse_for_testing_) {
      collapse_timer_.Start(FROM_HERE, kConfirmationDisplayDuration, this,
                            &ChipController::CollapseConfirmation);
    }
  } else {
    ResetPermissionPromptChip();
  }
}

void ChipController::AnnouncePermissionRequestForAccessibility(
    const std::u16string& text) {
  chip_->GetViewAccessibility().AnnounceText(text);
}

void ChipController::CollapsePrompt(bool allow_restart) {
  if (allow_restart && chip_->IsMouseHovered()) {
    StartCollapseTimer();
  } else {
    permission_prompt_model_->UpdateAutoCollapsePromptChipState(true);
    SyncChipWithModel();

    if (!chip_->is_fully_collapsed()) {
      chip_->AnimateCollapse(GetAnimationDuration(base::Milliseconds(250)));
    }

    StartDismissTimer();
  }
}

void ChipController::OnExpandAnimationEnded() {
  if (is_confirmation_showing_ || IsBubbleShowing() ||
      !IsPermissionPromptChipVisible() || !permission_prompt_model_) {
    return;
  }

  if (permission_prompt_model_->ShouldBubbleStartOpen() &&
      !is_bubble_suppressed_) {
    OpenPermissionPromptBubble();
  } else {
    StartCollapseTimer();
  }
}

void ChipController::OnCollapseAnimationEnded() {
  if (is_waiting_for_confirmation_collapse_) {
    HideChip();
    is_waiting_for_confirmation_collapse_ = false;
  }
}

void ChipController::HideChip() {
  if (!chip_->GetVisible())
    return;

  chip_->SetVisible(false);
  if (permission_dashboard_view_) {
    // The request chip is gone, the divider view is no longer needed.
    permission_dashboard_view_->UpdateDividerViewVisibility();

    // Hide the parent view `permission_dashboard_view_` if no children are
    // visible.
    if (!permission_dashboard_view_->GetIndicatorChip()->GetVisible()) {
      permission_dashboard_view_->SetVisible(false);
    }
  }
  // When the chip visibility changed from visible -> hidden, the locationbar
  // layout should be updated.
  GetLocationBarView()->InvalidateLayout();
}

void ChipController::OpenPermissionPromptBubble() {
  DCHECK(!IsBubbleShowing());
  if (!permission_prompt_model_ || !permission_prompt_model_->GetDelegate() ||
      !location_bar_view_->GetWebContents()) {
    return;
  }

  Browser* browser =
      chrome::FindBrowserWithTab(location_bar_view_->GetWebContents());
  if (!browser) {
    DLOG(WARNING) << "Permission prompt suppressed because the WebContents is "
                     "not attached to any Browser window.";
    return;
  }

  disallowed_custom_cursors_scope_ =
      permission_prompt_model_->GetDelegate()
          ->GetAssociatedWebContents()
          ->CreateDisallowCustomCursorScope(/*max_dimension_dips=*/0);

  // Prevent chip from collapsing while prompt bubble is open.
  ResetTimers();

  if (permission_prompt_model_->GetPromptStyle() ==
      PermissionPromptStyle::kChip) {
    // Loud prompt bubble.
    raw_ptr<PermissionPromptBubbleBaseView> prompt_bubble =
        CreatePermissionPromptBubbleView(
            browser, permission_prompt_model_->GetDelegate(),
            request_chip_shown_time_, PermissionPromptStyle::kChip);
    bubble_tracker_.SetView(prompt_bubble);
    prompt_bubble->Show();
  } else if (permission_prompt_model_->GetPromptStyle() ==
             PermissionPromptStyle::kQuietChip) {
    // Quiet prompt bubble.
    LocationBarView* lbv = GetLocationBarView();
    content::WebContents* web_contents = lbv->GetContentSettingWebContents();

    if (web_contents) {
      std::unique_ptr<ContentSettingQuietRequestBubbleModel>
          content_setting_bubble_model =
              std::make_unique<ContentSettingQuietRequestBubbleModel>(
                  lbv->GetContentSettingBubbleModelDelegate(), web_contents);
      ContentSettingBubbleContents* quiet_request_bubble =
          new ContentSettingBubbleContents(
              std::move(content_setting_bubble_model), web_contents, lbv,
              views::BubbleBorder::TOP_LEFT);
      quiet_request_bubble->set_close_on_deactivate(false);
      views::Widget* bubble_widget =
          views::BubbleDialogDelegateView::CreateBubble(quiet_request_bubble);
      quiet_request_bubble->set_close_on_deactivate(false);
      bubble_tracker_.SetView(quiet_request_bubble);
      bubble_widget->Show();
    }
  }

  // It is possible that a Chip got reset while the permission prompt bubble was
  // displayed.
  if (permission_prompt_model_ && IsBubbleShowing()) {
    ObservePromptBubble();
    permission_prompt_model_->GetDelegate()->SetPromptShown();
  }
}

void ChipController::ClosePermissionPromptBubbleWithReason(
    views::Widget::ClosedReason reason) {
  DCHECK(IsBubbleShowing());
  GetBubbleWidget()->CloseWithReason(reason);
}

void ChipController::RecordRequestChipButtonPressed(const char* recordKey) {
  base::UmaHistogramMediumTimes(
      recordKey, base::TimeTicks::Now() - request_chip_shown_time_);
}

void ChipController::ObservePromptBubble() {
  views::Widget* prompt_bubble_widget = GetBubbleWidget();
  if (prompt_bubble_widget) {
    parent_was_visible_when_activation_changed_ =
        prompt_bubble_widget->GetPrimaryWindowWidget()->IsVisible();
    prompt_bubble_widget->AddObserver(this);
  }
}

void ChipController::OnPromptBubbleDismissed() {
  DCHECK(permission_prompt_model_);
  if (!permission_prompt_model_)
    return;

  if (permission_prompt_model_->GetDelegate()) {
    permission_prompt_model_->GetDelegate()->SetDismissOnTabClose();
    // If the permission prompt bubble is closed, we count it as "Dismissed",
    // hence it should record the time when the bubble is closed.
    permission_prompt_model_->GetDelegate()->SetDecisionTime();
    // If a permission popup bubble is closed/dismissed, a permission request
    // should be dismissed as well.
    permission_prompt_model_->GetDelegate()->Dismiss();
  }
}

void ChipController::OnPromptExpired() {
  AnnouncePermissionRequestForAccessibility(l10n_util::GetStringUTF16(
      IDS_PERMISSIONS_EXPIRED_SCREENREADER_ANNOUNCEMENT));
  if (active_chip_permission_request_manager_.has_value()) {
    active_chip_permission_request_manager_.value()->RemoveObserver(this);
    active_chip_permission_request_manager_.reset();
  }

  // Because `OnPromptExpired` is called async, make sure that there is an
  // existing permission request before resolving it as `Ignore`.
  if (permission_prompt_model_ && permission_prompt_model_->GetDelegate() &&
      !permission_prompt_model_->GetDelegate()->Requests().empty()) {
    permission_prompt_model_->GetDelegate()->Ignore();
  }

  ResetPermissionPromptChip();
}

void ChipController::OnRequestChipButtonPressed() {
  if (permission_prompt_model_ &&
      (!IsBubbleShowing() ||
       permission_prompt_model_->ShouldBubbleStartOpen())) {
    // Only record if its the first interaction.
    if (permission_prompt_model_->GetPromptStyle() ==
        PermissionPromptStyle::kChip) {
      RecordRequestChipButtonPressed("Permissions.Chip.TimeToInteraction");
    } else if (permission_prompt_model_->GetPromptStyle() ==
               PermissionPromptStyle::kQuietChip) {
      RecordRequestChipButtonPressed("Permissions.QuietChip.TimeToInteraction");
    }
  }

  if (IsBubbleShowing()) {
    // A mouse click on chip while a permission prompt is open should dismiss
    // the prompt and collapse the chip
    ClosePermissionPromptBubbleWithReason(
        views::Widget::ClosedReason::kCloseButtonClicked);
  } else {
    OpenPermissionPromptBubble();
  }
}

void ChipController::OnChipVisibilityChanged(bool is_visible) {
  auto* prompt_bubble = GetBubbleWidget();
  if (!chip_->GetVisible() && prompt_bubble) {
    // In case if the prompt bubble isn't closed on focus loss, manually close
    // it when chip is hidden.
    prompt_bubble->Close();
  }
}

void ChipController::SyncChipWithModel() {
  PermissionPromptChipModel* const model = permission_prompt_model_.get();
  chip_->SetChipIcon(model->GetIcon());
  chip_->SetTheme(model->GetChipTheme());
  chip_->SetMessage(model->GetChipText());
  chip_->SetUserDecision(model->GetUserDecision());
  chip_->SetPermissionPromptStyle(model->GetPromptStyle());
  chip_->SetBlockedIconShowing(model->ShouldDisplayBlockedIcon());
}

void ChipController::StartCollapseTimer() {
  collapse_timer_.Start(FROM_HERE, kCollapseDelay,
                        base::BindOnce(&ChipController::CollapsePrompt,
                                       weak_factory_.GetWeakPtr(),
                                       /*allow_restart=*/true));
}

void ChipController::StartDismissTimer() {
  if (!permission_prompt_model_)
    return;

  dismiss_timer_.Start(FROM_HERE,
                       permission_prompt_model_->ShouldExpand()
                           ? kDismissDelay
                           : kDismissDelayForAbusiveOrigins,
                       this, &ChipController::OnPromptExpired);
}

void ChipController::ResetTimers() {
  collapse_timer_.AbandonAndStop();
  dismiss_timer_.AbandonAndStop();
  delay_prompt_timer_.AbandonAndStop();
}

views::Widget* ChipController::GetBubbleWidget() {
  // We can't call GetPromptBubbleView() here, because the bubble_tracker may
  // hold objects that aren't of typ `PermissionPromptBubbleBaseView`.
  return bubble_tracker_.view() ? bubble_tracker_.view()->GetWidget() : nullptr;
}

PermissionPromptBubbleBaseView* ChipController::GetPromptBubbleView() {
  // The tracked bubble view is a `PermissionPromptBubbleBaseView` only when
  // `kChip` is used.
  CHECK_EQ(permission_prompt_model_->GetPromptStyle(),
           PermissionPromptStyle::kChip);
  auto* view = bubble_tracker_.view();
  return view ? static_cast<PermissionPromptBubbleBaseView*>(view) : nullptr;
}
