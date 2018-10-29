// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/fast_unload_controller.h"

#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#endif  // (ENABLE_EXTENSIONS)

////////////////////////////////////////////////////////////////////////////////
// FastUnloadController, public:

FastUnloadController::FastUnloadController(Browser* browser)
    : browser_(browser),
      tab_needing_before_unload_ack_(NULL),
      is_attempting_to_close_browser_(false),
      weak_factory_(this) {
  browser_->tab_strip_model()->AddObserver(this);
}

FastUnloadController::~FastUnloadController() {
  browser_->tab_strip_model()->RemoveObserver(this);
  web_contents_waiting_for_deletion_.clear();
}

bool FastUnloadController::CanCloseContents(content::WebContents* contents) {
  // Don't try to close the tab when the whole browser is being closed, since
  // that avoids the fast shutdown path where we just kill all the renderers.
  return !is_attempting_to_close_browser_ ||
      is_calling_before_unload_handlers();
}

bool FastUnloadController::ShouldRunUnloadEventsHelper(
    content::WebContents* contents) {
  // If |contents| is being inspected, devtools needs to intercept beforeunload
  // events.
  return DevToolsWindow::GetInstanceForInspectedWebContents(contents) != NULL;
}

bool FastUnloadController::RunUnloadEventsHelper(
    content::WebContents* contents) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Don't run for extensions that are disabled or uninstalled; the tabs will
  // be killed if they make any network requests, and the extension shouldn't
  // be doing any work if it's removed.
  GURL url = contents->GetLastCommittedURL();
  if (url.SchemeIs(extensions::kExtensionScheme) &&
      !extensions::ExtensionRegistry::Get(browser_->profile())
           ->enabled_extensions()
           .GetExtensionOrAppByURL(url)) {
    return false;
  }
#endif  // (ENABLE_EXTENSIONS)

  // Special case for when we quit an application. The Devtools window can
  // close if it's beforeunload event has already fired which will happen due
  // to the interception of it's content's beforeunload.
  if (browser_->is_devtools() &&
      DevToolsWindow::HasFiredBeforeUnloadEventForDevToolsBrowser(browser_))
    return false;

  // If there's a devtools window attached to |contents|,
  // we would like devtools to call its own beforeunload handlers first,
  // and then call beforeunload handlers for |contents|.
  // See DevToolsWindow::InterceptPageBeforeUnload for details.
  if (DevToolsWindow::InterceptPageBeforeUnload(contents)) {
    return true;
  }
  // If the WebContents is not connected yet, then there's no unload
  // handler we can fire even if the WebContents has an unload listener.
  // One case where we hit this is in a tab that has an infinite loop
  // before load.
  if (contents->NeedToFireBeforeUnload()) {
    // If the page has unload listeners, then we tell the renderer to fire
    // them. Once they have fired, we'll get a message back saying whether
    // to proceed closing the page or not, which sends us back to this method
    // with the NeedToFireBeforeUnload bit cleared.
    contents->DispatchBeforeUnload(false /* auto_cancel */);
    return true;
  }
  return false;
}

bool FastUnloadController::BeforeUnloadFiredForContents(
    content::WebContents* contents,
    bool proceed) {
  if (!proceed)
    DevToolsWindow::OnPageCloseCanceled(contents);

  if (!is_attempting_to_close_browser_) {
    if (!proceed) {
      contents->SetClosedByUserGesture(false);
    } else {
      // No more dialogs are possible, so remove the tab and finish
      // running unload listeners asynchrounously.
      browser_->tab_strip_model()->delegate()->CreateHistoricalTab(contents);
      DetachWebContents(contents);
    }
    return proceed;
  }

  if (!proceed) {
    CancelWindowClose();
    contents->SetClosedByUserGesture(false);
    return false;
  }

  if (tab_needing_before_unload_ack_ == contents) {
    // Now that beforeunload has fired, queue the tab to fire unload.
    tab_needing_before_unload_ack_ = NULL;
    tabs_needing_unload_.insert(contents);
    ProcessPendingTabs(false);
    // We want to handle firing the unload event ourselves since we want to
    // fire all the beforeunload events before attempting to fire the unload
    // events should the user cancel closing the browser.
    return false;
  }

  return true;
}

