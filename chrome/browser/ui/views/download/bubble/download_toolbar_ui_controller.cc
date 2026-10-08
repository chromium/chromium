// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/download/bubble/download_toolbar_ui_controller.h"

#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

DEFINE_USER_DATA(DownloadToolbarUIController);

#include <optional>
#include <string>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/download/bubble/download_bubble_ui_controller.h"
#include "chrome/browser/download/bubble/download_display_controller.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/download/bubble/download_bubble_contents_view.h"
#include "chrome/browser/ui/views/download/bubble/download_bubble_started_animation_views.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/download_button.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/core/browser/suggestions/suggestion_hiding_reason.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/safe_browsing/core/common/safe_browsing_policy_handler.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/browser_thread.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/event_monitor.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/fullscreen_util_mac.h"
#endif

namespace {

// Close the partial bubble after 5 seconds if the user doesn't interact with
// it.
constexpr base::TimeDelta kAutoClosePartialViewDelay = base::Seconds(5);

PinnedToolbarActions* GetPinnedToolbarActions(BrowserView* browser_view) {
  auto* toolbar_button_provider = browser_view->toolbar_button_provider();
  return toolbar_button_provider
             ? toolbar_button_provider->GetPinnedToolbarActions()
             : nullptr;
}

DownloadButton* GetDownloadsButton(BrowserView* browser_view) {
  auto* container = GetPinnedToolbarActions(browser_view);
  return container ? container->GetDownloadButton() : nullptr;
}

gfx::Insets GetPrimaryViewMargin() {
  return gfx::Insets::VH(ChromeLayoutProvider::Get()->GetDistanceMetric(
                             views::DISTANCE_RELATED_CONTROL_VERTICAL),
                         0);
}

gfx::Insets GetSecurityViewMargin() {
  return gfx::Insets::VH(ChromeLayoutProvider::Get()->GetDistanceMetric(
                             views::DISTANCE_RELATED_CONTROL_VERTICAL),
                         0);
}

}  // namespace

// static
DownloadToolbarUIController* DownloadToolbarUIController::From(
    BrowserWindowInterface* browser) {
  return Get(browser->GetUnownedUserDataHost());
}

DownloadToolbarUIController::DownloadToolbarUIController(
    BrowserView* browser_view)
    : browser_view_(browser_view),
      auto_close_bubble_timer_(
          FROM_HERE,
          kAutoClosePartialViewDelay,
          base::BindRepeating(
              &DownloadToolbarUIController::AutoClosePartialView,
              base::Unretained(this))),
      scoped_unowned_user_data_(
          browser_view->browser()->GetUnownedUserDataHost(),
          *this) {
  BrowserWindowInterface* const browser = browser_view_->browser();
  action_item_ = actions::ActionManager::Get().FindAction(
      kActionShowDownloads, BrowserActions::From(browser)->root_action_item());
  CHECK(action_item_);
  tooltip_texts_[0] = l10n_util::GetStringUTF16(IDS_TOOLTIP_DOWNLOAD_ICON);
  action_item_->SetTooltipText(tooltip_texts_.at(0));

  bubble_controller_ = std::make_unique<DownloadBubbleUIController>(browser);

  controller_ = std::make_unique<DownloadDisplayController>(
      this, browser_view_->browser(), bubble_controller_.get());

  browser_collection_observation_.Observe(
      ProfileBrowserCollection::GetForProfile(browser->GetProfile()));
}

DownloadToolbarUIController::~DownloadToolbarUIController() = default;

void DownloadToolbarUIController::TearDownPreBrowserWindowDestruction() {
  immersive_revealed_lock_.reset();
  // DownloadDisplayController depends on BrowserView.
  controller_.reset();
  browser_view_ = nullptr;
}

void DownloadToolbarUIController::Show() {
  auto* container = GetPinnedToolbarActions(browser_view_);
  if (!container) {
    return;
  }
  container->ShowActionEphemerallyInToolbar(kActionShowDownloads, true);
}

void DownloadToolbarUIController::Hide() {
  HideDetails();
  auto* container = GetPinnedToolbarActions(browser_view_);
  if (!container) {
    return;
  }
  container->ShowActionEphemerallyInToolbar(kActionShowDownloads, false);
}

