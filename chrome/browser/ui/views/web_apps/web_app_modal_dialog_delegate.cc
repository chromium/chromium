// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/web_app_modal_dialog_delegate.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/extensions/security_dialog_tracker.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/page_action/page_action_view_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/views/widget/widget.h"

namespace web_app {

WebAppModalDialogDelegate::WebAppModalDialogDelegate(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

WebAppModalDialogDelegate::~WebAppModalDialogDelegate() = default;

void WebAppModalDialogDelegate::OnWidgetShownStartTracking(
    views::Widget* dialog_widget) {
  occlusion_observation_.Observe(dialog_widget);
  widget_observation_.Observe(dialog_widget);
  extensions::SecurityDialogTracker::GetInstance()->AddSecurityDialog(
      dialog_widget);
}

void WebAppModalDialogDelegate::OnVisibilityChanged(
    content::Visibility visibility) {
  if (visibility != content::Visibility::VISIBLE) {
    CloseDialogAsIgnored();
  }
}

void WebAppModalDialogDelegate::WebContentsDestroyed() {
  CloseDialogAsIgnored();
}

void WebAppModalDialogDelegate::PrimaryPageChanged(content::Page& page) {
  CloseDialogAsIgnored();
}

void WebAppModalDialogDelegate::OnWidgetDestroyed(views::Widget* widget) {
  widget_observation_.Reset();
}

void WebAppModalDialogDelegate::OnOcclusionStateChanged(bool occluded) {
  // If a picture-in-picture window is occluding the dialog, force it to close
  // to prevent spoofing.
  if (occluded) {
    PictureInPictureWindowManager::GetInstance()->ExitPictureInPicture();
  }
}

void WebAppModalDialogDelegate::MaybeRestoreFocusToInstallPageAction() {
  content::WebContents* contents = web_contents();
  if (!contents) {
    return;
  }
  tabs::TabInterface* tab = tabs::TabInterface::MaybeGetFromContents(contents);
  if (!tab) {
    return;
  }
  BrowserWindowInterface* browser = tab->GetBrowserWindowInterface();
  if (!browser) {
    return;
  }
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  if (!browser_view || !browser_view->toolbar_button_provider()) {
    return;
  }
  page_actions::PageActionViewInterface* page_action_interface =
      browser_view->toolbar_button_provider()->GetPageActionViewInterface(
          kActionInstallPwa);
  if (!page_action_interface) {
    return;
  }
  views::View* anchor_view =
      page_action_interface->GetBubbleAnchor().GetIfView();
  if (anchor_view && anchor_view->GetVisible()) {
    anchor_view->RequestFocus();
  }
}

}  // namespace web_app
