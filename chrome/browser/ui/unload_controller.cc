// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/unload_controller.h"

#include <algorithm>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/download/download_core_service.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/lifetime/application_lifetime_desktop.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/sessions/session_service_base.h"
#include "chrome/browser/sessions/session_service_lookup.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_live_tab_context.h"
#include "chrome/browser/ui/browser_manager_service.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/browser_window/public/desktop_browser_window_capabilities.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_tabbed_utils.h"
#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_features.h"
#include "components/keep_alive_registry/keep_alive_registry.h"
#include "components/performance_manager/public/execution_context_priority/execution_context_priority.h"
#include "components/sessions/core/session_id.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_interface.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#endif  // (ENABLE_EXTENSIONS)

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/boca/on_task/on_task_locked_controller.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

DEFINE_USER_DATA(UnloadController);

namespace {

// Returns a pair [last_window, last_window_for_profile] indicating if `browser`
// is the only browser in total and for this profile.
// Ignores browsers that are in the process of closing.
std::pair<bool, bool> IsLastWindow(const BrowserWindowInterface* browser) {
  bool last_window = true;
  bool last_window_for_profile = true;
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [&](BrowserWindowInterface* other_browser) {
        // Don't count this browser window or any other in the process of
        // closing. Window closing may be delayed, and windows that are in the
        // process of closing don't count against our totals.
        if (other_browser == browser ||
            other_browser->capabilities()->IsAttemptingToCloseBrowser()) {
          return true;
        }

        last_window = false;

        if (other_browser->GetProfile() == browser->GetProfile()) {
          last_window_for_profile = false;
        }
        return last_window_for_profile;
      });