bool DownloadToolbarUIController::IsShowing() const {
  auto* button = GetDownloadsButton(browser_view_);
  return button && button->IsShowing();
}

void DownloadToolbarUIController::Enable() {
  action_item_->SetEnabled(true);
}

void DownloadToolbarUIController::Disable() {
  action_item_->SetEnabled(false);
}

void DownloadToolbarUIController::UpdateDownloadIcon(
    const IconUpdateInfo& updates) {
  // Whether to update the icon after processing any changes.
  bool update_icon = false;

  if (updates.show_animation && show_download_started_animation_) {
    has_pending_download_started_animation_ = true;
    if (auto* container = GetPinnedToolbarActions(browser_view_)) {
      container->PostOrQueueActionAfterAnimation(base::BindOnce(
          &DownloadToolbarUIController::ShowPendingDownloadStartedAnimation,
          weak_factory_.GetWeakPtr()));
    }
  }
  if (updates.new_state && *updates.new_state != state_) {
    update_icon = true;
    state_ = *updates.new_state;
  }
  if (updates.new_active && *updates.new_active != active_) {
    update_icon = true;
    active_ = *updates.new_active;
  }

  if (updates.new_progress) {
    const ProgressInfo& new_progress = *updates.new_progress;
    // Only change the icon if the download count or progress certainty have
    // changed. If only the percentage changed, the icon itself doesn't
    // necessarily need to change; the ring change is captured by possibly
    // scheduling a paint.
    if (!new_progress.FieldsEqualExceptPercentage(progress_info_)) {
      update_icon = true;
    }

    // Schedule a paint when we hit 0 downloads, even if this button is
    // dormant. This will clear the ring. This is needed to avoid a ring being
    // left over on a dormant button when going from >0 to 0 downloads.
    if (new_progress.download_count == 0 && progress_info_.download_count > 0) {
      redraw_progress_soon_ = true;
    }

    if (!is_dormant_ && new_progress.progress_percentage !=
                            progress_info_.progress_percentage) {
      redraw_progress_soon_ = true;
    }
    progress_info_ = new_progress;
  }
  // We need to redraw the ring constantly while the scanning animation is
  // running.
  if (ShouldShowScanningAnimation()) {
    redraw_progress_soon_ = true;
  }

  if (redraw_progress_soon_ || update_icon) {
    UpdateIcon();
    redraw_progress_soon_ = false;
  }
}

void DownloadToolbarUIController::AnnounceAccessibleAlertNow(
    const std::u16string& alert_text) {
  if (auto* button = GetDownloadsButton(browser_view_)) {
    button->AnnounceAccessibleAlert(alert_text);
  }
}

bool DownloadToolbarUIController::IsFullscreenWithParentViewHidden() const {
#if BUILDFLAG(IS_MAC)
  if (fullscreen_utils::IsInContentFullscreen(browser_view_->browser())) {
    return true;
  }
#endif

  // If immersive fullscreen, check if top chrome is visible.
  auto* const controller =
      ImmersiveModeController::From(browser_view_->browser());
  if (browser_view_ && browser_view_->GetLocationBarView() &&
      controller->IsEnabled()) {
    return !controller->IsRevealed();
  }

  // Handle the remaining fullscreen case.
  return browser_view_->browser()->GetWindow() &&
         browser_view_->browser()->GetWindow()->IsFullscreen() &&
         !browser_view_->IsToolbarVisible();
}

bool DownloadToolbarUIController::ShouldShowExclusiveAccessBubble() const {
  if (!IsFullscreenWithParentViewHidden()) {
    return false;
  }
  if (!browser_view_) {
    return false;
  }
#if BUILDFLAG(IS_MAC)
  // In content fullscreen, we do not show the download bubble and the toolbar
  // is not visible. Therefore, we must show the ExclusiveAccessBubble notice.
  if (fullscreen_utils::IsInContentFullscreen(browser_view_->browser())) {
    return true;
  }
#endif
  return !ImmersiveModeController::From(browser_view_->browser())
              ->IsEnabled() &&
         browser_view_->GetExclusiveAccessContext()->CanUserExitFullscreen();
}