bool FastUnloadController::ShouldCloseWindow() {
  if (HasCompletedUnloadProcessing())
    return true;

  // Special case for when we quit an application. The Devtools window can
  // close if it's beforeunload event has already fired which will happen due
  // to the interception of it's content's beforeunload.
  if (browser_->is_devtools() &&
      DevToolsWindow::HasFiredBeforeUnloadEventForDevToolsBrowser(browser_)) {
    return true;
  }

  // The behavior followed here varies based on the current phase of the
  // operation and whether a batched shutdown is in progress.
  //
  // If there are tabs with outstanding beforeunload handlers:
  // 1. If a batched shutdown is in progress: return false.
  //    This is to prevent interference with batched shutdown already in
  //    progress.
  // 2. Otherwise: start sending beforeunload events and return false.
  //
  // Otherwise, If there are no tabs with outstanding beforeunload handlers:
  // 3. If a batched shutdown is in progress: start sending unload events and
  //    return false.
  // 4. Otherwise: return true.
  is_attempting_to_close_browser_ = true;
  // Cases 1 and 4.
  bool need_beforeunload_fired = TabsNeedBeforeUnloadFired();
  if (need_beforeunload_fired == is_calling_before_unload_handlers())
    return !need_beforeunload_fired;

  // Cases 2 and 3.
  on_close_confirmed_.Reset();
  ProcessPendingTabs(false);
  return false;
}

bool FastUnloadController::TryToCloseWindow(
    bool skip_beforeunload,
    const base::Callback<void(bool)>& on_close_confirmed) {
  // The devtools browser gets its beforeunload events as the results of
  // intercepting events from the inspected tab, so don't send them here as
  // well.
  if (browser_->is_devtools() || !TabsNeedBeforeUnloadFired())
    return false;

  on_close_confirmed_ = on_close_confirmed;
  is_attempting_to_close_browser_ = true;
  ProcessPendingTabs(skip_beforeunload);
  return !skip_beforeunload;
}

void FastUnloadController::ResetTryToCloseWindow() {
  if (!is_calling_before_unload_handlers())
    return;
  CancelWindowClose();
}

bool FastUnloadController::TabsNeedBeforeUnloadFired() {
  if (!tabs_needing_before_unload_.empty() ||
      tab_needing_before_unload_ack_ != NULL)
    return true;

  if (!is_calling_before_unload_handlers() && !tabs_needing_unload_.empty())
    return false;

  for (int i = 0; i < browser_->tab_strip_model()->count(); ++i) {
    content::WebContents* contents =
        browser_->tab_strip_model()->GetWebContentsAt(i);
    bool should_fire_beforeunload = contents->NeedToFireBeforeUnload() ||
        DevToolsWindow::NeedsToInterceptBeforeUnload(contents);
    if (!ContainsKey(tabs_needing_unload_, contents) &&
        !ContainsKey(tabs_needing_unload_ack_, contents) &&
        tab_needing_before_unload_ack_ != contents &&
        should_fire_beforeunload)
      tabs_needing_before_unload_.insert(contents);
  }
  return !tabs_needing_before_unload_.empty();
}

bool FastUnloadController::HasCompletedUnloadProcessing() const {
  return is_attempting_to_close_browser_ &&
      tabs_needing_before_unload_.empty() &&
      tab_needing_before_unload_ack_ == NULL &&
      tabs_needing_unload_.empty() &&
      tabs_needing_unload_ack_.empty();
}

void FastUnloadController::CancelTabNeedingBeforeUnloadAck() {
  if (tab_needing_before_unload_ack_ != NULL) {
    CoreTabHelper* core_tab_helper =
        CoreTabHelper::FromWebContents(tab_needing_before_unload_ack_);
    core_tab_helper->OnCloseCanceled();
    DevToolsWindow::OnPageCloseCanceled(tab_needing_before_unload_ack_);
    tab_needing_before_unload_ack_ = NULL;
  }
}

void FastUnloadController::CancelWindowClose() {
  // Closing of window can be canceled from a beforeunload handler.
  DCHECK(is_attempting_to_close_browser_);
  tabs_needing_before_unload_.clear();
  CancelTabNeedingBeforeUnloadAck();
  for (auto it = tabs_needing_unload_.begin(); it != tabs_needing_unload_.end();
       it++) {
    content::WebContents* contents = *it;

    CoreTabHelper* core_tab_helper = CoreTabHelper::FromWebContents(contents);
    core_tab_helper->OnCloseCanceled();
    DevToolsWindow::OnPageCloseCanceled(contents);
  }
  tabs_needing_unload_.clear();

  // No need to clear tabs_needing_unload_ack_. Those tabs are already detached.

  if (is_calling_before_unload_handlers()) {
    base::Callback<void(bool)> on_close_confirmed = on_close_confirmed_;
    on_close_confirmed_.Reset();
    on_close_confirmed.Run(false);
  }

  is_attempting_to_close_browser_ = false;

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_BROWSER_CLOSE_CANCELLED,
      content::Source<Browser>(browser_),
      content::NotificationService::NoDetails());
}