  return {last_window, last_window_for_profile};
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// UnloadController, public:

// static
UnloadController* UnloadController::From(BrowserWindowInterface* browser) {
  return Get(browser->GetUnownedUserDataHost());
}

// static
const UnloadController* UnloadController::From(
    const BrowserWindowInterface* browser) {
  return Get(browser->GetUnownedUserDataHost());
}

bool UnloadController::HandleBeforeClose() {
  const auto get_closing_status =
      [this]() -> BrowserWindowInterface::ClosingStatus {
    // If `force_skip_warning_user_` is true, then we should immediately
    // return true.
    if (force_skip_warning_user_on_close()) {
      return BrowserWindowInterface::ClosingStatus::kPermitted;
    }

    // If the user needs to see one or more warnings, hold off closing the
    // browser.
    const UnloadController::WarnBeforeClosingResult result =
        MaybeWarnBeforeClosing(base::BindOnce(
            &UnloadController::FinishWarnBeforeClosing, GetWeakPtr()));
    if (result == UnloadController::WarnBeforeClosingResult::kDoNotClose) {
      return BrowserWindowInterface::ClosingStatus::kDeniedByUser;
    }

    return GetBrowserClosingStatus();
  };

  // Notify clients if close was cancelled.
  const BrowserWindowInterface::ClosingStatus close_status =
      get_closing_status();
  const bool close_permitted =
      close_status == BrowserWindowInterface::ClosingStatus::kPermitted;
  if (!close_permitted) {
    NotifyWindowCloseCancelled(close_status);
  }
  return close_permitted;
}

void UnloadController::OnWindowClosing() {
  // There may be situations where async tasks, such as
  // UnloadController::ProcessPendingTabs, may call into OnWindowClosing() after
  // deletion has already been scheduled and closed notifications have been
  // propagated. No-op in such cases to avoid duplicating browser-closed
  // handling.
  if (is_delete_scheduled_) {
    return;
  }

  if (!HandleBeforeClose()) {
    return;
  }

  // Don't use GetForProfileIfExisting here, we want to force creation of the
  // session service so that user can restore what was open.
  SessionServiceBase* service =
      GetAppropriateSessionServiceForProfile(browser_);

  if (service) {
    service->WindowClosing(browser_->GetSessionID());
  }

  sessions::TabRestoreService* tab_restore_service =
      TabRestoreServiceFactory::GetForProfile(browser_->GetProfile());

  const auto browser_type = browser_->GetType();
  bool notify_restore_service =
      (browser_type == BrowserWindowInterface::Type::TYPE_NORMAL) &&
      browser_->GetTabStripModel()->count();
#if defined(USE_AURA) || BUILDFLAG(IS_MAC)
  notify_restore_service |=
      (browser_type == BrowserWindowInterface::Type::TYPE_APP) ||
      (browser_type == BrowserWindowInterface::Type::TYPE_APP_POPUP);
#endif

  if (tab_restore_service && notify_restore_service) {
    tab_restore_service->BrowserClosing(BrowserLiveTabContext::From(browser_));
  }

  if (!browser_->GetTabStripModel()->empty()) {
    // Closing all the tabs results in eventually calling back to
    // OnWindowClosing() again.
    browser_->GetTabStripModel()->CloseAllTabs();
  } else {
    // If there are no tabs, then a task will be scheduled (by views) to delete
    // this Browser.
    OnWindowCloseComplete();
  }
}

UnloadController::UnloadController(BrowserWindowInterface* browser)
    : browser_(browser),
      scoped_unowned_user_data_(browser->GetUnownedUserDataHost(), *this),
      web_contents_collection_(this),
      is_attempting_to_close_browser_(false) {
  browser_->tab_strip_model()->AddObserver(this);
}

void UnloadController::AddTabUnloadHandler(
    std::unique_ptr<TabUnloadHandler> handler) {
  tab_unload_handlers_.push_back(std::move(handler));
}

UnloadController::~UnloadController() {
  browser_->tab_strip_model()->RemoveObserver(this);
}

bool UnloadController::ShouldRunUnloadListenerBeforeClosing(
    content::WebContents* web_contents) {
  return !force_skip_warning_user_on_close_ &&
         ShouldRunUnloadEventsHelper(web_contents);
}

bool UnloadController::RunUnloadListenerBeforeClosing(
    content::WebContents* web_contents) {
  return !force_skip_warning_user_on_close_ &&
         RunUnloadEventsHelper(web_contents);
}

void UnloadController::BeforeUnloadFired(content::WebContents* web_contents,
                                         bool proceed,
                                         bool* proceed_to_fire_unload) {
  if ((browser_->GetType() == BrowserWindowInterface::Type::TYPE_DEVTOOLS) &&
      DevToolsWindow::HandleBeforeUnload(web_contents, proceed,
                                         proceed_to_fire_unload)) {
    return;
  }

  *proceed_to_fire_unload = BeforeUnloadFired(web_contents, proceed);
}

bool UnloadController::CanCloseContents(content::WebContents* contents) {
  // Don't try to close the tab when the whole browser is being closed, since
  // that avoids the fast shutdown path where we just kill all the renderers.
  if (is_attempting_to_close_browser_) {
    ClearUnloadState(contents, true);
  }

  if (!web_app::IsTabClosable(
          browser_->tab_strip_model(),
          browser_->tab_strip_model()->GetIndexOfWebContents(contents))) {
    return false;
  }

#if BUILDFLAG(IS_CHROMEOS)
  // Tabs cannot be closed when the app is locked for OnTask. Only relevant for
  // non-web browser scenarios.
  if (ash::boca::OnTaskLockedController::From(browser_)
          ->is_locked_for_on_task()) {
    return false;
  }
#endif

  return !is_attempting_to_close_browser_ ||
         is_calling_before_unload_handlers();
}

bool UnloadController::ShouldRunUnloadEventsHelper(
    content::WebContents* contents) {
  bool should_show_custom_confirmation = false;
  for (const auto& handler : tab_unload_handlers_) {
    if (handler->ShouldSkipBeforeUnload(contents)) {
      return false;
    }
    if (handler->ShouldShowCustomConfirmation(contents)) {
      should_show_custom_confirmation = true;
    }
  }
  // If |contents| is being inspected, devtools needs to intercept beforeunload
  // events.
  if (DevToolsWindow::GetInstanceForInspectedWebContents(contents) != nullptr) {
    return true;
  }
  return should_show_custom_confirmation;
}

bool UnloadController::RunUnloadEventsHelper(content::WebContents* contents) {
  // If a tab unload handler indicates that beforeunload handling should be
  // skipped (for example, because the user already confirmed closing the tab
  // via a custom task confirmation dialog), return false immediately. This
  // check must take precedence over all other checks so that once the user
  // confirms closure, the tab closes directly without showing duplicate alerts
  // or triggering standard website beforeunload handlers.
  TabUnloadHandler* handler_to_show = nullptr;
  for (const auto& handler : tab_unload_handlers_) {
    if (handler->ShouldSkipBeforeUnload(contents)) {
      return false;
    }
    if (!handler_to_show && handler->ShouldShowCustomConfirmation(contents)) {
      handler_to_show = handler.get();
    }
  }

  if (handler_to_show) {
    if (handler_to_show->ShowCustomConfirmation(
            contents,
            base::BindOnce(&UnloadController::OnCustomConfirmationClosed,
                           weak_factory_.GetWeakPtr(),
                           contents->GetWeakPtr()))) {
      return true;
    }
  }
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Don't run for extensions that are disabled or uninstalled; the tabs will
  // be killed if they make any network requests, and the extension shouldn't
  // be doing any work if it's removed.
  GURL url = contents->GetLastCommittedURL();
  if (url.SchemeIs(extensions::kExtensionScheme) &&
      !extensions::ExtensionRegistry::Get(browser_->GetProfile())
           ->enabled_extensions()
           .GetExtensionOrAppByURL(url)) {
    return false;
  }
#endif  // (ENABLE_EXTENSIONS)

  // Special case for when we quit an application. The devtools window can
  // close if it's beforeunload event has already fired which will happen due
  // to the interception of it's content's beforeunload.
  if (browser_->GetType() == BrowserWindowInterface::Type::TYPE_DEVTOOLS &&
      DevToolsWindow::HasFiredBeforeUnloadEventForDevToolsBrowser(browser_)) {
    return false;
  }

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
    // Inform PerformanceManager that the page is closing, so the priority of
    // its frames is boosted while beforeunload/unload handlers are running,
    // making page closing faster. This state may be reset in
    // BeforeUnloadFired() if page closing is aborted.
    performance_manager::execution_context_priority::SetPageIsClosing(
        contents, /*is_closing=*/true);

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

    // Inform PerformanceManager that page close was aborted. Any priority boost
    // will be removed.
    performance_manager::execution_context_priority::SetPageIsClosing(
        contents, /*is_closing=*/false);

    std::optional<tab_groups::TabGroupId> group =
        browser_->tab_strip_model()->GetTabGroupForTab(
            browser_->tab_strip_model()->GetIndexOfWebContents(contents));
    if (group.has_value()) {
      TabGroup* const tab_group =
          browser_->tab_strip_model()->group_model()->GetTabGroup(
              group.value());
      if (tab_group->IsGroupClosing()) {
        browser_->tab_strip_model()->GroupCloseStopped(group.value());
      }
    }
  }