void DownloadToolbarUIController::OpenSecuritySubpage(
    const offline_items_collection::ContentId& id) {
  OpenSecurityDialog(id);
}

// This function shows the partial view. If the main view is already showing,
// we do not show the partial view. If the partial view is already showing,
// there is nothing to do here, the controller should update the partial view.
void DownloadToolbarUIController::ShowDetails() {
  if (bubble_delegate_ || pending_bubble_) {
    return;
  }
  base::TimeDelta delay = GetAutoCloseDelay();
  if (use_auto_close_bubble_timer_ && !delay.is_max()) {
    auto_close_bubble_timer_.Start(
        FROM_HERE, delay,
        base::BindRepeating(&DownloadToolbarUIController::AutoClosePartialView,
                            base::Unretained(this)));
  }
  ShowBubble(DownloadBubbleMode::kPartial);
}

void DownloadToolbarUIController::HideDetails() {
  if (IsShowingDetails()) {
    CloseDialog(views::Widget::ClosedReason::kUnspecified);
  }
}

bool DownloadToolbarUIController::IsShowingDetails() const {
  return bubble_delegate_ != nullptr &&
         bubble_delegate_->GetWidget()->IsVisible();
}

void DownloadToolbarUIController::OnOfflineItemsInitialized() {
  // Only update models if the complete view is showing. Offline items
  // represent past downloads from history and are never displayed in the
  // partial view (which only shows new un-actioned downloads). Furthermore,
  // calling GetPrimaryViewModels() for the partial view triggers the 15-second
  // rate-limiting in GetPartialView(), which returns empty models and causes
  // the partial view bubble to be closed prematurely.
  if (bubble_contents_ && primary_view_mode_ == DownloadBubbleMode::kComplete) {
    bubble_contents_->info().UpdateModels(GetPrimaryViewModels());
  }
}

void DownloadToolbarUIController::UpdateIcon() {
  auto* button = GetDownloadsButton(browser_view_);
  if (!button) {
    return;
  }

  // Determine how the progress ring should be drawn.
  ActionItemProgressRingStatus ring_status;
  if (state_ == IconState::kComplete || progress_info_.download_count == 0) {
    // Do not show the progress ring when there is no in progress download.
    ring_status = ActionItemProgressRingStatus::kIdle;
  } else if (is_dormant_) {
    ring_status = ActionItemProgressRingStatus::kDormant;
  } else if (ShouldShowScanningAnimation()) {
    ring_status = ActionItemProgressRingStatus::kScanning;
  } else {
    ring_status = ActionItemProgressRingStatus::kDownloading;
  }

  int progress_download_count = progress_info_.download_count;
  bool is_disabled = !action_item_->GetEnabled() || is_dormant_;
  bool is_active = active_ == IconActive::kActive;
  SkColor badge_text_color =
      is_disabled
          ? button->GetColor(kColorToolbarButtonIconInactive)
          : button->GetColor(is_active ? kColorDownloadToolbarButtonActive
                                       : kColorDownloadToolbarButtonInactive);
  SkColor badge_background_color = button->GetColor(kColorToolbar);

  const gfx::VectorIcon* new_icon;
  // An active icon is indicated by the color and the presence of an underline
  // under the icon button.
  bool is_icon_active = !is_dormant_ && is_active;
  SkColor icon_color = browser_view_->GetColorProvider()->GetColor(
      is_icon_active ? kColorDownloadToolbarButtonActive
                     : kColorDownloadToolbarButtonInactive);
  bool is_touch_mode = ui::TouchUiController::Get()->touch_ui();
  if (state_ == IconState::kProgress || state_ == IconState::kDeepScanning ||
      state_ == IconState::kContentCheckPending) {
    new_icon = is_touch_mode ? &(features::IsRoundedIconsEnabled()
                                     ? kArrowDownwardAltIcon
                                     : kDownloadInProgressTouchOldIcon)
                             : &(features::IsRoundedIconsEnabled()
                                     ? kArrowDownwardAltIcon
                                     : kDownloadInProgressChromeRefreshOldIcon);
  } else {
    new_icon = is_touch_mode
                   ? &(features::IsRoundedIconsEnabled()
                           ? kDownloadIcon
                           : kDownloadToolbarButtonTouchOldIcon)
                   : &(features::IsRoundedIconsEnabled()
                           ? kDownloadIcon
                           : kDownloadToolbarButtonChromeRefreshOldIcon);
  }
  action_item_->SetProperty(kActionItemUnderlineIndicatorKey, is_icon_active);

  action_item_->SetImage(ui::ImageModel::FromVectorIcon(*new_icon, icon_color));

  // Update the toolbar button's tooltip.
  std::u16string& tooltip_for_progress_count =
      tooltip_texts_[progress_download_count];
  if (tooltip_for_progress_count.empty()) {
    // We already initialized the text for 0 downloads in the constructor.
    CHECK_GT(progress_download_count, 0);
    // "1 download in progress" or "N downloads in progress".
    tooltip_for_progress_count = l10n_util::GetPluralStringFUTF16(
        IDS_DOWNLOAD_BUBBLE_TOOLTIP_IN_PROGRESS_COUNT, progress_download_count);
  }
  if (progress_download_count == 0 && is_icon_active) {
    // If there are 0 in-progress downloads but the icon is still active, use
    // the tooltip text to indicate to a11y users (along with the visual
    // indications of the icon color and underline) that there is a new
    // "unactioned" complete download.
    action_item_->SetTooltipText(
        l10n_util::GetStringUTF16(IDS_TOOLTIP_DOWNLOAD_ICON_NEW_DOWNLOAD));
  } else {
    action_item_->SetTooltipText(tooltip_for_progress_count);
  }

  redraw_progress_soon_ = false;

  // Update the ring before the badge so that, when first installed, the badge
  // is stacked above the ring.
  button->UpdateProgressRing(ring_status, progress_info_.progress_percentage);
  button->UpdateBadge(is_active, progress_download_count, badge_text_color,
                      badge_background_color);
}

