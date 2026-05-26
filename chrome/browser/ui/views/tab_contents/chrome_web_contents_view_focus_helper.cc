// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tab_contents/chrome_web_contents_view_focus_helper.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/sad_tab_controller.h"
#include "chrome/browser/ui/sad_tab_helper.h"
#include "chrome/browser/ui/tabs/public/tab_dialog_manager.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "components/tabs/public/tab_interface.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

ChromeWebContentsViewFocusHelper::ChromeWebContentsViewFocusHelper(
    content::WebContents* web_contents)
    : content::WebContentsUserData<ChromeWebContentsViewFocusHelper>(
          *web_contents) {}

ChromeWebContentsViewFocusHelper::~ChromeWebContentsViewFocusHelper() = default;

bool ChromeWebContentsViewFocusHelper::Focus() {
  SadTabHelper* sad_tab_helper =
      SadTabHelper::FromWebContents(&GetWebContents());
  if (sad_tab_helper) {
    SadTabController* sad_tab =
        static_cast<SadTabController*>(sad_tab_helper->sad_tab());
    if (sad_tab) {
      sad_tab->RequestFocus();
      return true;
    }
  }

  // Don't forward the focus to the modal dialog during the focus restoration.
  // Otherwise, the browser window could fail to activate. See
  // TabDialogManagerDesktopWidgetUiTest.ActivateBrowserWindowWhenModalIsActive.
  if (GetFocusManager() && GetFocusManager()->IsSettingFocusedView() &&
      GetFocusManager()->focus_change_reason() ==
          views::FocusManager::FocusChangeReason::kFocusRestore) {
    return false;
  }

  const web_modal::WebContentsModalDialogManager* manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(
          &GetWebContents());
  if (manager && manager->IsDialogActive()) {
    manager->FocusTopmostDialog();
    return true;
  }

  tabs::TabInterface* tab_interface =
      tabs::TabInterface::MaybeGetFromContents(&GetWebContents());
  // WebApps and unit tests don't have TabFeatures and TabDialogManager.
  tabs::TabDialogManager* tab_dialog_manager =
      tab_interface && tab_interface->GetTabFeatures()
          ? tab_interface->GetTabFeatures()->tab_dialog_manager()
          : nullptr;
  if (tab_dialog_manager && tab_dialog_manager->MaybeActivateDialog()) {
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
  if (!GetFocusManager()) {
    return;
  }

  views::View* focused_view = GetFocusManager()->GetFocusedView();
  if (!focused_view) {
    return;
  }

  // Iterate through the focused view's ancestors to check if it's inside the
  // toolbar. We don't want to store the focus in the toolbar WebContents to
  // match the behavior in C++ views.
  // TODO(crbug.com/508632926): this is a temporary fix to differentiate the
  // focus behavior between normal WebContents and top Chrome WebUI. In the
  // long term, we should propose a more general fix from the focus system to
  // avoid doing this special checks against the element identifier. (e.g. the
  // focus behavior of the WebUI reload button is correct when the user clicks
  // the button, and the tab-key focus should follow the same way).
  for (views::View* v = focused_view; v; v = v->parent()) {
    if (v->GetProperty(views::kElementIdentifierKey) ==
        kWebUIToolbarElementIdentifier) {
      return;
    }
  }

  last_focused_view_tracker_.SetView(focused_view);
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

void ChromeWebContentsViewFocusHelper::SetStoredFocusView(views::View* view) {
  last_focused_view_tracker_.SetView(view);
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
