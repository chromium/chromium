// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ERROR_PAGE_CONTENT_BROWSER_NET_ERROR_AUTO_RELOADER_H_
#define COMPONENTS_ERROR_PAGE_CONTENT_BROWSER_NET_ERROR_AUTO_RELOADER_H_

#include <stddef.h>

#include <memory>
#include <optional>
#include <set>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/network_connection_tracker.h"
#include "services/network/public/mojom/network_change_manager.mojom.h"
#include "url/gurl.h"

namespace content {
class NavigationHandle;
class NavigationThrottle;
class WebContents;
}  // namespace content

namespace error_page {

// This class implements support for automatic reload attempts with backoff
// whenever a WebContents' main frame lands on common network error pages. This
// excludes errors that aren't connectivity related since a reload doesn't
// generally fix them (e.g. SSL errors or when the client blocked the request).
// To use this behavior as a Content embedder, simply call the static
// `MaybeCreateNavigationThrottle()` method from within your implementation of
// ContentBrowserClient::CreateThrottlesForNavigation.
class NetErrorAutoReloader
    : public content::WebContentsObserver,
      public content::WebContentsUserData<NetErrorAutoReloader>,
      public network::NetworkConnectionTracker::NetworkConnectionObserver {
 public:
  NetErrorAutoReloader(const NetErrorAutoReloader&) = delete;
  NetErrorAutoReloader& operator=(const NetErrorAutoReloader&) = delete;
  ~NetErrorAutoReloader() override;

  // Maybe installs a throttle for the given navigation, lazily initializing the
  // appropriate WebContents' NetErrorAutoReloader instance if necessary. For
  // embedders wanting to use NetErrorAutoReload's behavior, it's sufficient to
  // call this from ContentBrowserClient::CreateThrottlesForNavigation for each
  // navigation processed.
  static std::unique_ptr<content::NavigationThrottle> MaybeCreateThrottleFor(
      content::NavigationHandle* handle);

  // content::WebContentsObserver:
  void DidStartNavigation(content::NavigationHandle* handle) override;
  void DidFinishNavigation(content::NavigationHandle* handle) override;
  void NavigationStopped() override;
  void OnVisibilityChanged(content::Visibility visibility) override;

  // network::NetworkConnectionTracker::NetworkConnectionObserver:
  void OnConnectionChanged(network::mojom::ConnectionType type) override;

  // Returns the delay applied when scheduling the next auto-reload of a page
  // after it's already been auto-reloaded `reload_count` times.
  static base::TimeDelta GetNextReloadDelayForTesting(size_t reload_count);

  // Permanently unsubscribes this object from receiving OnConnectionChanged
  // notifications. Used in tests which want to drive this behavior explicitly.
  void DisableConnectionChangeObservationForTesting();

  // Returns the timer used internally to schedule the next auto-reload task,
  // or null if no auto-reload task is currently scheduled.
  std::optional<base::OneShotTimer>& next_reload_timer_for_testing() {
    return next_reload_timer_;
  }

 private:
  friend class content::WebContentsUserData<NetErrorAutoReloader>;

  explicit NetErrorAutoReloader(content::WebContents* web_contents);

  void SetInitialConnectionType(network::mojom::ConnectionType type);
  bool IsWebContentsVisible();
  void Reset();
  void PauseAutoReloadTimerIfRunning();
  void ResumeAutoReloadIfPaused();
  void ScheduleNextAutoReload();
  void ReloadMainFrame();
  std::unique_ptr<content::NavigationThrottle> MaybeCreateThrottle(
      content::NavigationHandle* handle);
  bool ShouldSuppressErrorPage(content::NavigationHandle* handle);

  struct ErrorPageInfo {
    ErrorPageInfo(const GURL& url, net::Error error);
    ~ErrorPageInfo();

    GURL url;
    net::Error error;
  };

  raw_ptr<network::NetworkConnectionTracker> connection_tracker_;
  bool is_online_ = true;
  std::set<raw_ptr<content::NavigationHandle, SetExperimental>>
      pending_navigations_;
  std::optional<base::OneShotTimer> next_reload_timer_;
  std::optional<ErrorPageInfo> current_reloadable_error_page_info_;
  size_t num_reloads_for_current_error_ = 0;
  bool is_auto_reload_in_progress_ = false;
  base::WeakPtrFactory<NetErrorAutoReloader> weak_ptr_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace error_page

#endif  // COMPONENTS_ERROR_PAGE_CONTENT_BROWSER_NET_ERROR_AUTO_RELOADER_H_
