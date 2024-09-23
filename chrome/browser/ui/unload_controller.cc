// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/unload_controller.h"

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/ranges/algorithm.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/lifetime/application_lifetime_desktop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_tabbed_utils.h"
#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#endif  // (ENABLE_EXTENSIONS)

////////////////////////////////////////////////////////////////////////////////
// UnloadController, public:

UnloadController::UnloadController(Browser* browser)
    : browser_(browser),
      web_contents_collection_(this),
      is_attempting_to_close_browser_(false) {
  browser_->tab_strip_model()->AddObserver(this);
}

UnloadController::~UnloadController() {
  browser_->tab_strip_model()->RemoveObserver(this);
}

bool UnloadController::CanCloseContents(content::WebContents* contents) {
  // Don't try to close the tab when the whole browser is being closed, since
  // that avoids the fast shutdown path where we just kill all the renderers.
  if (is_attempting_to_close_browser_)
    ClearUnloadState(contents, true);

  if (!web_app::IsTabClosable(
          browser_->tab_strip_model(),
          browser_->tab_strip_model()->GetIndexOfWebContents(contents))) {
    return false;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Tabs cannot be closed when the app is locked for OnTask. Only relevant for
  // non-web browser scenarios.
  if (browser_->IsLockedForOnTask()) {
    return false;
  }
#endif

  return !is_attempting_to_close_browser_ ||
         is_calling_before_unload_handlers();
}

bool UnloadController::ShouldRunUnloadEventsHelper(
    content::WebContents* contents) {
  // If |contents| is being inspected, devtools needs to intercept beforeunload
  // events.
  return DevToolsWindow::GetInstanceForInspectedWebContents(contents) !=
         nullptr;
}

bool UnloadController::RunUnloadEventsHelper(content::WebContents* contents) {
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

  // Special case for when we quit an application. The devtools window can
  // close if it's beforeunload event has already fired which will happen due
  // to the interception of it's content's beforeunload.
  if (browser_->is_type_devtools() &&
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
  if (contents->NeedToFireBeforeUnloadOrUnloadEvents()) {
    // If the page has unload listeners, then we tell the renderer to fire
    // them. Once they have fired, we'll get a message back saying whether
    // to proceed closing the page or not, which sends us back to this method
    // with the NeedToFireBeforeUnloadOrUnloadEvents bit cleared.
    contents->DispatchBeforeUnload(false /* auto_cancel */);
    return true;
  }
  return false;
}

bool UnloadController::BeforeUnloadFired(content::WebContents* contents,
                                         bool proceed) {
  if (!proceed) {
    DevToolsWindow::OnPageCloseCanceled(contents);
    std::optional<tab_groups::TabGroupId> group =
        browser_->tab_strip_model()->GetTabGroupForTab(
            browser_->tab_strip_model()->GetIndexOfWebContents(contents));
    if (group.has_value())
      browser_->tab_strip_model()->GroupCloseStopped(group.value());
  }

  if (!is_attempting_to_close_browser_) {
    if (!proceed)
      contents->SetClosedByUserGesture(false);
    return proceed;
  }

  if (!proceed) {
    CancelWindowClose();
    contents->SetClosedByUserGesture(false);
    return false;
  }

  if (RemoveFromSet(&tabs_needing_before_unload_fired_, contents)) {
    // Now that beforeunload has fired, put the tab on the queue to fire
    // unload.
    tabs_needing_unload_fired_.insert(contents);
    ProcessPendingTabs(false);
    // We want to handle firing the unload event ourselves since we want to
    // fire all the beforeunload events before attempting to fire the unload
    // events should the user cancel closing the browser.
    return false;
  }

  return true;
}

BrowserClosingStatus UnloadController::GetBrowserClosingStatus() {
  if (IsUnclosableApp()) {
    return BrowserClosingStatus::kDeniedByPolicy;
  }

  if (HasCompletedUnloadProcessing()) {
    return BrowserClosingStatus::kPermitted;
  }

  // Special case for when we quit an application. The devtools window can
  // close if it's beforeunload event has already fired which will happen due
  // to the interception of it's content's beforeunload.
  if (browser_->is_type_devtools() &&
      DevToolsWindow::HasFiredBeforeUnloadEventForDevToolsBrowser(browser_)) {
    return BrowserClosingStatus::kPermitted;
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
  tabs_needing_before_unload_fired_ = GetTabsNeedingBeforeUnloadFired();

  bool need_beforeunload_fired = !tabs_needing_before_unload_fired_.empty();
  if (need_beforeunload_fired == is_calling_before_unload_handlers()) {
    return need_beforeunload_fired
               ? BrowserClosingStatus::kDeniedUnloadHandlersNeedTime
               : BrowserClosingStatus::kPermitted;
  }

  // Cases 2 and 3.
  on_close_confirmed_.Reset();
  ProcessPendingTabs(false);
  return BrowserClosingStatus::kDeniedUnloadHandlersNeedTime;
}

bool UnloadController::TryToCloseWindow(
    bool skip_beforeunload,
    const base::RepeatingCallback<void(bool)>& on_close_confirmed) {
  // The devtools browser gets its beforeunload events as the results of
  // intercepting events from the inspected tab, so don't send them here as
  // well.
  if (browser_->is_type_devtools() || HasCompletedUnloadProcessing()) {
    return false;
  }

  tabs_needing_before_unload_fired_ = GetTabsNeedingBeforeUnloadFired();
  if (tabs_needing_before_unload_fired_.empty()) {
    return false;
  }

  is_attempting_to_close_browser_ = true;
  on_close_confirmed_ = on_close_confirmed;

  ProcessPendingTabs(skip_beforeunload);
  return !skip_beforeunload;
}

void UnloadController::ResetTryToCloseWindow() {
  if (!is_calling_before_unload_handlers())
    return;
  CancelWindowClose();
}

bool UnloadController::TabsNeedBeforeUnloadFired() const {
  return !GetTabsNeedingBeforeUnloadFired().empty();
}

UnloadController::UnloadListenerSet
UnloadController::GetTabsNeedingBeforeUnloadFired() const {
  if (!is_attempting_to_close_browser_) {
    CHECK(tabs_needing_unload_fired_.empty());
  }

  UnloadListenerSet tabs_needing_beforeunload;
  for (int i = 0; i < browser_->tab_strip_model()->count(); ++i) {
    content::WebContents* const contents =
        browser_->tab_strip_model()->GetWebContentsAt(i);
    const bool should_fire_beforeunload =
        contents->NeedToFireBeforeUnloadOrUnloadEvents() ||
        DevToolsWindow::NeedsToInterceptBeforeUnload(contents);
    // Note that we filter out tabs in `tabs_needing_unload_fired_` as they have
    // already had their BeforeUnload fired (and don't need it fired again
    // unless browser closing gets cancelled).
    if (!base::Contains(tabs_needing_unload_fired_, contents) &&
        should_fire_beforeunload) {
      tabs_needing_beforeunload.insert(contents);
    }
  }
  return tabs_needing_beforeunload;
}

void UnloadController::CancelWindowClose() {
  // Note that this method may be called if closing was canceled in a number of
  // different ways, so is_attempting_to_close_browser_ may be false. In that
  // case some of this code might not have an effect, but it's still useful to,
  // for example, call the notification(s).
  tabs_needing_before_unload_fired_.clear();
  for (auto it = tabs_needing_unload_fired_.begin();
       it != tabs_needing_unload_fired_.end(); ++it) {
    DevToolsWindow::OnPageCloseCanceled(*it);
  }
  tabs_needing_unload_fired_.clear();
  if (is_calling_before_unload_handlers())
    std::move(on_close_confirmed_).Run(false);
  is_attempting_to_close_browser_ = false;

  chrome::OnClosingAllBrowsers(false);
}

////////////////////////////////////////////////////////////////////////////////
// UnloadController, WebContentsCollection::Observer implementation:

void UnloadController::RenderProcessGone(content::WebContents* web_contents,
                                         base::TerminationStatus status) {
  if (is_attempting_to_close_browser_) {
    ClearUnloadState(web_contents,
                     false);  // See comment for ClearUnloadState().
  }
  web_contents_collection_.StopObserving(web_contents);
}

////////////////////////////////////////////////////////////////////////////////
// UnloadController, TabStripModelObserver implementation:

void UnloadController::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  std::vector<content::WebContents*> new_contents;
  std::vector<content::WebContents*> old_contents;

  if (change.type() == TabStripModelChange::kInserted) {
    for (const auto& contents : change.GetInsert()->contents)
      new_contents.push_back(contents.contents);
  } else if (change.type() == TabStripModelChange::kReplaced) {
    new_contents.push_back(change.GetReplace()->new_contents);
    old_contents.push_back(change.GetReplace()->old_contents);
  } else if (change.type() == TabStripModelChange::kRemoved) {
    for (const auto& contents : change.GetRemove()->contents)
      old_contents.push_back(contents.contents);
  }

  for (auto* contents : old_contents)
    TabDetachedImpl(contents);
  for (auto* contents : new_contents)
    TabAttachedImpl(contents);
}

void UnloadController::TabStripEmpty() {
  // Set is_attempting_to_close_browser_ here, so that extensions, etc, do not
  // attempt to add tabs to the browser before it closes.
  is_attempting_to_close_browser_ = true;
}

////////////////////////////////////////////////////////////////////////////////
// UnloadController, private:

void UnloadController::TabAttachedImpl(content::WebContents* contents) {
  // If the tab crashes in the beforeunload or unload handler, it won't be
  // able to ack. But we know we can close it.
  web_contents_collection_.StartObserving(contents);
}

void UnloadController::TabDetachedImpl(content::WebContents* contents) {
  if (is_attempting_to_close_browser_)
    ClearUnloadState(contents, false);
  // TODO(crbug.com/40054609): This CHECK is only in place to diagnose a UAF
  // bug. This is both used to confirm that a WebContents* isn't being removed
  // from this set, and also if that hypothesis is correct turns a UAF into a
  // non-security crash.
  CHECK(tabs_needing_before_unload_fired_.find(contents) ==
        tabs_needing_before_unload_fired_.end());
  web_contents_collection_.StopObserving(contents);
}

void UnloadController::ProcessPendingTabs(bool skip_beforeunload) {
  // Cancel posted/queued ProcessPendingTabs task if there is any.
  weak_factory_.InvalidateWeakPtrs();

  if (!is_attempting_to_close_browser_) {
    // Because we might invoke this after a delay it's possible for the value of
    // is_attempting_to_close_browser_ to have changed since we scheduled the
    // task.
    return;
  }

  if (HasCompletedUnloadProcessing()) {
    tabs_needing_before_unload_fired_ = GetTabsNeedingBeforeUnloadFired();
    if (tabs_needing_before_unload_fired_.empty()) {
      // We've finished all the unload events and can proceed to close the
      // browser.
      browser_->OnWindowClosing();
      return;
    }
  }

  if (skip_beforeunload) {
    tabs_needing_unload_fired_.insert(tabs_needing_before_unload_fired_.begin(),
                                      tabs_needing_before_unload_fired_.end());
    tabs_needing_before_unload_fired_.clear();
  }

  // Process beforeunload tabs first. When that queue is empty, process
  // unload tabs.
  if (!tabs_needing_before_unload_fired_.empty()) {
    content::WebContents* const web_contents =
        *(tabs_needing_before_unload_fired_.begin());
    // Null check render_view_host here as this gets called on a PostTask and
    // the tab's render_view_host may have been nulled out.
    if (web_contents->GetPrimaryMainFrame()->GetRenderViewHost()) {
      // If there's a devtools window attached to |web_contents|,
      // we would like devtools to call its own beforeunload handlers first,
      // and then call beforeunload handlers for |web_contents|.
      // See DevToolsWindow::InterceptPageBeforeUnload for details.
      if (!DevToolsWindow::InterceptPageBeforeUnload(web_contents))
        web_contents->DispatchBeforeUnload(false /* auto_cancel */);
    } else {
      ClearUnloadState(web_contents, true);
    }
    return;
  }
  if (is_calling_before_unload_handlers()) {
    base::RepeatingCallback<void(bool)> on_close_confirmed =
        on_close_confirmed_;
    // Reset |on_close_confirmed_| in case the callback tests
    // |is_calling_before_unload_handlers()|, we want to return that calling
    // is complete.
    if (tabs_needing_unload_fired_.empty())
      on_close_confirmed_.Reset();
    if (!skip_beforeunload)
      on_close_confirmed.Run(true);
    return;
  }
  CHECK(!tabs_needing_unload_fired_.empty());
  // We've finished firing all beforeunload events and can proceed with unload
  // events.
  // TODO(ojan): We should add a call to browser_shutdown::OnShutdownStarting
  // somewhere around here so that we have accurate measurements of shutdown
  // time.
  // TODO(ojan): We can probably fire all the unload events in parallel and
  // get a perf benefit from that in the cases where the tab hangs in it's
  // unload handler or takes a long time to page in.
  content::WebContents* const web_contents =
      *(tabs_needing_unload_fired_.begin());
  // Null check render_view_host here as this gets called on a PostTask and
  // the tab's render_view_host may have been nulled out.
  if (web_contents->GetPrimaryMainFrame()->GetRenderViewHost()) {
    web_contents->ClosePage();
  } else {
    ClearUnloadState(web_contents, true);
  }
}

bool UnloadController::HasCompletedUnloadProcessing() const {
  return is_attempting_to_close_browser_ &&
         tabs_needing_before_unload_fired_.empty() &&
         tabs_needing_unload_fired_.empty();
}

bool UnloadController::RemoveFromSet(UnloadListenerSet* set,
                                     content::WebContents* web_contents) {
  DCHECK(is_attempting_to_close_browser_);

  auto iter = base::ranges::find(*set, web_contents);
  if (iter != set->end()) {
    set->erase(iter);
    return true;
  }
  return false;
}

void UnloadController::ClearUnloadState(content::WebContents* web_contents,
                                        bool process_now) {
  if (is_attempting_to_close_browser_) {
    RemoveFromSet(&tabs_needing_before_unload_fired_, web_contents);
    RemoveFromSet(&tabs_needing_unload_fired_, web_contents);
    if (process_now) {
      ProcessPendingTabs(false);
    } else {
      // Do not post a new task if there is already any.
      if (weak_factory_.HasWeakPtrs())
        return;
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&UnloadController::ProcessPendingTabs,
                                    weak_factory_.GetWeakPtr(), false));
    }
  }
}

bool UnloadController::IsUnclosableApp() const {
  if (!web_app::AppBrowserController::IsWebApp(browser_.get())) {
    return false;
  }

  content::WebContents* const active_web_contents =
      browser_->tab_strip_model()->GetActiveWebContents();
  if (!active_web_contents) {
    return false;
  }
  return web_app::WebAppProvider::GetForWebContents(active_web_contents)
      ->policy_manager()
      .IsPreventCloseEnabled(browser_->app_controller()->app_id());
}