  if (!is_attempting_to_close_browser_) {
    if (!proceed) {
      contents->SetClosedByUserGesture(false);
    }
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

BrowserWindowInterface::ClosingStatus
UnloadController::GetBrowserClosingStatus() {
  if (IsUnclosableApp()) {
    return BrowserWindowInterface::ClosingStatus::kDeniedByPolicy;
  }

  if (HasCompletedUnloadProcessing()) {
    return BrowserWindowInterface::ClosingStatus::kPermitted;
  }

  // Special case for when we quit an application. The devtools window can
  // close if it's beforeunload event has already fired which will happen due
  // to the interception of it's content's beforeunload.
  if (browser_->GetType() == BrowserWindowInterface::Type::TYPE_DEVTOOLS &&
      DevToolsWindow::HasFiredBeforeUnloadEventForDevToolsBrowser(browser_)) {
    return BrowserWindowInterface::ClosingStatus::kPermitted;
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
               ? BrowserWindowInterface::ClosingStatus::
                     kDeniedUnloadHandlersNeedTime
               : BrowserWindowInterface::ClosingStatus::kPermitted;
  }

  // Cases 2 and 3.
  on_close_confirmed_.Reset();
  ProcessPendingTabs(false);
  return BrowserWindowInterface::ClosingStatus::kDeniedUnloadHandlersNeedTime;
}

bool UnloadController::TryToCloseWindow(
    bool skip_beforeunload,
    const base::RepeatingCallback<void(bool)>& on_close_confirmed) {
  cancel_download_confirmation_state_ =
      CancelDownloadConfirmationState::kResponseReceived;
  // The devtools browser gets its beforeunload events as the results of
  // intercepting events from the inspected tab, so don't send them here as
  // well.
  if (browser_->GetType() == BrowserWindowInterface::Type::TYPE_DEVTOOLS ||
      HasCompletedUnloadProcessing()) {
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
  cancel_download_confirmation_state_ =
      CancelDownloadConfirmationState::kNotPrompted;
  if (!is_calling_before_unload_handlers()) {
    return;
  }
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
    if (!tabs_needing_unload_fired_.contains(contents) &&
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
  for (const auto& it : tabs_needing_unload_fired_) {
    DevToolsWindow::OnPageCloseCanceled(it);
  }
  tabs_needing_unload_fired_.clear();
  if (is_calling_before_unload_handlers()) {
    std::move(on_close_confirmed_).Run(false);
  }
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
    for (const auto& contents : change.GetInsert()->contents) {
      new_contents.push_back(contents.contents);
    }
  } else if (change.type() == TabStripModelChange::kReplaced) {
    new_contents.push_back(change.GetReplace()->new_contents);
    old_contents.push_back(change.GetReplace()->old_contents);
  } else if (change.type() == TabStripModelChange::kRemoved) {
    for (const auto& contents : change.GetRemove()->contents) {
      old_contents.push_back(contents.contents);
    }
  }

  for (auto* contents : old_contents) {
    TabDetachedImpl(contents);
  }
  for (auto* contents : new_contents) {
    TabAttachedImpl(contents);
  }
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
  if (is_attempting_to_close_browser_) {
    ClearUnloadState(contents, false);
  }
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
      UnloadController::From(browser_)->OnWindowClosing();
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
      if (!DevToolsWindow::InterceptPageBeforeUnload(web_contents)) {
        // Inform PerformanceManager that the page is closing, so the priority
        // of its frames is boosted while beforeunload/unload handlers are
        // running, making page closing faster. This state may be reset in
        // BeforeUnloadFired() if page closing is aborted.
        performance_manager::execution_context_priority::SetPageIsClosing(
            web_contents, /*is_closing=*/true);

        web_contents->DispatchBeforeUnload(false /* auto_cancel */);
      }
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
    if (tabs_needing_unload_fired_.empty()) {
      on_close_confirmed_.Reset();
    }
    if (!skip_beforeunload) {
      on_close_confirmed.Run(true);
    }
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

  auto iter = std::ranges::find(*set, web_contents);
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
      if (weak_factory_.HasWeakPtrs()) {
        return;
      }
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&UnloadController::ProcessPendingTabs,
                                    weak_factory_.GetWeakPtr(), false));
    }
  }
}

