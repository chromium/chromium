// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/download/bubble/download_toolbar_ui_controller.h"

#include <string>

#include "base/functional/bind.h"
#include "base/i18n/number_formatting.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "cc/paint/paint_flags.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/download/bubble/download_bubble_prefs.h"
#include "chrome/browser/download/bubble/download_bubble_ui_controller.h"
#include "chrome/browser/download/bubble/download_display_controller.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/pinned_action_toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions_container.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/render_text.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/components/kiosk/kiosk_utils.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/fullscreen_util_mac.h"
#endif

namespace {

PinnedToolbarActionsContainer* GetPinnedToolbarActionsContainer(
    BrowserView* browser_view) {
  return browser_view->toolbar()->pinned_toolbar_actions_container();
}

ToolbarButton* GetDownloadsButton(BrowserView* browser_view) {
  auto* container = GetPinnedToolbarActionsContainer(browser_view);
  return container ? container->GetButtonFor(kActionShowDownloads) : nullptr;
}

SkColor GetIconColor(bool is_dormant,
                     DownloadDisplay::IconActive active,
                     const ui::ColorProvider* color_provider) {
  ui::ColorId color_id = kColorDownloadToolbarButtonActive;
  if (is_dormant || active != DownloadDisplay::IconActive::kActive) {
    color_id = kColorDownloadToolbarButtonInactive;
  }
  return color_provider->GetColor(color_id);
}

}  // namespace

DownloadToolbarUIController::DownloadToolbarUIController(
    BrowserView* browser_view)
    : browser_view_(browser_view) {
  action_item_ =
      actions::ActionManager::Get()
          .FindAction(
              kActionShowDownloads,
              browser_view_->browser()->browser_actions()->root_action_item())
          ->GetAsWeakPtr();
  tooltip_texts_[0] = l10n_util::GetStringUTF16(IDS_TOOLTIP_DOWNLOAD_ICON);
  action_item_->SetTooltipText(tooltip_texts_.at(0));

  bubble_controller_ =
      std::make_unique<DownloadBubbleUIController>(browser_view_->browser());

  BrowserList::GetInstance()->AddObserver(this);

  // Wait until we're done with everything else before creating `controller_`
  // since it can call `Show()` synchronously.
  controller_ = std::make_unique<DownloadDisplayController>(
      this, browser_view_->browser(), bubble_controller_.get());
}

DownloadToolbarUIController::~DownloadToolbarUIController() {
  BrowserList::GetInstance()->RemoveObserver(this);
  controller_.reset();
  bubble_controller_.reset();
}

void DownloadToolbarUIController::Show() {
  auto* container = GetPinnedToolbarActionsContainer(browser_view_);
  container->ShowActionEphemerallyInToolbar(kActionShowDownloads, true);
}

void DownloadToolbarUIController::Hide() {
  HideDetails();
  auto* container = GetPinnedToolbarActionsContainer(browser_view_);
  if (!container) {
    return;
  }
  container->ShowActionEphemerallyInToolbar(kActionShowDownloads, false);
}

bool DownloadToolbarUIController::IsShowing() const {
  auto* button = GetDownloadsButton(browser_view_);
  return button && button->GetVisible();
}

void DownloadToolbarUIController::Enable() {
  if (action_item_.get()) {
    action_item_->SetEnabled(true);
  }
}

void DownloadToolbarUIController::Disable() {
  if (action_item_.get()) {
    action_item_->SetEnabled(false);
  }
}

void DownloadToolbarUIController::UpdateDownloadIcon(
    const IconUpdateInfo& updates) {
  // Whether to update the icon after processing any changes.
  bool update_icon = false;

  if (updates.new_state && *updates.new_state != state_) {
    update_icon = true;
    state_ = *updates.new_state;
  }
  if (updates.new_active && *updates.new_active != active_) {
    update_icon = true;
    active_ = *updates.new_active;
  }

  if (update_icon) {
    UpdateIcon();
  }
}

void DownloadToolbarUIController::AnnounceAccessibleAlertNow(
    const std::u16string& alert_text) {
  if (auto* button = GetDownloadsButton(browser_view_)) {
    button->GetViewAccessibility().AnnounceText(alert_text);
  }
}