void DownloadToolbarUIController::OpenPrimaryDialog() {
  if (!bubble_delegate_) {
    return;
  }
  bubble_contents_->ShowPrimaryPage(std::nullopt);
  bubble_delegate_->SetButtons(
      static_cast<int>(ui::mojom::DialogButton::kNone));
  bubble_delegate_->SetDefaultButton(
      static_cast<int>(ui::mojom::DialogButton::kNone));
  bubble_delegate_->set_margins(GetPrimaryViewMargin());
}

void DownloadToolbarUIController::OpenSecurityDialog(
    const ContentId& content_id) {
  if (!bubble_delegate_) {
    ShowBubble(DownloadBubbleMode::kComplete, content_id);
    return;
  }
  ShowSecurityPage(content_id);
}

void DownloadToolbarUIController::ShowSecurityPage(
    const ContentId& content_id) {
  bubble_contents_->ShowSecurityPage(content_id);
  bubble_delegate_->set_margins(GetSecurityViewMargin());
}

void DownloadToolbarUIController::CloseDialog(
    views::Widget::ClosedReason reason) {
  if (bubble_delegate_) {
    bubble_delegate_->GetWidget()->CloseWithReason(reason);
  }
}

void DownloadToolbarUIController::OnSecurityDialogButtonPress(
    const DownloadUIModel& model,
    DownloadCommands::Command command) {
  if (model.GetDangerType() ==
          download::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT &&
      command == DownloadCommands::DISCARD) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&DownloadToolbarUIController::ShowIphPromo,
                                  weak_factory_.GetWeakPtr()));
  }
}

void DownloadToolbarUIController::OnDialogInteracted() {
  DeactivateAutoClose();
}

std::unique_ptr<DownloadBubbleNavigationHandler::CloseOnDeactivatePin>
DownloadToolbarUIController::PreventDialogCloseOnDeactivate() {
  if (!bubble_delegate_) {
    return nullptr;
  }
  return bubble_delegate_->PreventCloseOnDeactivate();
}