////////////////////////////////////////////////////////////////////////////////
// FastUnloadController, content::WebContentsDelegate implementation:

bool FastUnloadController::ShouldSuppressDialogs(content::WebContents* source) {
  return true;
}

void FastUnloadController::CloseContents(content::WebContents* source) {
  auto it = web_contents_waiting_for_deletion_.find(source);
  DCHECK(it != web_contents_waiting_for_deletion_.end());
  web_contents_waiting_for_deletion_.erase(it);
}

////////////////////////////////////////////////////////////////////////////////
// FastUnloadController, content::NotificationObserver implementation:

void FastUnloadController::Observe(
      int type,
      const content::NotificationSource& source,
      const content::NotificationDetails& details) {
  DCHECK_EQ(content::NOTIFICATION_WEB_CONTENTS_DISCONNECTED, type);

  registrar_.Remove(this, content::NOTIFICATION_WEB_CONTENTS_DISCONNECTED,
                    source);
  ClearUnloadState(content::Source<content::WebContents>(source).ptr());
}

////////////////////////////////////////////////////////////////////////////////
// FastUnloadController, TabStripModelObserver implementation:

void FastUnloadController::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (change.type() != TabStripModelChange::kInserted &&
      change.type() != TabStripModelChange::kRemoved &&
      change.type() != TabStripModelChange::kReplaced)
    return;

  for (const auto& delta : change.deltas()) {
    content::WebContents* new_contents = nullptr;
    content::WebContents* old_contents = nullptr;
    if (change.type() == TabStripModelChange::kInserted) {
      new_contents = delta.insert.contents;
    } else if (change.type() == TabStripModelChange::kReplaced) {
      new_contents = delta.replace.new_contents;
      old_contents = delta.replace.old_contents;
    } else {
      old_contents = delta.remove.contents;
    }

    if (old_contents)
      TabDetachedImpl(old_contents);
    if (new_contents)
      TabAttachedImpl(new_contents);
  }
}

void FastUnloadController::TabStripEmpty() {
  // Set is_attempting_to_close_browser_ here, so that extensions, etc, do not
  // attempt to add tabs to the browser before it closes.
  is_attempting_to_close_browser_ = true;
}

////////////////////////////////////////////////////////////////////////////////
// FastUnloadController, private:

void FastUnloadController::TabAttachedImpl(content::WebContents* contents) {
  // If the tab crashes in the beforeunload or unload handler, it won't be
  // able to ack. But we know we can close it.
  registrar_.Add(
      this,
      content::NOTIFICATION_WEB_CONTENTS_DISCONNECTED,
      content::Source<content::WebContents>(contents));
}

void FastUnloadController::TabDetachedImpl(content::WebContents* contents) {
  if (tabs_needing_unload_ack_.find(contents) !=
      tabs_needing_unload_ack_.end()) {
    // Tab needs unload to complete.
    // It will send |NOTIFICATION_WEB_CONTENTS_DISCONNECTED| when done.
    return;
  }

  // If WEB_CONTENTS_DISCONNECTED was received then the notification may have
  // already been unregistered.
  const content::NotificationSource& source =
      content::Source<content::WebContents>(contents);
  if (registrar_.IsRegistered(this,
                              content::NOTIFICATION_WEB_CONTENTS_DISCONNECTED,
                              source)) {
    registrar_.Remove(this,
                      content::NOTIFICATION_WEB_CONTENTS_DISCONNECTED,
                      source);
  }

  if (is_attempting_to_close_browser_)
    ClearUnloadState(contents);
}

bool FastUnloadController::DetachWebContents(content::WebContents* contents) {
  int index = browser_->tab_strip_model()->GetIndexOfWebContents(contents);
  if (index != TabStripModel::kNoTab &&
      contents->NeedToFireBeforeUnload()) {
    tabs_needing_unload_ack_.insert(contents);
    web_contents_waiting_for_deletion_[contents] =
        browser_->tab_strip_model()->DetachWebContentsAt(index);
    contents->SetDelegate(this);
    CoreTabHelper* core_tab_helper = CoreTabHelper::FromWebContents(contents);
    core_tab_helper->OnUnloadDetachedStarted();
    return true;
  }
  return false;
}

