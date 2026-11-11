// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_bubble_controller_base.h"

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "build/build_config.h"
#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/autofill/autofill_bubble_handler.h"
#include "chrome/browser/ui/autofill/bubble_manager.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/tabs/public/tab_interface.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/page_action/page_action_controller.h"
#include "ui/actions/actions.h"
#endif  // !BUILDFLAG(IS_ANDROID)

namespace autofill {

AutofillBubbleControllerBase::AutofillBubbleControllerBase(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

AutofillBubbleControllerBase::AutofillBubbleControllerBase(
    content::WebContents* web_contents,
    PageActionConfig page_action_config)
    : content::WebContentsObserver(web_contents),
      page_action_config_(page_action_config) {}

AutofillBubbleControllerBase::AutofillBubbleControllerBase(
    tabs::TabInterface* tab_interface,
    PageActionConfig page_action_config)
    : content::WebContentsObserver(CHECK_DEREF(tab_interface).GetContents()),
      tab_interface_(tab_interface),
      page_action_config_(page_action_config),
      will_discard_contents_subscription_(
          tab_interface_->RegisterWillDiscardContents(base::BindRepeating(
              &AutofillBubbleControllerBase::WillDiscardContents,
              base::Unretained(this)))) {}

AutofillBubbleControllerBase::~AutofillBubbleControllerBase() {
  if (IsShowingBubble()) {
    SetActionItemIsShowingBubble(false);
    bubble_view_->Hide();
    bubble_view_ = nullptr;
  }
}

void AutofillBubbleControllerBase::OnVisibilityChanged(
    content::Visibility visibility) {
  if (IsBubbleManagerEnabled()) {
    // BubbleManager will handle the effects of tab changes.
    return;
  }

  if (visibility == content::Visibility::HIDDEN) {
    HideBubble(/*initiated_by_bubble_manager=*/false);
  }
}

bool AutofillBubbleControllerBase::ShouldReshowOnTabVisible() const {
  return false;
}

std::optional<actions::ActionId>
AutofillBubbleControllerBase::GetActionIdForPageAction() {
  if (page_action_config_ && page_action_config_->should_manage_visibility) {
    return page_action_config_->action_id;
  }
  return std::nullopt;
}

void AutofillBubbleControllerBase::WebContentsDestroyed() {
  if (IsShowingBubble()) {
    SetActionItemIsShowingBubble(false);
    bubble_view_->Hide();
    bubble_view_ = nullptr;
  }
}

bool AutofillBubbleControllerBase::ShouldShowPageAction() {
  return IsShowingBubble();
}

std::optional<std::u16string>
AutofillBubbleControllerBase::GetPageActionTooltipText() {
  return std::nullopt;
}

void AutofillBubbleControllerBase::UpdatePageActionIcon() {
  // Page action icons do not exist for Android.
#if !BUILDFLAG(IS_ANDROID)
  std::optional<actions::ActionId> action_id = GetActionIdForPageAction();
  if (!action_id.has_value()) {
    return;
  }

  tabs::TabInterface* const tab_interface = GetTabInterface();
  if (!tab_interface) {
    return;
  }

  // NOTE: Consider creating a separate page action view controller file
  // when the logic to show the page action become complex.
  page_actions::PageActionController* page_action_controller =
      page_actions::PageActionController::From(tab_interface);
  if (!page_action_controller) {
    return;
  }

  if (ShouldShowPageAction()) {
    if (auto tooltip_text = GetPageActionTooltipText()) {
      page_action_controller->OverrideText(*action_id, *tooltip_text);
      page_action_controller->OverrideTooltip(*action_id, *tooltip_text);
    }
    page_action_controller->Show(*action_id);
  } else {
    page_action_controller->Hide(*action_id);
    page_action_controller->ClearOverrideText(*action_id);
    page_action_controller->ClearOverrideTooltip(*action_id);
  }
#endif  // !BUILDFLAG(IS_ANDROID)
}

void AutofillBubbleControllerBase::ShowBubble() {
  was_bubble_shown_ = true;
  UpdatePageActionIcon();
  DoShowBubble();
  UpdatePageActionIcon();
}

void AutofillBubbleControllerBase::HideBubble(
    bool initiated_by_bubble_manager) {
  if (IsShowingBubble()) {
    bubble_hide_initiated_by_bubble_manager_ = initiated_by_bubble_manager;
    bubble_view_->Hide();
    ResetBubbleViewAndInformBubbleManager();
  }
  bubble_hide_initiated_by_bubble_manager_ = false;
}

bool AutofillBubbleControllerBase::CanBeReshown() const {
  return true;
}

bool AutofillBubbleControllerBase::IsShowingBubble() const {
  return bubble_view_ != nullptr;
}

bool AutofillBubbleControllerBase::IsMouseHovered() const {
  return IsShowingBubble() && bubble_view_->IsMouseHovered();
}

bool AutofillBubbleControllerBase::IsBubbleManagerEnabled() {
#if BUILDFLAG(IS_ANDROID)
  return false;
#else
  return BubbleManager::GetForTab(GetTabInterface()) != nullptr;
#endif
}

bool AutofillBubbleControllerBase::MaySetUpBubble() {
#if BUILDFLAG(IS_ANDROID)
  return true;
#else  // BUILDFLAG(IS_ANDROID)
  BubbleManager* const manager = BubbleManager::GetForTab(GetTabInterface());
  return !manager || !manager->HasConflictingPendingBubble(GetBubbleType());
#endif
}

void AutofillBubbleControllerBase::QueueOrShowBubble(bool force_show) {
#if !BUILDFLAG(IS_ANDROID)
  if (BubbleManager* const manager =
          BubbleManager::GetForTab(GetTabInterface())) {
    manager->RequestShowController(*this, force_show);
    return;
  }
#endif

  ShowBubble();
}

void AutofillBubbleControllerBase::SetBubbleView(
    AutofillBubbleBase& bubble_view) {
  bubble_view_ = &bubble_view;
  SetActionItemIsShowingBubble(true);
}

void AutofillBubbleControllerBase::ResetBubbleViewAndInformBubbleManager() {
#if !BUILDFLAG(IS_ANDROID)
  const bool was_showing = IsShowingBubble();
#endif  // !BUILDFLAG(IS_ANDROID)

  bubble_view_ = nullptr;

#if !BUILDFLAG(IS_ANDROID)
  if (was_showing) {
    SetActionItemIsShowingBubble(false);
    if (BubbleManager* const manager =
            BubbleManager::GetForTab(GetTabInterface())) {
      manager->OnBubbleHiddenByController(*this,
                                          allow_bubble_manager_to_show_next_);
    }
  }
#endif  // !BUILDFLAG(IS_ANDROID)
}

tabs::TabInterface* AutofillBubbleControllerBase::GetTabInterface() {
  if (tab_interface_) {
    return tab_interface_.get();
  }
  if (!web_contents()) {
    return nullptr;
  }
  return tabs::TabInterface::MaybeGetFromContents(web_contents());
}

AutofillBubbleHandler*
AutofillBubbleControllerBase::GetAutofillBubbleHandler() {
#if !BUILDFLAG(IS_ANDROID)
  // We may be in a non-tab WebContents such as a side panel or extension popup;
  // in those cases we cannot access the AutofillBubbleHandler.
  tabs::TabInterface* const tab = GetTabInterface();
  if (!tab) {
    return nullptr;
  }
  BrowserWindowInterface* browser = tab->GetBrowserWindowInterface();
  if (!browser) {
    return nullptr;
  }
  if (AutofillBubbleHandler* handler =
          AutofillBubbleHandler::Get(browser->GetUnownedUserDataHost())) {
    return handler;
  }
  if (BrowserWindow* browser_window = BrowserWindow::FromBrowser(browser)) {
    return browser_window->GetAutofillBubbleHandler();
  }
#endif  // !BUILDFLAG(IS_ANDROID)
  return nullptr;
}

void AutofillBubbleControllerBase::WillDiscardContents(
    tabs::TabInterface* tab,
    content::WebContents* old_contents,
    content::WebContents* new_contents) {
  Observe(new_contents);
}

void AutofillBubbleControllerBase::SetActionItemIsShowingBubble(bool showing) {
#if !BUILDFLAG(IS_ANDROID)
  if (!page_action_config_ ||
      !page_action_config_->should_sync_bubble_showing_state) {
    return;
  }
  tabs::TabInterface* const tab = GetTabInterface();
  if (!tab) {
    return;
  }
  // `BubbleManagerImpl` hides active bubbles synchronously on
  // `WillDeactivate`, which `TabStripModel` fires before detaching a tab from
  // its model (`TabModel::OnRemovedFromModel()`), so `browser_window` still
  // points to the window where the bubble was shown.
  BrowserWindowInterface* const browser_window =
      tab->GetBrowserWindowInterface();
  if (!browser_window) {
    return;
  }
  BrowserActions* const browser_actions = BrowserActions::From(browser_window);
  if (!browser_actions || !browser_actions->root_action_item()) {
    return;
  }
  if (actions::ActionItem* const action_item =
          actions::ActionManager::Get().FindAction(
              page_action_config_->action_id,
              browser_actions->root_action_item())) {
    action_item->SetIsShowingBubble(showing);
  }
#endif  // !BUILDFLAG(IS_ANDROID)
}

}  // namespace autofill