base::WeakPtr<DownloadBubbleNavigationHandler>
DownloadToolbarUIController::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void DownloadToolbarUIController::OnBrowserActivated(
    BrowserWindowInterface* browser) {
  UpdateIconDormant();
}

void DownloadToolbarUIController::OnBrowserDeactivated(
    BrowserWindowInterface* browser) {
  UpdateIconDormant();
}

void DownloadToolbarUIController::DeactivateAutoClose() {
  auto_close_bubble_timer_.Stop();
}

void DownloadToolbarUIController::InvokeUI() {
  if (!bubble_delegate_ && !bubble_controller_->GetMainView().empty()) {
    button_click_time_ = base::TimeTicks::Now();
    ShowBubble(DownloadBubbleMode::kComplete);
  } else {
    chrome::ShowDownloads(browser_view_->browser());
  }
  controller_->OnButtonPressed();
}

void DownloadToolbarUIController::ShowPendingDownloadStartedAnimation() {
  if (!has_pending_download_started_animation_) {
    return;
  }
  CHECK(show_download_started_animation_);
  has_pending_download_started_animation_ = false;
  if (!gfx::Animation::ShouldRenderRichAnimation()) {
    return;
  }
  tabs::TabInterface* const tab =
      browser_view_->browser()->GetActiveTabInterface();
  content::WebContents* const web_contents = tab ? tab->GetContents() : nullptr;
  if (!web_contents ||
      !platform_util::IsVisible(web_contents->GetNativeView())) {
    return;
  }
  // Animation cleans itself up after it's done.
  if (auto* button = GetDownloadsButton(browser_view_)) {
    new DownloadBubbleStartedAnimationViews(
        web_contents, button->GetBoundsInScreen(),
        button->GetColor(kColorDownloadToolbarButtonAnimationForeground),
        button->GetColor(kColorDownloadToolbarButtonAnimationBackground));
  }
}

bool DownloadToolbarUIController::IsProgressRingInDownloadingStateForTesting() {
  auto* button = GetDownloadsButton(browser_view_);
  return button && button->GetProgressRingStatusForTesting() ==
                       ActionItemProgressRingStatus::kDownloading;
}

bool DownloadToolbarUIController::IsProgressRingInDormantStateForTesting() {
  auto* button = GetDownloadsButton(browser_view_);
  return button && button->GetProgressRingStatusForTesting() ==
                       ActionItemProgressRingStatus::kDormant;
}

views::ImageView* DownloadToolbarUIController::GetImageBadgeForTesting() {
  auto* button = GetDownloadsButton(browser_view_);
  return button ? button->GetImageBadgeForTesting() : nullptr;
}

DownloadToolbarUIController::BubbleCloser::BubbleCloser(
    views::BubbleAnchor anchor,
    views::Widget* bubble_widget,
    base::WeakPtr<DownloadDisplay> download_display)
    : download_display_(download_display) {
  CHECK(!anchor.IsNull());
  CHECK(bubble_widget);
  bubble_widget_observation_.Observe(bubble_widget);
  views::Widget* anchor_widget;
  if (anchor.GetIfView()) {
    anchor_widget = anchor.GetIfView()->GetWidget();
  } else {
    CHECK(anchor.GetIfElement());
    anchor_widget = views::Widget::GetWidgetForNativeView(
        anchor.GetIfElement()->GetNativeView());
  }
  if (anchor_widget && anchor_widget->GetTopLevelWidget() &&
      anchor_widget->GetTopLevelWidget()->GetNativeWindow()) {
    event_monitor_ = views::EventMonitor::CreateWindowMonitor(
        this, anchor_widget->GetTopLevelWidget()->GetNativeWindow(),
        {ui::EventType::kMousePressed, ui::EventType::kKeyPressed,
         ui::EventType::kTouchPressed});
  }
}

DownloadToolbarUIController::BubbleCloser::~BubbleCloser() = default;