void UnloadController::OnCustomConfirmationClosed(
    base::WeakPtr<content::WebContents> web_contents,
    bool confirmed) {
  if (!web_contents) {
    return;
  }
  if (confirmed) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](base::WeakPtr<UnloadController> controller,
               base::WeakPtr<content::WebContents> web_contents) {
              if (!controller || !web_contents) {
                return;
              }
              // Retrieve the tab host browser via TabInterface. This works both
              // when the tab remains in its original browser window and when it
              // has been dragged to another window while the confirmation
              // dialog was open.
              BrowserWindowInterface* browser = nullptr;
              if (tabs::TabInterface* tab =
                      tabs::TabInterface::GetFromContents(web_contents.get())) {
                browser = tab->GetBrowserWindowInterface();
              }
              if (browser && browser->GetTabStripModel()) {
                if (browser->GetTabStripModel()->GetIndexOfWebContents(
                        web_contents.get()) != TabStripModel::kNoTab) {
                  // Note: Once the user has confirmed once via the custom
                  // confirmation dialog, the tab closes directly without any
                  // additional prompts.
                  browser->GetTabStripModel()->CloseWebContents(
                      web_contents.get(), TabCloseTypes::CLOSE_USER_GESTURE);
                }
              }
            },
            weak_factory_.GetWeakPtr(), web_contents));
  } else {
    BeforeUnloadFired(web_contents.get(), false);
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
  auto* const app_controller = web_app::AppBrowserController::From(browser_);
  return web_app::WebAppProvider::GetForWebContents(active_web_contents)
      ->policy_manager()
      .IsPreventCloseEnabled(app_controller->app_id());
}

