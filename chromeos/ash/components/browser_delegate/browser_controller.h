// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BROWSER_DELEGATE_BROWSER_CONTROLLER_H_
#define CHROMEOS_ASH_COMPONENTS_BROWSER_DELEGATE_BROWSER_CONTROLLER_H_

#include <optional>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/function_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation_traits.h"
#include "base/types/optional_ref.h"
#include "chromeos/ash/components/browser_delegate/browser_type.h"
#include "components/apps/link_capturing/intent_picker_info.h"
#include "components/webapps/common/web_app_id.h"
#include "ui/views/controls/webview/simple_web_view.h"
#include "url/gurl.h"

class AccountId;
class BrowserWindowInterface;

namespace aura {
class Window;
}  // namespace aura

namespace content {
class WebContents;
}  // namespace content

namespace url {
class Origin;
}  // namespace url

namespace views {
class SimpleWebViewDialogDelegate;
}  // namespace views

namespace ash {

class BrowserDelegate;

// BrowserController is a singleton created by
// ChromeBrowserMainPartsAsh::PostProfileInit. See also README.md.
class BrowserController {
 public:
  // See AddObserver below.
  class Observer : public base::CheckedObserver {
   public:
    // Called when a browser is created.
    // `browser` is never nullptr.
    // Note: When invoking BrowserController::ForEachBrowser in
    // OnBrowserCreated, the new browser will show up for
    // kAscendingCreationTime but not yet for kAscendingActivationTime.
    // TODO(crbug.com/369688254): Revisit this behavior.
    // Note: No TabObserver events for `browser` will be emitted before this.
    virtual void OnBrowserCreated(BrowserDelegate* browser) {}

    // Called when a browser is activated.
    // `browser` is never nullptr.
    virtual void OnBrowserActivated(BrowserDelegate* browser) {}

    // Called when a browser is closed.
    // `browser` is never nullptr.
    // Note: No TabObserver events for `browser` will be emitted after this.
    virtual void OnBrowserClosed(BrowserDelegate* browser) {}

    // Called when the last browser is irrevocably being closed.
    // TODO(crbug.com/369689187): Figure out if/how we want to allow inspection
    // of the browser (the instance still exists but we shouldn't allow
    // arbitrary operations).
    virtual void OnLastBrowserClosed() {}
  };

  // See AddTabObserver below.
  //
  // Note: When a new browser window is created, all
  // Observer::OnBrowserCreated() notifications are delivered first. Then,
  // OnTabInserted() is emitted for each tab in that browser.
  //
  // Dually, when a browser window is closed, OnTabRemoved() is emitted for
  // each remaining tab in that browser before the Observer::OnBrowserClosed()
  // notifications are delivered.
  class TabObserver : public base::CheckedObserver {
   public:
    // Called when a new tab is inserted into `browser`'s tab strip.
    // `browser` and `contents` are never nullptr.
    virtual void OnTabInserted(BrowserDelegate* browser,
                               content::WebContents* contents) {}

    // Called when a tab in `browser` is removed from its tab strip
    // (including when an entire browser window closes with remaining tabs).
    // `browser` and `contents` are never nullptr. `will_delete` is true if the
    // tab will be destroyed (e.g. closing), and false if it will just move to
    // another browser window dynamically.
    virtual void OnTabRemoved(BrowserDelegate* browser,
                              content::WebContents* contents,
                              bool will_delete) {}

    // Called when a tab's WebContents in `browser` is replaced in place (e.g.
    // discard or prerender swap).
    // `browser`, `old_contents`, and `new_contents` are never nullptr.
    virtual void OnTabReplaced(BrowserDelegate* browser,
                               content::WebContents* old_contents,
                               content::WebContents* new_contents) {}

    // Called when the active tab in `browser` changes.
    // `browser` and `new_contents` are never nullptr.
    // `old_contents` is the previously active WebContents (can be nullptr).
    virtual void OnActiveWebContentsChanged(
        BrowserDelegate* browser,
        content::WebContents* old_contents,
        content::WebContents* new_contents) {}
  };

  // See CreateWebApp below.
  struct CreateParams {
    bool allow_resize;
    bool allow_maximize;
    bool allow_fullscreen;
    // TODO(crbug.com/369689187): Figure out if the restore_id field makes
    // sense, and if so, add a description.
    int32_t restore_id;
  };

  // See ForEachBrowser below.
  enum class BrowserOrder {
    kAscendingCreationTime,
    kAscendingActivationTime,
  };
  using enum BrowserOrder;

  // See ForEachBrowser below.
  enum class IterationDirective {
    kContinueIteration,
    kBreakIteration,
  };
  using enum IterationDirective;

  static BrowserController* GetInstance();

  // Returns the corresponding delegate, possibly creating it first.
  // Returns nullptr for a nullptr input.
  // NOTE: This function is here only temporarily to facilitate transitioning
  // code from BrowserWindowInterface to BrowserDelegate incrementally. See also
  // BrowserDelegate::GetBrowser.
  virtual BrowserDelegate* GetDelegate(BrowserWindowInterface* bwi) = 0;

  // Returns (the delegate for) the most recently used browser that still
  // exists. Returns nullptr if there's none.
  virtual BrowserDelegate* GetLastUsedBrowser() = 0;

  // Returns (the delegate for) the most recently used browser that is
  // currently visible. Returns nullptr if there's none.
  virtual BrowserDelegate* GetLastUsedVisibleBrowser() = 0;