void DownloadToolbarUIController::BubbleCloser::OnEvent(
    const ui::Event& event) {
  // If the bubble widget is active, we should do nothing and defer to the
  // close-on-deactivate behavior from BubbleDialogDelegate which is in effect
  // when the bubble is active.
  if (bubble_widget_observation_.IsObserving() &&
      bubble_widget_observation_.GetSource()->IsActive()) {
    return;
  }
  CHECK(event_monitor_);
  if (event.IsKeyEvent() && event.AsKeyEvent()->key_code() != ui::VKEY_ESCAPE) {
    return;
  }

  if (download_display_) {
    download_display_->HideDetails();
  }
  // `this` will be deleted.
}

void DownloadToolbarUIController::BubbleCloser::OnWidgetDestroyed(
    views::Widget* widget) {
  if (bubble_widget_observation_.IsObservingSource(widget)) {
    bubble_widget_observation_.Reset();
  }
}

void DownloadToolbarUIController::ShowBubble(
    DownloadBubbleMode mode,
    std::optional<ContentId> content_id) {
  // Requests for a complete (i.e. non-partial) window override requests for a
  // partial window.
  if (!pending_bubble_ || mode == DownloadBubbleMode::kComplete) {
    primary_view_mode_ = mode;
  }
  // Requests that show some security content override requests that don't.
  if (!pending_bubble_ || content_id.has_value()) {
    pending_security_content_ = content_id;
  }
  if (pending_bubble_) {
    return;
  }

  auto* container = GetPinnedToolbarActions(browser_view_);
  if (!container) {
    return;
  }

  pending_bubble_ = true;
  container->GetBubbleAnchorAsync(
      kActionShowDownloads,
      base::BindOnce(&DownloadToolbarUIController::OnBubbleAnchorAssembled,
                     weak_factory_.GetWeakPtr()));
}

void DownloadToolbarUIController::OnBubbleAnchorAssembled(
    base::expected<views::BubbleAnchor, GetAnchorFailureReason> anchor) {
  pending_bubble_ = false;
  // The bubble should not show if the button doesn't exist since it would have
  // nothing to anchor to.
  if (!anchor.has_value()) {
    return;
  }
  std::vector<DownloadUIModel::DownloadUIModelPtr> primary_view_models =
      GetPrimaryViewModels();
  if (primary_view_models.empty()) {
    return;
  }

  // If we are in immersive fullscreen, reveal the toolbar to show the bubble.
  if (browser_view_) {
    auto* const controller =
        ImmersiveModeController::From(browser_view_->browser());
    if (controller) {
      immersive_revealed_lock_ = controller->GetRevealedLock(
          ImmersiveModeController::ANIMATE_REVEAL_YES);
    }
  }
  auto bubble_delegate = std::make_unique<views::BubbleDialogDelegate>(
      anchor.value(), views::BubbleBorder::TOP_RIGHT,
      views::BubbleBorder::DIALOG_SHADOW,
      /*autosize=*/true);
  bubble_delegate->SetOwnedByWidget(
      views::WidgetDelegate::OwnedByWidgetPassKey());
  bubble_delegate->SetTitle(
      l10n_util::GetStringUTF16(IDS_DOWNLOAD_BUBBLE_HEADER_LABEL));
  bubble_delegate->SetShowTitle(false);
  bubble_delegate->set_internal_name(kBubbleName);
  bubble_delegate->SetShowCloseButton(false);
  bubble_delegate->SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  bubble_delegate->SetDefaultButton(
      static_cast<int>(ui::mojom::DialogButton::kNone));
  bubble_delegate->RegisterWindowClosingCallback(
      base::BindOnce(&DownloadToolbarUIController::OnBubbleClosing,
                     weak_factory_.GetWeakPtr()));
  auto bubble_contents = std::make_unique<DownloadBubbleContentsView>(
      browser_view_->browser(), bubble_controller_->GetWeakPtr(), GetWeakPtr(),
      primary_view_mode_,
      std::make_unique<DownloadBubbleContentsViewInfo>(
          std::move(primary_view_models)),
      bubble_delegate.get());
  bubble_contents_ = bubble_contents.get();
  bubble_delegate->SetContentsView(std::move(bubble_contents));
  // The contents view displays the primary view by default.
  bubble_delegate->set_margins(GetPrimaryViewMargin());
  bubble_delegate->SetEnableArrowKeyTraversal(true);
  bubble_delegate_ = bubble_delegate.get();
  views::Widget* bubble_widget =
      views::BubbleDialogDelegate::CreateBubbleDeprecated(
          std::move(bubble_delegate),
          views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET);
  CHECK(bubble_widget);

  if (primary_view_mode_ != DownloadBubbleMode::kPartial &&
      !button_click_time_.is_null()) {
    // If the main view was shown after clicking on the toolbar button,
    // record the time from click to shown. (The main view can be shown without
    // clicking the toolbar button, e.g. from clicking on a notification.)
    bubble_widget->GetCompositor()
        ->RequestSuccessfulPresentationTimeForNextFrame(base::BindOnce(
            [](base::TimeTicks click_time,
               const viz::FrameTimingDetails& frame_timing_details) {
              base::TimeTicks presentation_time =
                  frame_timing_details.presentation_feedback.timestamp;
              UmaHistogramTimes(
                  "Download.Bubble.ToolbarButtonClickToFullViewShownLatency",
                  presentation_time - click_time);
            },
            button_click_time_));
    // Reset click time.
    button_click_time_ = base::TimeTicks();
  }

  CloseAutofillPopup();
  if (ShouldShowBubbleAsInactive()) {
    CHECK(anchor.has_value());
    bubble_widget->ShowInactive();
    bubble_widget->GetRootView()->GetViewAccessibility().AnnounceText(
        l10n_util::GetStringUTF16(IDS_SHOW_BUBBLE_INACTIVE_DESCRIPTION));
  } else {
    bubble_widget->Show();
  }
  bubble_closer_ = std::make_unique<BubbleCloser>(anchor.value(), bubble_widget,
                                                  weak_factory_.GetWeakPtr());

  action_item_->SetIsShowingBubble(true);

  // For IPH bubble. The IPH should show when the partial view is closed, either
  // manually or automatically.
  if (primary_view_mode_ == DownloadBubbleMode::kPartial) {
    bubble_delegate_->SetCloseCallback(
        base::BindOnce(&DownloadToolbarUIController::OnPartialViewClosed,
                       weak_factory_.GetWeakPtr()));
  }

  UpdateIconDormant();

  if (pending_security_content_.has_value()) {
    ShowSecurityPage(*pending_security_content_);
  }
}