UnloadController::WarnBeforeClosingResult
UnloadController::MaybeWarnBeforeClosing(
    WarnBeforeClosingCallback warn_callback) {
  // If the browser can close right away (we've indicated that we want to skip
  // before-unload handlers by setting `force_skip_warning_user_on_close_` to
  // true or there are no pending downloads we need to prompt about) then
  // there's no need to warn.
  if (force_skip_warning_user_on_close()) {
    return WarnBeforeClosingResult::kOkToClose;
  }

  // `CanCloseWithInProgressDownloads()` may trigger a modal dialog.
  bool can_close_with_downloads = CanCloseWithInProgressDownloads();
  if (can_close_with_downloads) {
    return WarnBeforeClosingResult::kOkToClose;
  }

  DCHECK(!warn_before_closing_callback_)
      << "Tried to close window during close warning; dialog should be modal.";
  warn_before_closing_callback_ = std::move(warn_callback);

  return WarnBeforeClosingResult::kDoNotClose;
}

UnloadController::DownloadCloseType
UnloadController::OkToCloseWithInProgressDownloads(
    int* num_downloads_blocking) const {
  DCHECK(num_downloads_blocking);
  *num_downloads_blocking = 0;

  // If we're not running a full browser process with a profile manager
  // (testing), it's ok to close the browser.
  if (!g_browser_process->profile_manager()) {
    return DownloadCloseType::kOk;
  }

  int total_download_count =
      DownloadCoreService::BlockingShutdownCountAllProfiles();
  if (total_download_count == 0) {
    return DownloadCloseType::kOk;  // No downloads; can definitely close.
  }

  // Figure out how many windows are open total, and associated with this
  // profile, that are relevant for the ok-to-close decision.
  auto [last_window, last_window_for_profile] = IsLastWindow(browser_);

  // If there aren't any other windows, we're at browser shutdown,
  // which would cancel all current downloads.
  if (last_window) {
    *num_downloads_blocking = total_download_count;
    return DownloadCloseType::kBrowserShutdown;
  }

  // If there aren't any other windows on our profile, and we're an Incognito
  // or Guest profile, and there are downloads associated with that profile,
  // those downloads would be cancelled by our window (-> profile) close.
  // The profile's DownloadCoreService may already be torn down (e.g. during
  // OTR profile shutdown), in which case there's nothing left to block on.
  DownloadCoreService* download_core_service =
      DownloadCoreServiceFactory::GetForBrowserContext(browser_->GetProfile());
  if (last_window_for_profile && download_core_service &&
      (download_core_service->BlockingShutdownCount() > 0) &&
      (browser_->GetProfile()->IsIncognitoProfile() ||
       browser_->GetProfile()->IsGuestSession())) {
    *num_downloads_blocking = download_core_service->BlockingShutdownCount();
    return browser_->GetProfile()->IsGuestSession()
               ? DownloadCloseType::kLastWindowInGuestSession
               : DownloadCloseType::kLastWindowInIncognitoProfile;
  }

  // Those are the only conditions under which we will block shutdown.
  return DownloadCloseType::kOk;
}

bool UnloadController::CanCloseWithInProgressDownloads() {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)
  // On Mac and ChromeOS, non-incognito and non-Guest downloads can still
  // continue after window is closed.
  if (!browser_->GetProfile()->IsOffTheRecord()) {
    return true;
  }
