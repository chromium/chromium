// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/split_tab_highlight_controller.h"

#include <memory>

#include "base/callback_list.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/split_tab_highlight_delegate.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view_base.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace split_tabs {

SplitTabHighlightController::SplitTabHighlightController(
    BrowserView* browser_view)
    : split_tab_highlight_delegate_(
          std::make_unique<split_tabs::SplitTabHighlightDelegateImpl>(
              browser_view)),
      browser_window_interface_(browser_view->browser()) {
  active_tab_change_subscription_ =
      browser_window_interface_->RegisterActiveTabDidChange(
          base::BindRepeating(&SplitTabHighlightController::OnActiveTabChange,
                              base::Unretained(this)));
  chip_controller_observation_.Observe(
      browser_view->toolbar()->location_bar()->GetChipController());
  page_info_bubble_created_subscription_ =
      PageInfoBubbleViewBase::RegisterPageInfoCreatedCallback(
          base::BindRepeating(
              &SplitTabHighlightController::OnPageInfoBubbleCreated,
              base::Unretained(this)));
}

SplitTabHighlightController::~SplitTabHighlightController() = default;

bool SplitTabHighlightController::ShouldHighlight() {
  return is_omnibox_popup_showing_ || is_permission_prompt_showing_ ||
         is_page_info_bubble_showing_;
}

void SplitTabHighlightController::OnOmniboxPopupVisibilityChanged(
    bool popup_is_open) {
  is_omnibox_popup_showing_ = popup_is_open;
  UpdateHighlight();
}

void SplitTabHighlightController::OnPermissionPromptShown() {
  is_permission_prompt_showing_ = true;
  UpdateHighlight();
}

void SplitTabHighlightController::OnPermissionPromptHidden() {
  is_permission_prompt_showing_ = false;
  UpdateHighlight();
}

void SplitTabHighlightController::OnWidgetVisibilityChanged(
    views::Widget* widget,
    bool visible) {
  is_page_info_bubble_showing_ = visible;
  UpdateHighlight();
}

void SplitTabHighlightController::OnWidgetDestroyed(views::Widget* widget) {
  page_info_bubble_observation_.Reset();
  is_page_info_bubble_showing_ = false;
  UpdateHighlight();
}

void SplitTabHighlightController::OnActiveTabChange(
    BrowserWindowInterface* browser_window_interface) {
  omnibox_tab_helper_observation_.Reset();
  tabs::TabInterface* const active_tab =
      browser_window_interface->GetActiveTabInterface();
  if (active_tab) {
    tab_will_detach_subscription_ = active_tab->RegisterWillDetach(
        base::BindRepeating(&SplitTabHighlightController::OnTabWillDetach,
                            base::Unretained(this)));
    OmniboxTabHelper* const tab_helper =
        OmniboxTabHelper::FromWebContents(active_tab->GetContents());
    CHECK(tab_helper);
    omnibox_tab_helper_observation_.Observe(tab_helper);
    tab_will_discard_subscription_ = active_tab->RegisterWillDiscardContents(
        base::BindRepeating(&SplitTabHighlightController::OnTabWillDiscard,
                            base::Unretained(this)));
  }
  // Need to update the highlight because the omnibox focus state
  // event might have already been triggered before the active tab change.
  UpdateHighlight();
}

void SplitTabHighlightController::OnTabWillDetach(
    tabs::TabInterface* tab_interface,
    tabs::TabInterface::DetachReason reason) {
  // Reset the omnibox tab helper observation to ensure that it doesn't live
  // longer than the web contents it is observing.
  omnibox_tab_helper_observation_.Reset();
  tab_will_detach_subscription_ = base::CallbackListSubscription();
}

void SplitTabHighlightController::OnTabWillDiscard(
    tabs::TabInterface* tab_interface,
    content::WebContents* old_contents,
    content::WebContents* new_contents) {
  // Reset the observation of the omnibox tab helper since it is possible for
  // the active tab to be discarded on CrOS.
  omnibox_tab_helper_observation_.Reset();
  is_omnibox_popup_showing_ = false;
  UpdateHighlight();
}

void SplitTabHighlightController::OnPageInfoBubbleCreated(
    PageInfoBubbleViewBase* bubble_view) {
  views::Widget* const bubble_widget = bubble_view->GetWidget();
  if (browser_window_interface_->GetActiveTabInterface()->GetContents() ==
      bubble_view->web_contents()) {
    page_info_bubble_observation_.Reset();
    page_info_bubble_observation_.Observe(bubble_widget);
  }

  is_page_info_bubble_showing_ = bubble_widget->IsVisible();
  UpdateHighlight();
}

void SplitTabHighlightController::UpdateHighlight() {
  split_tab_highlight_delegate_->SetHighlight(ShouldHighlight());
}

}  // namespace split_tabs