bool DownloadToolbarUIController::IsFullscreenWithParentViewHidden() const {
#if BUILDFLAG(IS_MAC)
  if (fullscreen_utils::IsInContentFullscreen(browser_view_->browser())) {
    return true;
  }
#endif

  // If immersive fullscreen, check if top chrome is visible.
  if (browser_view_ && browser_view_->GetLocationBarView() &&
      browser_view_->IsImmersiveModeEnabled()) {
    return !browser_view_->immersive_mode_controller()->IsRevealed();
  }

  // Handle the remaining fullscreen case.
  return browser_view_->browser()->window() &&
         browser_view_->browser()->window()->IsFullscreen() &&
         !browser_view_->browser()->window()->IsToolbarVisible();
}

bool DownloadToolbarUIController::ShouldShowExclusiveAccessBubble() const {
  if (!IsFullscreenWithParentViewHidden()) {
    return false;
  }
  if (!browser_view_) {
    return false;
  }
#if BUILDFLAG(IS_CHROMEOS)
  if (chromeos::IsKioskSession()) {
    return false;
  }
#endif
  return !browser_view_->IsImmersiveModeEnabled() &&
         browser_view_->CanUserExitFullscreen();
}

void DownloadToolbarUIController::OpenSecuritySubpage(
    const offline_items_collection::ContentId& id) {
  // TODO(crbug.com/323962334): Add implementation once this class becomes a
  // DownloadBubbleNavigationHandler.
}

void DownloadToolbarUIController::ShowDetails() {
  // TODO(crbug.com/323962334): Add implementation once this class becomes a
  // DownloadBubbleNavigationHandler.
}

void DownloadToolbarUIController::HideDetails() {
  // TODO(crbug.com/323962334): Add implementation once this class becomes a
  // DownloadBubbleNavigationHandler.
}

bool DownloadToolbarUIController::IsShowingDetails() const {
  // TODO(crbug.com/323962334): Add implementation once this class becomes a
  // DownloadBubbleNavigationHandler.
  return false;
}

void DownloadToolbarUIController::UpdateIcon() {
  if (!action_item_.get()) {
    return;
  }

  if (!GetDownloadsButton(browser_view_)) {
    return;
  }

  const gfx::VectorIcon* new_icon;
  SkColor icon_color =
      GetIconColor(is_dormant_, active_, browser_view_->GetColorProvider());
  bool is_touch_mode = ui::TouchUiController::Get()->touch_ui();
  if (state_ == IconState::kProgress || state_ == IconState::kDeepScanning) {
    new_icon = is_touch_mode ? &kDownloadInProgressTouchIcon
                             : &kDownloadInProgressChromeRefreshIcon;
  } else {
    new_icon = is_touch_mode ? &kDownloadToolbarButtonTouchIcon
                             : &kDownloadToolbarButtonChromeRefreshIcon;
  }
  actions::ActionManager::Get()
      .FindAction(
          kActionShowDownloads,
          browser_view_->browser()->browser_actions()->root_action_item())
      ->SetProperty(kActionItemUnderlineIndicatorKey,
                    (!is_dormant_ && (active_ == IconActive::kActive)));

  action_item_->SetImage(ui::ImageModel::FromVectorIcon(*new_icon, icon_color));
}

void DownloadToolbarUIController::OnBrowserSetLastActive(Browser* browser) {
  UpdateIconDormant();
}

void DownloadToolbarUIController::OnBrowserNoLongerActive(Browser* browser) {
  UpdateIconDormant();
}

void DownloadToolbarUIController::InvokeUI() {
  // TODO(crbug.com/323962334): Add implementation opening downloads bubble if
  // there are recent downloads once this class becomes a
  // DownloadBubbleNavigationHandler.
  chrome::ShowDownloads(browser_view_->browser());
  controller_->OnButtonPressed();
}

void DownloadToolbarUIController::CloseAutofillPopup() {
  content::WebContents* web_contents =
      browser_view_->browser()->tab_strip_model()->GetActiveWebContents();
  if (!web_contents) {
    return;
  }
  if (auto* autofill_client =
          autofill::ContentAutofillClient::FromWebContents(web_contents)) {
    autofill_client->HideAutofillSuggestions(
        autofill::SuggestionHidingReason::kOverlappingWithAnotherPrompt);
  }
}

void DownloadToolbarUIController::UpdateIconDormant() {
  // Check if the current browser is the last active browser in this profile.
  // TODO(crbug.com/323962334): This should also check whether the bubble is
  // open once the bubble is added.
  bool should_update_button_progress =
      browser_view_->browser() ==
      chrome::FindBrowserWithProfile(browser_view_->GetProfile());
  if (is_dormant_ == !should_update_button_progress) {
    return;
  }
  is_dormant_ = !should_update_button_progress;
  UpdateIcon();
}

DownloadDisplay::IconState DownloadToolbarUIController::GetIconState() const {
  return state_;
}