#endif

  // If we've prompted, we need to hear from the user before we
  // can close.
  if (cancel_download_confirmation_state_ !=
      CancelDownloadConfirmationState::kNotPrompted) {
    return cancel_download_confirmation_state_ !=
           CancelDownloadConfirmationState::kWaitingForResponse;
  }

  int num_downloads_blocking;
  DownloadCloseType dialog_type =
      OkToCloseWithInProgressDownloads(&num_downloads_blocking);
  if (dialog_type == DownloadCloseType::kOk) {
    return true;
  }

  // Closing this window will kill some downloads; prompt to make sure
  // that's ok.
  cancel_download_confirmation_state_ =
      CancelDownloadConfirmationState::kWaitingForResponse;
  BrowserWindow::FromBrowser(browser_)->ConfirmBrowserCloseWithPendingDownloads(
      num_downloads_blocking, dialog_type,
      base::BindOnce(&UnloadController::InProgressDownloadResponse,
                     weak_factory_.GetWeakPtr()));

  // Return false so the browser does not close.  We'll close if the user
  // confirms in the dialog.
  return false;
}

base::WeakPtr<UnloadController> UnloadController::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void UnloadController::InProgressDownloadResponse(bool cancel_downloads) {
  if (cancel_downloads) {
    cancel_download_confirmation_state_ =
        CancelDownloadConfirmationState::kResponseReceived;
    std::move(warn_before_closing_callback_)
        .Run(WarnBeforeClosingResult::kOkToClose);
    return;
  }

  // Sets the confirmation state to
  // CancelDownloadConfirmationState::kNotPrompted so that if the user tries to
  // close again we'll show the warning again.
  cancel_download_confirmation_state_ =
      CancelDownloadConfirmationState::kNotPrompted;

  // Show the download page so the user can figure-out what downloads are still
  // in-progress.
  chrome::ShowDownloads(browser_);

  std::move(warn_before_closing_callback_)
      .Run(WarnBeforeClosingResult::kDoNotClose);
}

void UnloadController::FinishWarnBeforeClosing(WarnBeforeClosingResult result) {
  switch (result) {
    case WarnBeforeClosingResult::kOkToClose:
      chrome::CloseWindow(browser_);
      break;
    case WarnBeforeClosingResult::kDoNotClose:
      // Reset UnloadController::is_attempting_to_close_browser_ so that we
      // don't prompt every time any tab is closed. http://crbug.com/40336263
      CancelWindowClose();
  }
}

void UnloadController::NotifyWindowCloseCancelled(
    BrowserWindowInterface::ClosingStatus status) {
  browser_close_cancelled_callback_list_.Notify(browser_, status);
}

void UnloadController::OnWindowCloseComplete() {
  // If there are no tabs, then a task will be scheduled (by views) to delete
  // this Browser.
  is_delete_scheduled_ = true;

  // At this point the browser has successfully closed and is scheduled for
  // deletion.
  browser_did_close_callback_list_.Notify(browser_);

  // Application should shutdown on last window close if the user is
  // explicitly trying to quit, or if there is nothing keeping the browser
  // alive (such as AppController on the Mac, or BackgroundContentsService for
  // background pages).
  const bool should_quit_if_last_browser =
      browser_shutdown::IsTryingToQuit() ||
      KeepAliveRegistry::GetInstance()->IsKeepingAliveOnlyByBrowserOrigin();

  // Below will not consider browsers for which delete has already been
  // scheduled.
  const bool is_last_browser =
      !GetLastActiveBrowserWindowInterfaceWithAnyProfile();

  if (should_quit_if_last_browser && is_last_browser) {
    browser_shutdown::OnShutdownStarting(
        browser_shutdown::ShutdownType::kWindowClose);
  }

  // Once a Browser has successfully closed, client code expects control to
  // return to the run loop before the instance is finally deleted. To
  // maintain existing expectations schedule the delete asynchronously here.
  // TODO(crbug.com/413168662): Explore synchronously destroying the browser
  // instead.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<BrowserWindowInterface> browser) {
            if (browser) {
              BrowserManagerService::SynchronouslyDestroyBrowser(browser.get());
            }
          },
          browser_->GetWeakPtr()));
}

base::CallbackListSubscription UnloadController::RegisterBrowserDidClose(
    BrowserWindowInterface::BrowserDidCloseCallback callback) {
  return browser_did_close_callback_list_.Add(std::move(callback));
}

base::CallbackListSubscription UnloadController::RegisterBrowserCloseCancelled(
    BrowserWindowInterface::BrowserCloseCancelledCallback callback) {
  return browser_close_cancelled_callback_list_.Add(std::move(callback));
}