void DownloadToolbarUIController::OnBubbleClosing() {
  immersive_revealed_lock_.reset();
  bubble_delegate_ = nullptr;
  bubble_contents_ = nullptr;
  bubble_closer_.reset();
  UpdateIconDormant();
  action_item_->SetIsShowingBubble(false);
}

void DownloadToolbarUIController::OnPartialViewClosed() {
  // We use PostTask to avoid calling the FocusAndActivateWindow
  // function reentrantly from ui/wm/core/focus_controller.cc.
  // We make sure each call to the FocusAndActivateWindow method
  // finishes before the next.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&DownloadToolbarUIController::ShowIphPromo,
                                weak_factory_.GetWeakPtr()));
}

void DownloadToolbarUIController::ShowIphPromo() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (auto* button = GetDownloadsButton(browser_view_)) {
    button->SetElementIdentifier(kToolbarDownloadButtonElementId);
  }
  Profile* profile = browser_view_->GetProfile();
  // Don't show IPH Promo if safe browsing level is set by policy.
  if (safe_browsing::SafeBrowsingPolicyHandler::
          IsSafeBrowsingProtectionLevelSetByPolicy(profile->GetPrefs())) {
    return;
  }
  if (safe_browsing::GetSafeBrowsingState(*profile->GetPrefs()) ==
          safe_browsing::SafeBrowsingState::STANDARD_PROTECTION &&
      !profile->IsOffTheRecord()) {
    BrowserUserEducationInterface::From(browser_view_->browser())
        ->MaybeShowFeaturePromo(
            feature_engagement::kIPHDownloadEsbPromoFeature);
  }
#endif
}

