// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tab_contents/chrome_web_contents_view_focus_helper.h"

#include "chrome/browser/ui/sad_tab_helper.h"
#include "chrome/browser/ui/views/sad_tab_view.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/widget/widget.h"

// static
void ChromeWebContentsViewFocusHelper::CreateForWebContents(
    content::WebContents* web_contents) {
  if (!ChromeWebContentsViewFocusHelper::FromWebContents(web_contents)) {
    web_contents->SetUserData(
        ChromeWebContentsViewFocusHelper::UserDataKey(),
        base::WrapUnique(new ChromeWebContentsViewFocusHelper(web_contents)));
  }
}

ChromeWebContentsViewFocusHelper::ChromeWebContentsViewFocusHelper(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {}

bool ChromeWebContentsViewFocusHelper::Focus() {
  SadTabHelper* sad_tab_helper = SadTabHelper::FromWebContents(web_contents_);
  if (sad_tab_helper) {
    SadTabView* sad_tab = static_cast<SadTabView*>(sad_tab_helper->sad_tab());
    if (sad_tab) {
      sad_tab->RequestFocus();
      return true;
    }
  }

  const web_modal::WebContentsModalDialogManager* manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(web_contents_);
  if (manager && manager->IsDialogActive()) {
    manager->FocusTopmostDialog();
    return true;
  }

  return false;
}

bool ChromeWebContentsViewFocusHelper::TakeFocus(bool reverse) {
  views::FocusManager* focus_manager = GetFocusManager();
  if (focus_manager) {
    focus_manager->AdvanceFocus(reverse);
    return true;
  }
  return false;
}

void ChromeWebContentsViewFocusHelper::StoreFocus() {
  last_focused_view_tracker_.SetView(nullptr);
  if (GetFocusManager())
    last_focused_view_tracker_.SetView(GetFocusManager()->GetFocusedView());
}

bool ChromeWebContentsViewFocusHelper::RestoreFocus() {
  views::View* view_to_focus = GetStoredFocus();
  last_focused_view_tracker_.SetView(nullptr);
  if (view_to_focus) {
    view_to_focus->RequestFocus();
    return true;
  }
  return false;
}

void ChromeWebContentsViewFocusHelper::ResetStoredFocus() {
  last_focused_view_tracker_.SetView(nullptr);
}

views::View* ChromeWebContentsViewFocusHelper::GetStoredFocus() {
  views::View* last_focused_view = last_focused_view_tracker_.view();
  if (last_focused_view && last_focused_view->IsFocusable() &&
      GetFocusManager()->ContainsView(last_focused_view)) {
    return last_focused_view;
  }
  return nullptr;
}

gfx::NativeView ChromeWebContentsViewFocusHelper::GetActiveNativeView() {
  return web_contents_->GetFullscreenRenderWidgetHostView() ?
      web_contents_->GetFullscreenRenderWidgetHostView()->GetNativeView() :
      web_contents_->GetNativeView();
}

views::Widget* ChromeWebContentsViewFocusHelper::GetTopLevelWidget() {
  return views::Widget::GetTopLevelWidgetForNativeView(GetActiveNativeView());
}

views::FocusManager* ChromeWebContentsViewFocusHelper::GetFocusManager() {
  views::Widget* toplevel_widget = GetTopLevelWidget();
  return toplevel_widget ? toplevel_widget->GetFocusManager() : NULL;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ChromeWebContentsViewFocusHelper)