  // Returns (the delegate for) the most recently used browser that is
  // currently visible and on-the-record. Returns nullptr if there's none.
  virtual BrowserDelegate* GetLastUsedVisibleOnTheRecordBrowser() = 0;

  // Iterates over (the delegates for) the currently existing browsers in the
  // given order, invoking the callback for each. The callback can terminate the
  // iteration early by returning kBreakIteration.
  virtual void ForEachBrowser(
      BrowserOrder order,
      base::FunctionRef<IterationDirective(BrowserDelegate&)> callback) = 0;

  // Returns (the delegate for) the browser associated with the given native
  // window, if any. This can be nullptr when the browser is shutting down.
  virtual BrowserDelegate* GetBrowserForWindow(aura::Window* window) = 0;

  // Returns (the delegate for) the browser associated with the given tab, if
  // any. This can be nullptr when the tab is in the process of being moved from
  // one browser to another.
  virtual BrowserDelegate* GetBrowserForTab(content::WebContents* contents) = 0;

  // Returns (the delegate for) the most recently activated web app browser
  // that matches the given parameters. Returns nullptr if there's none.
  // Url matching is done ignoring any references, and only if `url` is not
  // empty.
  // The `browser_type` must be kApp or kAppPopup.
  virtual BrowserDelegate* FindWebApp(const AccountId& account_id,
                                      webapps::AppId app_id,
                                      BrowserType browser_type,
                                      const GURL& url = GURL()) = 0;

  // Makes a POST request in a new tab in the last active tabbed browser. If no
  // such browser exists, a new one is created. Returns nullptr if the creation
  // is not possible for the given arguments.
  // This is needed by the Media app.
  virtual BrowserDelegate* NewTabWithPostData(
      const AccountId& account_id,
      const GURL& url,
      base::span<const uint8_t> post_data,
      std::string_view extra_headers) = 0;

  // Creates a web app browser for the given parameters.
  // The `browser_type` must be kApp or kAppPopup. In the case of kApp, a pinned
  // home tab is added if that feature is supported and a URL is registered for
  // the app.
  // Returns nullptr if the creation is not possible for the given arguments.
  virtual BrowserDelegate* CreateWebApp(const AccountId& account_id,
                                        webapps::AppId app_id,
                                        BrowserType browser_type,
                                        const CreateParams& params) = 0;

  // Closes all browsers. It may fail.
  // Note: conceptually this should be equivalent to
  //
  // ForEachBrowser(..., [](BrowserDelegate& browser) {
  //   browser.Close();
  // });
  //
  // but currently it has different implementation with some additional work
  // for historical reason.
  // Nice to revisit here to migrate in the future.
  virtual void MayCloseAllBrowsers() = 0;

  // Closes all browsers and if successful, quits BrowserController
  // (i.e., the Chrome as a whole encapsulated by BrowserController).
  // Note: In ChromeOS, it also means to shut down the chromeos-chrome
  // including OS system UI.
  // Note: this is currently used only by Kiosk app updating. We should see
  // if this can be consolidated with CloseAllBrowsers() declared above.
  virtual void MayCloseAllBrowsersAndQuit() = 0;

  // Returns whether the BrowserController is trying to quit.
  virtual bool IsTryingToQuit() = 0;

  // Returns whether BrowserController shutdown is started.
  // Conceptually this is closer to IsTryingToQuit, but run in different
  // context. Please find chrome/browser/lifetime for details of the
  // implementation difference.
  // Note: currently these carries the complexity from Chrome implementation
  // just to be transparent, but later it'd be great to consolidate closer APIs.
  virtual bool HasShutdownStarted() = 0;

  // Facilitates observation of browser events.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Facilitates observation of tab events across all browsers.
  virtual void AddTabObserver(TabObserver* observer) = 0;
  virtual void RemoveTabObserver(TabObserver* observer) = 0;

  // Encapsulates the creation of AutofillClient instances.
  virtual void CreateAutofillClientForWebContents(
      content::WebContents* web_contents) = 0;

  // Creates a SimpleWebView.
  virtual std::unique_ptr<views::SimpleWebView>
  CreateSimpleWebViewForSigninScreen(
      views::SimpleWebViewDialogDelegate* delegate) = 0;

  // Shows the intent picker bubble for the browser window containing
  // `web_contents`, and manages showing/hiding the omnibox icon on that tab.
  // Returns false if `web_contents` is invalid, no browser was found for it,
  // or `app_info` is empty.
  virtual bool ShowIntentPicker(
      base::WeakPtr<content::WebContents> web_contents,
      std::vector<apps::IntentPickerAppInfo> app_info,
      bool show_stay_in_chrome,
      bool show_remember_selection,
      apps::IntentPickerBubbleType bubble_type,
      base::optional_ref<const url::Origin> initiating_origin,
      IntentPickerResponse callback) = 0;

 protected:
  BrowserController();
  BrowserController(const BrowserController&) = delete;
  BrowserController& operator=(const BrowserController&) = delete;
  virtual ~BrowserController();
};

}  // namespace ash

namespace base {

template <>
struct ScopedObservationTraits<ash::BrowserController,
                               ash::BrowserController::TabObserver> {
  static void AddObserver(ash::BrowserController* source,
                          ash::BrowserController::TabObserver* observer) {
    source->AddTabObserver(observer);
  }
  static void RemoveObserver(ash::BrowserController* source,
                             ash::BrowserController::TabObserver* observer) {
    source->RemoveTabObserver(observer);
  }
};

}  // namespace base

#endif  // CHROMEOS_ASH_COMPONENTS_BROWSER_DELEGATE_BROWSER_CONTROLLER_H_
