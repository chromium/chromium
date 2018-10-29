// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_FAST_UNLOAD_CONTROLLER_H_
#define CHROME_BROWSER_UI_FAST_UNLOAD_CONTROLLER_H_

#include <memory>
#include <set>
#include <unordered_map>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_piece.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_delegate.h"

class Browser;
class TabStripModel;

namespace content {
class NotificationSource;
class NotificationDetails;
class WebContents;
}

// FastUnloadController manages closing tabs and windows -- especially in
// regards to beforeunload handlers (have proceed/cancel dialogs) and
// unload handlers (have no user interaction).
//
// Typical flow of closing a tab:
//  1. Browser calls CanCloseContents().
//     If true, browser calls contents::CloseWebContents().
//  2. WebContents notifies us via its delegate and BeforeUnloadFired()
//     that the beforeunload handler was run. If the user allowed the
//     close to continue, we detached the tab and hold onto it while the
//     close finishes.
//
// Typical flow of closing a window:
//  1. BrowserView::CanClose() calls TabsNeedBeforeUnloadFired().
//     If beforeunload/unload handlers need to run, FastUnloadController returns
//     true and calls ProcessPendingTabs() (private method).
//  2. For each tab with a beforeunload/unload handler, ProcessPendingTabs()
//        calls |CoreTabHelper::OnCloseStarted()|
//        and   |web_contents->GetRenderViewHost()->FirePageBeforeUnload()|.
//  3. If the user allowed the close to continue, we detach all the tabs with
//     unload handlers, remove them from the tab strip, and finish closing
//     the tabs in the background.
//  4. The browser gets notified that the tab strip is empty and calls
//     CloseFrame where the empty tab strip causes the window to hide.
//     Once the detached tabs finish, the browser calls CloseFrame again and
//     the window is finally closed.
//
class FastUnloadController : public content::NotificationObserver,
                             public TabStripModelObserver,
                             public content::WebContentsDelegate {
 public:
  explicit FastUnloadController(Browser* browser);
  ~FastUnloadController() override;

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
  bool BeforeUnloadFiredForContents(content::WebContents* contents,
                                    bool proceed);

  bool is_attempting_to_close_browser() const {
    return is_attempting_to_close_browser_;
  }

  // Called in response to a request to close |browser_|'s window. Returns true
  // when there are no remaining beforeunload handlers to be run.
  bool ShouldCloseWindow();

  // Begins the process of confirming whether the associated browser can be
  // closed. Beforeunload events won't be fired if |skip_beforeunload|
  // true.
  bool TryToCloseWindow(bool skip_beforeunload,
                        const base::Callback<void(bool)>& on_close_confirmed);

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
  bool TabsNeedBeforeUnloadFired();

  // Returns true if all tabs' beforeunload/unload events have fired.
  bool HasCompletedUnloadProcessing() const;

  // Clears all the state associated with processing tabs' beforeunload/unload
  // events since the user cancelled closing the window.
  void CancelWindowClose();

 private:
  // Overridden from content::WebContentsDelegate
  bool ShouldSuppressDialogs(content::WebContents* source) override;
  void CloseContents(content::WebContents* source) override;

  // Overridden from content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // Overridden from TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;
  void TabStripEmpty() override;

  void TabAttachedImpl(content::WebContents* contents);
  void TabDetachedImpl(content::WebContents* contents);

  // Detach |contents| and wait for it to finish closing.
  // The close must be inititiated outside of this method.
  // Returns true if it succeeds.
  bool DetachWebContents(content::WebContents* contents);

  // Processes the next tab that needs its beforeunload/unload event fired.
  void ProcessPendingTabs(bool skip_beforeunload);

  // Cleans up state appropriately when we are trying to close the
  // browser or close a tab in the background. We also use this in the
  // cases where a tab crashes or hangs even if the
  // beforeunload/unload haven't successfully fired.
  void ClearUnloadState(content::WebContents* contents);

  // Helper for |ClearUnloadState| to unwind stack before proceeding.
  void PostTaskForProcessPendingTabs();

  // Log a step of the unload processing.
  void LogUnloadStep(const base::StringPiece& step_name,
                     content::WebContents* contents) const;

  void CancelTabNeedingBeforeUnloadAck();

  bool is_calling_before_unload_handlers() {
    return !on_close_confirmed_.is_null();
  }

  Browser* const browser_;

  content::NotificationRegistrar registrar_;

  typedef std::set<content::WebContents*> WebContentsSet;

  // Tracks tabs that need their beforeunload event started.
  // Only gets populated when we try to close the browser.
  WebContentsSet tabs_needing_before_unload_;

  // Tracks the tab that needs its beforeunload event result.
  // Only gets populated when we try to close the browser.
  content::WebContents* tab_needing_before_unload_ack_;

  // Tracks tabs that need their unload event started.
  // Only gets populated when we try to close the browser.
  WebContentsSet tabs_needing_unload_;

  // Tracks tabs that need to finish running their unload event.
  // Populated both when closing individual tabs and when closing the browser.
  WebContentsSet tabs_needing_unload_ack_;

  // Whether we are processing the beforeunload and unload events of each tab
  // in preparation for closing the browser. FastUnloadController owns this
  // state rather than Browser because unload handlers are the only reason that
  // a Browser window isn't just immediately closed.
  bool is_attempting_to_close_browser_;

  // A callback to call to report whether the user chose to close all tabs of
  // |browser_| that have beforeunload event handlers. This is set only if we
  // are currently confirming that the browser is closable.
  base::Callback<void(bool)> on_close_confirmed_;

  // This class must wait for a call to WebContentsDelegate::CloseContents
  // before the WebContents can be deleted.
  std::unordered_map<content::WebContents*,
                     std::unique_ptr<content::WebContents>>
      web_contents_waiting_for_deletion_;

  base::WeakPtrFactory<FastUnloadController> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FastUnloadController);
};

#endif  // CHROME_BROWSER_UI_FAST_UNLOAD_CONTROLLER_H_
