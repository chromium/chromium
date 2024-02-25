// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tab_contents/chrome_web_contents_view_focus_helper.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/ui/sad_tab_helper.h"
#include "chrome/browser/ui/views/sad_tab_view.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/widget/widget.h"

ChromeWebContentsViewFocusHelper::ChromeWebContentsViewFocusHelper(
    content::WebContents* web_contents)
    : content::WebContentsUserData<ChromeWebContentsViewFocusHelper>(
          *web_contents) {}

bool ChromeWebContentsViewFocusHelper::Focus() {
  SadTabHelper* sad_tab_helper =
      SadTabHelper::FromWebContents(&GetWebContents());
  if (sad_tab_helper) {
    SadTabView* sad_tab = static_cast<SadTabView*>(sad_tab_helper->sad_tab());
    if (sad_tab) {
      sad_tab->RequestFocus();
      return true;
    }
  }

  const web_modal::WebContentsModalDialogManager* manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(
          &GetWebContents());
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
  views::FocusManager* focus_manager = GetFocusManager();
  if (last_focused_view && focus_manager && last_focused_view->IsFocusable() &&
      focus_manager->ContainsView(last_focused_view)) {
    return last_focused_view;
  }
  return nullptr;
}

gfx::NativeView ChromeWebContentsViewFocusHelper::GetActiveNativeView() {
  return GetWebContents().GetNativeView();
}

views::Widget* ChromeWebContentsViewFocusHelper::GetTopLevelWidget() {
  return views::Widget::GetTopLevelWidgetForNativeView(GetActiveNativeView());
}

views::FocusManager* ChromeWebContentsViewFocusHelper::GetFocusManager() {
  views::Widget* toplevel_widget = GetTopLevelWidget();
  return toplevel_widget ? toplevel_widget->GetFocusManager() : nullptr;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ChromeWebContentsViewFocusHelper);
