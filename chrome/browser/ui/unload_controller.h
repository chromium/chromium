// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_UNLOAD_CONTROLLER_H_
#define CHROME_BROWSER_UI_UNLOAD_CONTROLLER_H_

#include <memory>
#include <set>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/tab_contents/web_contents_collection.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class Browser;
class TabStripModel;

namespace content {
class WebContents;
}  // namespace content

class UnloadController : public WebContentsCollection::Observer,
                         public TabStripModelObserver {
 public:
  // Represents the result of the user being warned before closing the browser.
  // See WarnBeforeClosingCallback and WarnBeforeClosing() below.
  enum class WarnBeforeClosingResult { kOkToClose, kDoNotClose };

  // Callback that receives the result of a user being warned about closing a
  // browser window (for example, if closing the window would interrupt a
  // download). The parameter is whether the close should proceed.
  using WarnBeforeClosingCallback =
      base::OnceCallback<void(WarnBeforeClosingResult)>;

  // The context for a download blocked notification from
  // OkToCloseWithInProgressDownloads.
  enum class DownloadCloseType {
    // Browser close is not blocked by download state.
    kOk,

    // The browser is shutting down and there are active downloads
    // that would be cancelled.
    kBrowserShutdown,

    // There are active downloads associated with this incognito profile
    // that would be canceled.
    kLastWindowInIncognitoProfile,

    // There are active downloads associated with this guest session
    // that would be canceled.
    kLastWindowInGuestSession,
  };

  DECLARE_USER_DATA(UnloadController);

  // Interface for custom handlers that intercept tab close events. This allows
  // background task systems (such as active automated agents or tools) to warn
  // the user before a tab actively running a task is unloaded.
  class TabUnloadHandler {
   public:
    virtual ~TabUnloadHandler() = default;

    // Returns true if standard beforeunload handling should be skipped for this
    // tab (e.g., when a custom confirmation dialog or background task manages
    // it).
    virtual bool ShouldSkipBeforeUnload(content::WebContents* contents) = 0;

    // Returns true if a custom confirmation dialog should be displayed before
    // unloading this tab.
    virtual bool ShouldShowCustomConfirmation(
        content::WebContents* contents) = 0;

    // Displays the custom confirmation dialog. Returns true if the confirmation
    // dialog was shown and will intercept unload.
    // `on_closed` is invoked with true if the user confirmed closing the tab.
    virtual bool ShowCustomConfirmation(
        content::WebContents* contents,
        base::OnceCallback<void(bool /* confirmed */)> on_closed) = 0;
  };

  explicit UnloadController(BrowserWindowInterface* browser);

  void AddTabUnloadHandler(std::unique_ptr<TabUnloadHandler> handler);
  bool HasTabUnloadHandlers() const { return !tab_unload_handlers_.empty(); }
  const std::vector<std::unique_ptr<TabUnloadHandler>>&
  tab_unload_handlers_for_testing() const {
    return tab_unload_handlers_;
  }

  static UnloadController* From(BrowserWindowInterface* browser);
  static const UnloadController* From(const BrowserWindowInterface* browser);

  UnloadController(const UnloadController&) = delete;
  UnloadController& operator=(const UnloadController&) = delete;

  ~UnloadController() override;

  // Returns true if |contents| can be cleanly closed. When |browser_| is being
  // closed, this function will return false to indicate |contents| should not
  // be cleanly closed, since the fast shutdown path will just kill its
  // renderer.
  bool CanCloseContents(content::WebContents* contents);

  // Returns true if we need to run unload events for the |contents|.
  bool ShouldRunUnloadEventsHelper(content::WebContents* contents);

  // Helper function to run beforeunload listeners on a WebContents.
  // Returns true if |contents| beforeunload listeners were invoked.
  bool RunUnloadEventsHelper(content::WebContents* contents);

  // Called when a BeforeUnload handler is fired for |contents|. |proceed|
  // indicates the user's response to the Y/N BeforeUnload handler dialog. If
  // this parameter is false, any pending attempt to close the whole browser
  // will be canceled. Returns true if Unload handlers should be fired. When the
  // |browser_| is being closed, Unload handlers for any particular WebContents
  // will not be run until every WebContents being closed has a chance to run
  // its BeforeUnloadHandler.
  bool BeforeUnloadFired(content::WebContents* contents, bool proceed);

  // Browser closing consists of the following phases:
  //
  // 1. If the browser has WebContents with before unload handlers, then the
  //    before unload handlers are processed (this is asynchronous). During this
  //    phase IsAttemptingToCloseBrowser() returns true. When processing
  //    completes, the WebContents is removed. Once all WebContents are removed,
  //    the next phase happens. Note that this phase may be aborted.
  // 2. The Browser window is hidden, and a task is posted that results in
  //    deleting the Browser (Views is responsible for posting the task). This
  //    phase can not be stopped. During this phase IsDeleteScheduled()
  //    returns true.
  //
  // Note that there are other cases that may delay closing, such as downloads,
  // but that is done before any of these steps.
  // TODO(crbug.com/40064092): See about unifying IsAttemptingToCloseBrowser()
  // and IsDeleteScheduled().
  bool is_attempting_to_close_browser() const {
    return is_attempting_to_close_browser_;
  }

  // Returns true if the browser window has completed closing and is scheduled
  // for deletion.
  bool is_delete_scheduled() const { return is_delete_scheduled_; }

  // Called when the window closing process has been cancelled.
  void NotifyWindowCloseCancelled(BrowserWindowInterface::ClosingStatus status);

  // Called when the window closing process has been completed and the window
  // can be safely destroyed.
  void OnWindowCloseComplete();

  base::CallbackListSubscription RegisterBrowserDidClose(
      BrowserWindowInterface::BrowserDidCloseCallback callback);
  base::CallbackListSubscription RegisterBrowserCloseCancelled(
      BrowserWindowInterface::BrowserCloseCancelledCallback callback);

  // Called in response to a request to close `browser_`'s window. Returns
  // `BrowserWindowInterface::ClosingStatus::kPermitted` if the window can be
  // closed (or other enum values if closure is not permitted for a given
  // reason).
  BrowserWindowInterface::ClosingStatus GetBrowserClosingStatus();

  // Gives beforeunload handlers the chance to cancel the close. Returns true if
  // the close operation was permitted. Closing can be denied due to different
  // reasons. This function checks if unload handlers are still executing. It
  // further may ask the user for permission to close the browser (e.g. if
  // downloads are ongoing).
  // If this function is called
  // * but the user denied closure after being prompted, it returns false and
  //   emits `BrowserWindowInterface::ClosingStatus::kDeniedByUser`.
  // * but the closure is not permitted by policy, it returns false and emits
  //   `BrowserWindowInterface::ClosingStatus::kDeniedByPolicy`.
  // * while the process begun by `TryToCloseWindow()` is in progress, it
  //   returns false and emits
  //   `BrowserWindowInterface::ClosingStatus::kDeniedUnloadHandlersNeedTime`.
  //
  // If you don't care about beforeunload handlers and just want to prompt the
  // user that they might lose an in-progress operation, call
  // `MaybeWarnBeforeClosing()` instead (`HandleBeforeClose()` also calls this
  // method).
  bool HandleBeforeClose();

  // Invoked when the window containing us is closing. Performs the necessary
  // cleanup.
  void OnWindowClosing();

  // Displays any necessary warnings to the user on taking an action that might
  // close the browser (for example, warning if there are downloads in progress
  // that would be interrupted).
  //
  // Distinct from HandleBeforeClose() (which calls this method) because
  // this method does not consider beforeunload handler, only things the user
  // should be prompted about.
  //
  // If no warnings are needed, the method returns kOkToClose, indicating that
  // the close can proceed immediately, and the callback is not called. If the
  // method returns kDoNotClose, closing should be handled by |warn_callback|
  // (and then only if the callback receives the kOkToClose value).
  WarnBeforeClosingResult MaybeWarnBeforeClosing(
      WarnBeforeClosingCallback warn_callback);

  // Called when all warnings have completed when attempting to close the
  // browser directly (e.g. via hotkey, close button, terminate signal, etc.)
  // Used as a WarnBeforeClosingCallback by HandleBeforeClose().
  void FinishWarnBeforeClosing(WarnBeforeClosingResult result);

  // Begins the process of confirming whether the associated browser can be
  // closed. Beforeunload events won't be fired if |skip_beforeunload| is true.
  // Otherwise, it starts prompting the user, returns true and will call
  // |on_close_confirmed| with the result of the user's decision. After calling
  // this function, if the window will not be closed, call
  // ResetBeforeUnloadHandlers() to reset all beforeunload handlers; calling
  // this function multiple times without an intervening call to
  // Browser::ResetTryToCloseWindow() will run only the beforeunload handlers
  // registered since the previous call. Note that if the browser window has
  // been used before, users should always have a chance to save their work
  // before the window is closed without triggering beforeunload event.
  bool TryToCloseWindow(
      bool skip_beforeunload,
      const base::RepeatingCallback<void(bool)>& on_close_confirmed);

  // Clears the results of any beforeunload confirmation dialogs triggered by a
  // TryToCloseWindow call.
  void ResetTryToCloseWindow();

  // Returns true if |browser_| has any tabs that have BeforeUnload handlers
  // that have not been fired. This method is non-const because it builds a list
  // of tabs that need their BeforeUnloadHandlers fired.
  // TODO(beng): This seems like it could be private but it is used by
  //             AreAllBrowsersCloseable() in application_lifetime.cc. It seems
  //             very similar to ShouldCloseWindow() and some consolidation
  //             could be pursued.
  bool TabsNeedBeforeUnloadFired() const;

  // Clears all the state associated with processing tabs' beforeunload/unload
  // events since the user cancelled closing the window.
  void CancelWindowClose();

  bool ShouldRunUnloadListenerBeforeClosing(content::WebContents* web_contents);

  bool RunUnloadListenerBeforeClosing(content::WebContents* web_contents);

  void BeforeUnloadFired(content::WebContents* web_contents,
                         bool proceed,
                         bool* proceed_to_fire_unload);

  void set_force_skip_warning_user_on_close(
      bool force_skip_warning_user_on_close) {
    force_skip_warning_user_on_close_ = force_skip_warning_user_on_close;
  }
  bool force_skip_warning_user_on_close() const {
    return force_skip_warning_user_on_close_;
  }

  // Indicates whether or not this browser window can be closed, or
  // would be blocked by in-progress downloads.
  // If executing downloads would be cancelled by this window close,
  // then |*num_downloads_blocking| is updated with how many downloads
  // would be canceled if the close continued.
  DownloadCloseType OkToCloseWithInProgressDownloads(
      int* num_downloads_blocking) const;

  // Called when the window is closing to check if potential in-progress
  // downloads should prevent it from closing.
  // Returns true if the window can close, false otherwise.
  bool CanCloseWithInProgressDownloads();

  base::WeakPtr<UnloadController> GetWeakPtr();

 private:
  typedef std::set<raw_ptr<content::WebContents, SetExperimental>>
      UnloadListenerSet;

  enum class CancelDownloadConfirmationState {
    kNotPrompted,         // We have not asked the user.
    kWaitingForResponse,  // We have asked the user and have not received a
                          // response yet.
    kResponseReceived     // The user was prompted and made a decision already.
  };

  // Called when the user has decided whether to proceed or not with the browser
  // closure.  |cancel_downloads| is true if the downloads should be canceled
  // and the browser closed, false if the browser should stay open and the
  // downloads running.
  void InProgressDownloadResponse(bool cancel_downloads);

 private:
  void RenderProcessGone(content::WebContents* web_contents,
                         base::TerminationStatus status) override;

  // Overridden from TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;
  void TabStripEmpty() override;

  void TabAttachedImpl(content::WebContents* contents);
  void TabDetachedImpl(content::WebContents* contents);

  UnloadListenerSet GetTabsNeedingBeforeUnloadFired() const;

  // Processes the next tab that needs it's beforeunload/unload event fired.
  void ProcessPendingTabs(bool skip_beforeunload);

  // Whether we've completed firing all the tabs' beforeunload/unload events.
  bool HasCompletedUnloadProcessing() const;

  // Removes |web_contents| from the passed |set|.
  // Returns whether the tab was in the set in the first place.
  bool RemoveFromSet(UnloadListenerSet* set,
                     content::WebContents* web_contents);

  // Cleans up state appropriately when we are trying to close the browser and
  // the tab has finished firing its unload handler. We also use this in the
  // cases where a tab crashes or hangs even if the beforeunload/unload haven't
  // successfully fired. If |process_now| is true |ProcessPendingTabs| is
  // invoked immediately, otherwise it is invoked after a delay (PostTask).
  //
  // Typically you'll want to pass in true for |process_now|. Passing in true
  // may result in deleting |tab|. If you know that shouldn't happen (because of
  // the state of the stack), pass in false.
  void ClearUnloadState(content::WebContents* web_contents, bool process_now);

  void OnCustomConfirmationClosed(
      base::WeakPtr<content::WebContents> web_contents,
      bool confirmed);

  bool IsUnclosableApp() const;

  bool is_calling_before_unload_handlers() {
    return !on_close_confirmed_.is_null();
  }

  const raw_ptr<Browser> browser_;

  ui::ScopedUnownedUserData<UnloadController> scoped_unowned_user_data_;

  WebContentsCollection web_contents_collection_;

  // Tracks tabs that need their beforeunload event fired before we can
  // close the browser. Only gets populated when we try to close the browser.
  UnloadListenerSet tabs_needing_before_unload_fired_;

  // Tracks tabs that need their unload event fired before we can
  // close the browser. Only gets populated when we try to close the browser.
  UnloadListenerSet tabs_needing_unload_fired_;

  // Whether we are processing the beforeunload and unload events of each tab
  // in preparation for closing the browser. UnloadController owns this state
  // rather than Browser because unload handlers are the only reason that a
  // Browser window isn't just immediately closed.
  bool is_attempting_to_close_browser_;

  // A callback to call to report whether the user chose to close all tabs of
  // |browser_| that have beforeunload event handlers. This is set only if we
  // are currently confirming that the browser is closable. This can be called
  // more than once if a user confirms all the beforeunload prompts (at which
  // point it will be called with true) but the window close is later aborted
  // (at which point it will be called with false). This can happen when
  // multiple browser windows are being closed together. See
  // BrowserList::TryToCloseBrowserList.
  base::RepeatingCallback<void(bool)> on_close_confirmed_;

  // Tells if the browser should skip warning the user when closing the window.
  bool force_skip_warning_user_on_close_ = false;

  // Registered handlers that can intercept and confirm tab unload events.
  std::vector<std::unique_ptr<TabUnloadHandler>> tab_unload_handlers_;

  // State used to figure-out whether we should prompt the user for confirmation
  // when the browser is closed with in-progress downloads.
  CancelDownloadConfirmationState cancel_download_confirmation_state_ =
      CancelDownloadConfirmationState::kNotPrompted;

  WarnBeforeClosingCallback warn_before_closing_callback_;

  using BrowserDidCloseCallbackList =
      base::RepeatingCallbackList<void(BrowserWindowInterface*)>;
  BrowserDidCloseCallbackList browser_did_close_callback_list_;

  using BrowserCloseCancelledCallbackList =
      base::RepeatingCallbackList<void(BrowserWindowInterface*,
                                       BrowserWindowInterface::ClosingStatus)>;
  BrowserCloseCancelledCallbackList browser_close_cancelled_callback_list_;

  // If true, the Browser window has been closed and this will be deleted
  // shortly (after a PostTask).
  bool is_delete_scheduled_ = false;

  base::WeakPtrFactory<UnloadController> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_UNLOAD_CONTROLLER_H_
