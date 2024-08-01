// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/error_page/content/browser/net_error_auto_reloader.h"

#include <algorithm>

#include "base/functional/callback.h"
#include "base/logging.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/web_contents.h"
#include "net/base/net_errors.h"
#include "url/gurl.h"

namespace error_page {

namespace {

bool ShouldAutoReload(content::NavigationHandle* handle) {
  DCHECK(handle->HasCommitted());
  const int net_error = handle->GetNetErrorCode();
  return handle->IsErrorPage() && net_error != net::OK && !handle->IsPost() &&
         // For now, net::ERR_UNKNOWN_URL_SCHEME is only being displayed on
         // Chrome for Android.
         net_error != net::ERR_UNKNOWN_URL_SCHEME &&
         // Do not trigger for SSL interstitials since they're not fixed by
         // reloads.
         !net::IsCertificateError(net_error) &&
         // Do not trigger if the server rejects a client certificate.
         // https://crbug.com/431387
         !net::IsClientCertificateError(net_error) &&
         // Some servers reject client certificates with a generic
         // handshake_failure alert.
         // https://crbug.com/431387
         net_error != net::ERR_SSL_PROTOCOL_ERROR &&
         // Do not trigger for blocklisted URLs.
         // https://crbug.com/803839
         // Do not trigger for requests that were blocked by the browser itself.
         !net::IsRequestBlockedError(net_error) &&
         // Do not trigger for this error code because it is used by Chrome
         // while an auth prompt is being displayed.
         net_error != net::ERR_INVALID_AUTH_CREDENTIALS &&
         // Don't auto-reload non-http/https schemas.
         // https://crbug.com/471713
         handle->GetURL().SchemeIsHTTPOrHTTPS() &&
         // Don't auto reload if the error was a secure DNS network error, since
         // the reload may interfere with the captive portal probe state.
         // TODO(crbug.com/40104002): Explore how to allow reloads for secure
         // DNS network errors without interfering with the captive portal probe
         // state.
         !handle->GetResolveErrorInfo().is_secure_network_error &&
         // Don't auto reload if the error is caused by the server returning a
         // non-2xx HTTP response code.
         net_error != net::ERR_HTTP_RESPONSE_CODE_FAILURE &&
         // Do not auto-reload if the error is caused by private network access
         // preflight failures because user reloads have different initiator
         // policies.
         net_error != net::ERR_BLOCKED_BY_PRIVATE_NETWORK_ACCESS_CHECKS;
}

base::TimeDelta GetNextReloadDelay(size_t reload_count) {
  static constexpr base::TimeDelta kDelays[] = {
      base::Seconds(1), base::Seconds(5),  base::Seconds(30), base::Minutes(1),
      base::Minutes(5), base::Minutes(10), base::Minutes(30)};
  return kDelays[std::min(reload_count, std::size(kDelays) - 1)];
}

// Helper to block a navigation that would result in re-committing the same
// error page a tab is already displaying.
class IgnoreDuplicateErrorThrottle : public content::NavigationThrottle {
 public:
  using ShouldSuppressCallback =
      base::OnceCallback<bool(content::NavigationHandle*)>;

  IgnoreDuplicateErrorThrottle(content::NavigationHandle* handle,
                               ShouldSuppressCallback should_suppress)
      : content::NavigationThrottle(handle),
        should_suppress_(std::move(should_suppress)) {
    DCHECK(should_suppress_);
  }
  IgnoreDuplicateErrorThrottle(const IgnoreDuplicateErrorThrottle&) = delete;
  IgnoreDuplicateErrorThrottle& operator=(const IgnoreDuplicateErrorThrottle&) =
      delete;
  ~IgnoreDuplicateErrorThrottle() override = default;

  // content::NavigationThrottle:
  content::NavigationThrottle::ThrottleCheckResult WillFailRequest() override {
    DCHECK(should_suppress_);
    if (std::move(should_suppress_).Run(navigation_handle()))
      return content::NavigationThrottle::ThrottleAction::CANCEL;
    return content::NavigationThrottle::ThrottleAction::PROCEED;
  }

  const char* GetNameForLogging() override {
    return "IgnoreDuplicateErrorThrottle";
  }

