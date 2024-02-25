// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/javascript_dialogs/javascript_tab_modal_dialog_manager_delegate_desktop.h"

#include <utility>

#include "chrome/browser/safe_browsing/user_interaction_observer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/javascript_dialogs/app_modal_dialog_manager.h"
#include "components/javascript_dialogs/tab_modal_dialog_manager.h"
#include "components/javascript_dialogs/tab_modal_dialog_view.h"
#include "components/navigation_metrics/navigation_metrics.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "ui/gfx/text_elider.h"

JavaScriptTabModalDialogManagerDelegateDesktop::
    JavaScriptTabModalDialogManagerDelegateDesktop(
        content::WebContents* web_contents)
    : web_contents_(web_contents) {}

JavaScriptTabModalDialogManagerDelegateDesktop::
    ~JavaScriptTabModalDialogManagerDelegateDesktop() {
  DCHECK(!tab_strip_model_being_observed_);
}

void JavaScriptTabModalDialogManagerDelegateDesktop::WillRunDialog() {
  BrowserList::AddObserver(this);
  // SafeBrowsing Delayed Warnings experiment can delay some SafeBrowsing
  // warnings until user interaction. If the current page has a delayed warning,
  // it'll have a user interaction observer attached. Show the warning
  // immediately in that case.
  safe_browsing::SafeBrowsingUserInteractionObserver* observer =
      safe_browsing::SafeBrowsingUserInteractionObserver::FromWebContents(
          web_contents_);
  if (observer) {
    observer->OnJavaScriptDialog();
  }
}

void JavaScriptTabModalDialogManagerDelegateDesktop::DidCloseDialog() {
  BrowserList::RemoveObserver(this);
}

void JavaScriptTabModalDialogManagerDelegateDesktop::SetTabNeedsAttention(
    bool attention) {
  Browser* browser = chrome::FindBrowserWithTab(web_contents_);
  if (!browser) {
    // It's possible that the WebContents is no longer in the tab strip. If so,
    // just give up. https://crbug.com/786178
    return;
  }

  TabStripModel* tab_strip_model = browser->tab_strip_model();
  SetTabNeedsAttentionImpl(
      attention, tab_strip_model,
      tab_strip_model->GetIndexOfWebContents(web_contents_));
}

bool JavaScriptTabModalDialogManagerDelegateDesktop::IsWebContentsForemost() {
  Browser* browser = BrowserList::GetInstance()->GetLastActive();
  if (!browser) {
    // It's rare, but there are crashes from where sites are trying to show
    // dialogs in the split second of time between when their Browser is gone
    // and they're gone. In that case, bail. https://crbug.com/1142806
    return false;
  }

  return browser->tab_strip_model()->GetActiveWebContents() == web_contents_;
}

bool JavaScriptTabModalDialogManagerDelegateDesktop::IsApp() {
  Browser* browser = chrome::FindBrowserWithTab(web_contents_);
  return browser && (browser->is_type_app() || browser->is_type_app_popup());
}

void JavaScriptTabModalDialogManagerDelegateDesktop::OnBrowserSetLastActive(
    Browser* browser) {
  javascript_dialogs::TabModalDialogManager::FromWebContents(web_contents_)
      ->BrowserActiveStateChanged();
}

void JavaScriptTabModalDialogManagerDelegateDesktop::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (change.type() == TabStripModelChange::kReplaced) {
    auto* replace = change.GetReplace();
    if (replace->old_contents == web_contents_) {
      // At this point, this WebContents is no longer in the tabstrip. The usual
      // teardown will not be able to turn off the attention indicator, so that
      // must be done here.
      SetTabNeedsAttentionImpl(false, tab_strip_model, replace->index);

      javascript_dialogs::TabModalDialogManager::FromWebContents(web_contents_)
          ->CloseDialogWithReason(javascript_dialogs::TabModalDialogManager::
                                      DismissalCause::kTabSwitchedOut);
    }
  } else if (change.type() == TabStripModelChange::kRemoved) {
    for (const auto& contents : change.GetRemove()->contents) {
      if (contents.contents == web_contents_) {
        // We don't call TabStripModel::SetTabNeedsAttention because it causes
        // re-entrancy into TabStripModel and correctness of the |index|
        // parameter is dependent on observer ordering. This is okay in the
        // short term because the tab in question is being removed.
        // TODO(erikchen): Clean up TabStripModel observer API so that this
        // doesn't require re-entrancy and/or works correctly
        // https://crbug.com/842194.
        DCHECK(tab_strip_model_being_observed_);
        tab_strip_model_being_observed_->RemoveObserver(this);
        tab_strip_model_being_observed_ = nullptr;
        javascript_dialogs::TabModalDialogManager::FromWebContents(
            web_contents_)
            ->CloseDialogWithReason(javascript_dialogs::TabModalDialogManager::
                                        DismissalCause::kTabHelperDestroyed);
        break;
      }
    }
  }
}

void JavaScriptTabModalDialogManagerDelegateDesktop::SetTabNeedsAttentionImpl(
    bool attention,
    TabStripModel* tab_strip_model,
    int index) {
  tab_strip_model->SetTabNeedsAttentionAt(index, attention);
  if (attention) {
    tab_strip_model->AddObserver(this);
    tab_strip_model_being_observed_ = tab_strip_model;
  } else {
    DCHECK_EQ(tab_strip_model_being_observed_, tab_strip_model);
    tab_strip_model_being_observed_->RemoveObserver(this);
    tab_strip_model_being_observed_ = nullptr;
  }
}
