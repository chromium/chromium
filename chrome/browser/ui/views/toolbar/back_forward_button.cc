// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/back_forward_button.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/chained_back_navigation_tracker.h"
#include "chrome/browser/preloading/chrome_preloading.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/toolbar/back_forward_menu_model.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/window_open_disposition_utils.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/view_class_properties.h"

BackForwardButton::BackForwardButton(Direction direction,
                                     PressedCallback callback,
                                     Browser* browser)
    : ToolbarButton(std::move(callback),
                    std::make_unique<BackForwardMenuModel>(
                        browser,
                        direction == Direction::kBack
                            ? BackForwardMenuModel::ModelType::kBackward
                            : BackForwardMenuModel::ModelType::kForward),
                    browser->tab_strip_model()),
      browser_(browser),
      direction_(direction) {
  SetHideInkDropWhenShowingContextMenu(false);
  SetTriggerableEventFlags(ui::EF_LEFT_MOUSE_BUTTON |
                           ui::EF_MIDDLE_MOUSE_BUTTON);
  if (direction == Direction::kBack) {
    SetVectorIcons(vector_icons::kBackArrowChromeRefreshIcon,
                   kBackArrowTouchIcon);
    SetTooltipText(l10n_util::GetStringUTF16(IDS_TOOLTIP_BACK));
    GetViewAccessibility().SetName(l10n_util::GetStringUTF16(IDS_ACCNAME_BACK));
    GetViewAccessibility().SetDescription(
        l10n_util::GetStringUTF8(IDS_ACCDESCRIPTION_BACK));
    SetID(VIEW_ID_BACK_BUTTON);
    SetProperty(views::kElementIdentifierKey, kToolbarBackButtonElementId);
    set_menu_identifier(kToolbarBackButtonMenuElementId);
  } else {
    SetVectorIcons(vector_icons::kForwardArrowChromeRefreshIcon,
                   kForwardArrowTouchIcon);
    SetTooltipText(l10n_util::GetStringUTF16(IDS_TOOLTIP_FORWARD));
    GetViewAccessibility().SetName(
        l10n_util::GetStringUTF16(IDS_ACCNAME_FORWARD));
    GetViewAccessibility().SetDescription(
        l10n_util::GetStringUTF8(IDS_ACCDESCRIPTION_FORWARD));
    SetID(VIEW_ID_FORWARD_BUTTON);
    SetProperty(views::kElementIdentifierKey, kToolbarForwardButtonElementId);
    set_menu_identifier(kToolbarForwardButtonMenuElementId);
  }
}

BackForwardButton::~BackForwardButton() = default;

const std::u16string BackForwardButton::GetAccessiblePageLoadingMessage() {
  // If we don't have a model, there is no menu from which to obtain the title
  // of the page that is about to be loaded.
  if (!menu_model())
    return std::u16string();

  // The title of the page which is about to be loaded should be at the top of
  // the menu.
  return l10n_util::GetStringFUTF16(IDS_PAGE_LOADING_AX_TITLE_FORMAT,
                                    menu_model()->GetLabelAt(0));
}

void BackForwardButton::NotifyClick(const ui::Event& event) {
  // If the focus is on web content the screen reader will announce the page
  // load; if not we want to make sure that something is still announced.
  if (GetFocusManager()->GetFocusedView() !=
      BrowserView::GetBrowserViewForBrowser(browser_)->contents_web_view()) {
    const std::u16string message = GetAccessiblePageLoadingMessage();
    if (!message.empty())
      GetViewAccessibility().AnnounceText(message);
  }

  content::WebContents* web_contents =
      browser_->tab_strip_model()->GetActiveWebContents();
  ChainedBackNavigationTracker* tracker =
      ChainedBackNavigationTracker::FromWebContents(web_contents);
  CHECK(tracker);
  tracker->RecordBackButtonClickForChainedBackNavigation();

  // Do this last because upon activation the MenuModel gets updated, removing
  // the label for the page about to be loaded. However, the title associated
  // with the ContentsWebView has not yet been updated.
  ToolbarButton::NotifyClick(event);
}

void BackForwardButton::StateChanged(ButtonState old_state) {
  ToolbarButton::StateChanged(old_state);
  if (direction_ != Direction::kBack) {
    return;
  }

  if (old_state == ButtonState::STATE_NORMAL &&
      GetState() == ButtonState::STATE_HOVERED) {
    content::WebContents* active_contents =
        browser_->tab_strip_model()->GetActiveWebContents();
    if (active_contents) {
      active_contents->BackNavigationLikely(
          chrome_preloading_predictor::kBackButtonHover,
          last_back_assumed_disposition_);
    }
  }
}

void BackForwardButton::OnMouseEntered(const ui::MouseEvent& event) {
  if (direction_ == Direction::kBack) {
    // Record this before the event triggers `StateChanged` via
    // `ToolbarButton::OnMouseEntered`.
    last_back_assumed_disposition_ = ui::DispositionFromEventFlags(
        event.flags(), WindowOpenDisposition::CURRENT_TAB);
  }

  ToolbarButton::OnMouseEntered(event);
}

bool BackForwardButton::ShouldShowInkdropAfterIphInteraction() {
  return false;
}

BEGIN_METADATA(BackForwardButton)
END_METADATA