 private:
  ShouldSuppressCallback should_suppress_;
};

}  // namespace

NetErrorAutoReloader::ErrorPageInfo::ErrorPageInfo(const GURL& url,
                                                   net::Error error)
    : url(url), error(error) {}

NetErrorAutoReloader::ErrorPageInfo::~ErrorPageInfo() = default;

NetErrorAutoReloader::NetErrorAutoReloader(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<NetErrorAutoReloader>(*web_contents),
      connection_tracker_(content::GetNetworkConnectionTracker()) {
  connection_tracker_->AddNetworkConnectionObserver(this);

  network::mojom::ConnectionType connection_type;
  if (connection_tracker_->GetConnectionType(
          &connection_type,
          base::BindOnce(&NetErrorAutoReloader::SetInitialConnectionType,
                         weak_ptr_factory_.GetWeakPtr()))) {
    SetInitialConnectionType(connection_type);
  }
}

NetErrorAutoReloader::~NetErrorAutoReloader() {
  // NOTE: Tests may call `DisableConnectionChangeObservationForTesting` to null
  // this out.
  if (connection_tracker_)
    connection_tracker_->RemoveNetworkConnectionObserver(this);
}

// static
std::unique_ptr<content::NavigationThrottle>
NetErrorAutoReloader::MaybeCreateThrottleFor(
    content::NavigationHandle* handle) {
  if (!handle->IsInPrimaryMainFrame())
    return nullptr;

  // Note that `CreateForWebContents` is a no-op if `contents` already has a
  // NetErrorAutoReloader. See WebContentsUserData.
  content::WebContents* contents = handle->GetWebContents();
  CreateForWebContents(contents);
  return FromWebContents(contents)->MaybeCreateThrottle(handle);
}

void NetErrorAutoReloader::DidStartNavigation(
    content::NavigationHandle* handle) {
  if (!handle->IsInPrimaryMainFrame())
    return;

  // Suppress automatic reload as long as any navigations are pending.
  PauseAutoReloadTimerIfRunning();
  pending_navigations_.insert(handle);
}

void NetErrorAutoReloader::DidFinishNavigation(
    content::NavigationHandle* handle) {
  if (!handle->IsInPrimaryMainFrame())
    return;

  pending_navigations_.erase(handle);
  if (!handle->HasCommitted()) {
    // This navigation was cancelled and not committed. If there are still other
    // pending navigations, or we aren't sitting on a error page which allows
    // auto-reload, there's nothing to do.
    if (!pending_navigations_.empty() || !current_reloadable_error_page_info_)
      return;

    // The last pending navigation was just cancelled and we're sitting on an
    // error page which allows auto-reload. Schedule the next auto-reload
    // attempt.
    is_auto_reload_in_progress_ = false;
    ScheduleNextAutoReload();
    return;
  }

  if (!ShouldAutoReload(handle)) {
    // We've committed something that doesn't support auto-reload. Reset
    // all auto-reload state so nothing interesting happens until another
    // error page navigation is committed.
    Reset();
    return;
  }

  // This heuristic isn't perfect but it should be good enough: if the new
  // commit is not a reload, or if it's an error page with an error code
  // different from what we had previously committed, we treat it as a new
  // error and thus reset our tracking state.
  net::Error net_error = handle->GetNetErrorCode();
  if (handle->GetReloadType() == content::ReloadType::NONE ||
      !current_reloadable_error_page_info_ ||
      net_error != current_reloadable_error_page_info_->error) {
    Reset();
    current_reloadable_error_page_info_ =
        ErrorPageInfo(handle->GetURL(), net_error);
  }

  // We only schedule a reload if there are no other pending navigations.
  // If there are and they end up getting terminated without a commit, we
  // will schedule the next auto-reload at that time.
  if (pending_navigations_.empty())
    ScheduleNextAutoReload();
}

void NetErrorAutoReloader::NavigationStopped() {
  // Stopping navigation or loading cancels all pending auto-reload behavior
  // until the next time a new error page is committed. Note that a stop during
  // navigation will also result in a DidFinishNavigation with a failed
  // navigation and an error code of ERR_ABORTED. However stops can also occur
  // after an error page commits but before it finishes loading, and we want to
  // catch those cases too.
  Reset();
}

void NetErrorAutoReloader::OnVisibilityChanged(content::Visibility visibility) {
  if (!IsWebContentsVisible()) {
    PauseAutoReloadTimerIfRunning();
  } else if (pending_navigations_.empty()) {
    ResumeAutoReloadIfPaused();
  }
}

void NetErrorAutoReloader::OnConnectionChanged(
    network::mojom::ConnectionType type) {
  is_online_ = (type != network::mojom::ConnectionType::CONNECTION_NONE);
  if (!is_online_) {
    PauseAutoReloadTimerIfRunning();
  } else if (pending_navigations_.empty()) {
    ResumeAutoReloadIfPaused();
  }
}

// static
base::TimeDelta NetErrorAutoReloader::GetNextReloadDelayForTesting(
    size_t reload_count) {
  return GetNextReloadDelay(reload_count);
}

void NetErrorAutoReloader::DisableConnectionChangeObservationForTesting() {
  if (connection_tracker_) {
    connection_tracker_->RemoveNetworkConnectionObserver(this);
    connection_tracker_ = nullptr;
  }
}

void NetErrorAutoReloader::SetInitialConnectionType(
    network::mojom::ConnectionType type) {
  // NOTE: Tests may call `DisableConnectionChangeObservationForTesting` to null
  // this out.
  if (connection_tracker_)
    OnConnectionChanged(type);
}

bool NetErrorAutoReloader::IsWebContentsVisible() {
  return web_contents()->GetVisibility() != content::Visibility::HIDDEN;
}

void NetErrorAutoReloader::Reset() {
  next_reload_timer_.reset();
  num_reloads_for_current_error_ = 0;
  is_auto_reload_in_progress_ = false;
  current_reloadable_error_page_info_.reset();
}

void NetErrorAutoReloader::PauseAutoReloadTimerIfRunning() {
  next_reload_timer_.reset();
}

void NetErrorAutoReloader::ResumeAutoReloadIfPaused() {
  if (current_reloadable_error_page_info_ && !next_reload_timer_)
    ScheduleNextAutoReload();
}

void NetErrorAutoReloader::ScheduleNextAutoReload() {
  DCHECK(current_reloadable_error_page_info_);
  if (!is_online_ || !IsWebContentsVisible())
    return;

  // Note that Unretained is safe here because base::OneShotTimer will never
  // run its callback once destructed.
  next_reload_timer_.emplace();
  next_reload_timer_->Start(
      FROM_HERE, GetNextReloadDelay(num_reloads_for_current_error_),
      base::BindOnce(&NetErrorAutoReloader::ReloadMainFrame,
                     base::Unretained(this)));
}

void NetErrorAutoReloader::ReloadMainFrame() {
  DCHECK(current_reloadable_error_page_info_);
  if (!is_online_ || !IsWebContentsVisible())
    return;

  ++num_reloads_for_current_error_;
  is_auto_reload_in_progress_ = true;
  web_contents()->GetPrimaryMainFrame()->Reload();
}

std::unique_ptr<content::NavigationThrottle>
NetErrorAutoReloader::MaybeCreateThrottle(content::NavigationHandle* handle) {
  DCHECK(handle->IsInPrimaryMainFrame());
  if (!current_reloadable_error_page_info_ ||
      current_reloadable_error_page_info_->url != handle->GetURL() ||
      !is_auto_reload_in_progress_) {
    return nullptr;
  }

  return std::make_unique<IgnoreDuplicateErrorThrottle>(
      handle, base::BindOnce(&NetErrorAutoReloader::ShouldSuppressErrorPage,
                             base::Unretained(this)));
}

bool NetErrorAutoReloader::ShouldSuppressErrorPage(
    content::NavigationHandle* handle) {
  // We already verified these conditions when the throttle was created, but now
  // that the throttle is about to fail its navigation, we double-check in case
  // another navigation has committed in the interim.
  if (!current_reloadable_error_page_info_ ||
      current_reloadable_error_page_info_->url != handle->GetURL() ||
      current_reloadable_error_page_info_->error != handle->GetNetErrorCode()) {
    return false;
  }

  return true;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(NetErrorAutoReloader);

}  // namespace error_page