void DownloadToolbarUIController::AutoClosePartialView() {
  // Nothing to do if the bubble is not open.
  if (!bubble_contents_) {
    return;
  }
  // Don't close the security page.
  if (bubble_contents_->VisiblePage() ==
      DownloadBubbleContentsView::Page::kSecurity) {
    return;
  }
  if (primary_view_mode_ != DownloadBubbleMode::kPartial ||
      !use_auto_close_bubble_timer_) {
    return;
  }
  // Don't close if the user is hovering over the bubble.
  if (bubble_contents_->IsMouseHovered()) {
    return;
  }
  HideDetails();
}

base::TimeDelta DownloadToolbarUIController::GetAutoCloseDelay() const {
  // If accessibility mode is enabled (screen reader, screen magnifier, etc.) do
  // not auto-close on a timer to allow users sufficient time to locate and
  // interact with it.
  if (!content::BrowserAccessibilityState::GetInstance()
           ->GetAccessibilityMode()
           .is_mode_off()) {
    return base::TimeDelta::Max();
  }
  return kAutoClosePartialViewDelay;
}

std::vector<DownloadUIModel::DownloadUIModelPtr>
DownloadToolbarUIController::GetPrimaryViewModels() {
  switch (primary_view_mode_) {
    case DownloadBubbleMode::kPartial:
      return bubble_controller_->GetPartialView();
    case DownloadBubbleMode::kComplete:
      return bubble_controller_->GetMainView();
  }
}

bool DownloadToolbarUIController::ShouldShowBubbleAsInactive() const {
  // The bubble can either be shown as active or inactive. When the current
  // browser is inactive, make the bubble inactive to avoid stealing focus from
  // non-Chrome windows or showing on a different workspace.
  if (!browser_view_->browser()->GetWindow() ||
      !browser_view_->browser()->GetWindow()->IsActive()) {
    return true;
  }

  // Don't show as active if there is a running context menu, otherwise the
  // context menu will be closed.
  tabs::TabInterface* const tab =
      browser_view_->browser()->GetActiveTabInterface();
  if (content::WebContents* web_contents = tab ? tab->GetContents() : nullptr) {
    if (web_contents->IsShowingContextMenu()) {
      return true;
    }
  }

  // The partial view shows up without user interaction, so it should not
  // steal focus from the web contents.
  return primary_view_mode_ == DownloadBubbleMode::kPartial;
}

void DownloadToolbarUIController::CloseAutofillPopup() {
  tabs::TabInterface* const tab =
      browser_view_->browser()->GetActiveTabInterface();
  content::WebContents* web_contents = tab ? tab->GetContents() : nullptr;
  if (!web_contents) {
    return;
  }
  if (auto* autofill_client =
          autofill::ContentAutofillClient::FromWebContents(web_contents)) {
    autofill_client->HideSuggestions(
        autofill::SuggestionHidingReason::kOverlappingWithAnotherPrompt,
        /*product=*/std::nullopt);
  }
}

bool DownloadToolbarUIController::ShouldShowScanningAnimation() const {
  return !is_dormant_ && (state_ == IconState::kDeepScanning ||
                          state_ == IconState::kContentCheckPending ||
                          !progress_info_.progress_certain);
}

void DownloadToolbarUIController::UpdateIconDormant() {
  // Ensure no updates are attempted once BrowserView destruction has started or
  // if the host Widget has already been closed.
  if (!browser_view_ || !browser_view_->GetWidget() ||
      browser_view_->GetWidget()->IsClosed()) {
    return;
  }

  // Check if the current browser is the last active browser in this profile.
  // TODO(crbug.com/323962334): This should also check whether the bubble is
  // open once the bubble is added.
  BrowserWindowInterface* last_active =
      ProfileBrowserCollection::GetForProfile(browser_view_->GetProfile())
          ->GetLastActiveBrowser();
  bool should_update_button_progress =
      last_active && browser_view_->browser() == last_active;
  if (is_dormant_ == !should_update_button_progress) {
    return;
  }
  is_dormant_ = !should_update_button_progress;
  UpdateIcon();
}

DownloadDisplay::IconState DownloadToolbarUIController::GetIconState() const {
  return state_;
}

void DownloadToolbarUIController::OnAnyRowRemoved() {
  if (bubble_contents_->info().row_list_view_info().rows().empty()) {
    CloseDialog(views::Widget::ClosedReason::kUnspecified);
  }
}