void FastUnloadController::ProcessPendingTabs(bool skip_beforeunload) {
  if (!is_attempting_to_close_browser_) {
    // Because we might invoke this after a delay it's possible for the value of
    // is_attempting_to_close_browser_ to have changed since we scheduled the
    // task.
    return;
  }

  if (tab_needing_before_unload_ack_ != NULL) {
    if (skip_beforeunload) {
      // Cancel and skip the ongoing before unload event.
      tabs_needing_before_unload_.insert(tab_needing_before_unload_ack_);
      CancelTabNeedingBeforeUnloadAck();
    } else {
      // Wait for |BeforeUnloadFiredForContents| before proceeding.
      return;
    }
  }

  // Process a beforeunload handler.
  if (!tabs_needing_before_unload_.empty()) {
    if (skip_beforeunload) {
      tabs_needing_unload_.insert(tabs_needing_before_unload_.begin(),
                                  tabs_needing_before_unload_.end());
      tabs_needing_before_unload_.clear();
    } else {
      auto it = tabs_needing_before_unload_.begin();
      content::WebContents* contents = *it;
      tabs_needing_before_unload_.erase(it);
      // Null check render_view_host here as this gets called on a PostTask and
      // the tab's render_view_host may have been nulled out.
      if (contents->GetRenderViewHost()) {
        tab_needing_before_unload_ack_ = contents;

        CoreTabHelper* core_tab_helper =
            CoreTabHelper::FromWebContents(contents);
        core_tab_helper->OnCloseStarted();

        // If there's a devtools window attached to |contents|,
        // we would like devtools to call its own beforeunload handlers first,
        // and then call beforeunload handlers for |contents|.
        // See DevToolsWindow::InterceptPageBeforeUnload for details.
        if (!DevToolsWindow::InterceptPageBeforeUnload(contents))
          contents->DispatchBeforeUnload(false /* auto_cancel */);
      } else {
        ProcessPendingTabs(skip_beforeunload);
      }
      return;
    }
  }

  if (is_calling_before_unload_handlers()) {
    base::OnceCallback<void(bool)> on_close_confirmed = on_close_confirmed_;
    // Reset |on_close_confirmed_| in case the callback tests
    // |is_calling_before_unload_handlers()|, we want to return that calling
    // is complete.
    if (tabs_needing_unload_.empty())
      on_close_confirmed_.Reset();
    if (!skip_beforeunload)
      std::move(on_close_confirmed).Run(true);
    return;
  }

  // Process all the unload handlers. (The beforeunload handlers have finished.)
  if (!tabs_needing_unload_.empty()) {
    browser_->OnWindowClosing();

    // Run unload handlers detached since no more interaction is possible.
    auto it = tabs_needing_unload_.begin();
    while (it != tabs_needing_unload_.end()) {
      auto current = it++;
      content::WebContents* contents = *current;
      tabs_needing_unload_.erase(current);
      // Null check render_view_host here as this gets called on a PostTask
      // and the tab's render_view_host may have been nulled out.
      if (contents->GetRenderViewHost()) {
        CoreTabHelper* core_tab_helper =
            CoreTabHelper::FromWebContents(contents);
        core_tab_helper->OnUnloadStarted();
        DetachWebContents(contents);
        contents->ClosePage();
      }
    }

    // Get the browser hidden.
    if (browser_->tab_strip_model()->empty()) {
      browser_->TabStripEmpty();
    } else {
      browser_->tab_strip_model()->CloseAllTabs();  // tabs not needing unload
    }
    return;
  }

  if (HasCompletedUnloadProcessing()) {
    browser_->OnWindowClosing();

    // Get the browser closed.
    if (browser_->tab_strip_model()->empty()) {
      browser_->TabStripEmpty();
    } else {
      // There may be tabs if the last tab needing beforeunload crashed.
      browser_->tab_strip_model()->CloseAllTabs();
    }
    return;
  }
}

void FastUnloadController::ClearUnloadState(content::WebContents* contents) {
  if (tabs_needing_unload_ack_.erase(contents) > 0) {
    if (HasCompletedUnloadProcessing())
      PostTaskForProcessPendingTabs();
    return;
  }

  if (!is_attempting_to_close_browser_)
    return;

  if (tab_needing_before_unload_ack_ == contents) {
    tab_needing_before_unload_ack_ = NULL;
    PostTaskForProcessPendingTabs();
    return;
  }

  if (tabs_needing_before_unload_.erase(contents) > 0 ||
      tabs_needing_unload_.erase(contents) > 0) {
    if (tab_needing_before_unload_ack_ == NULL)
      PostTaskForProcessPendingTabs();
  }
}

void FastUnloadController::PostTaskForProcessPendingTabs() {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&FastUnloadController::ProcessPendingTabs,
                                weak_factory_.GetWeakPtr(), false));
}
