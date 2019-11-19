// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_BROWSER_BROWSER_URL_LOADER_THROTTLE_H_
#define COMPONENTS_SAFE_BROWSING_BROWSER_BROWSER_URL_LOADER_THROTTLE_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "url/gurl.h"

namespace content {
class ResourceContext;
class WebContents;
}

namespace net {
class HttpRequestHeaders;
}

namespace safe_browsing {

class UrlCheckerDelegate;

// BrowserURLLoaderThrottle is used in the browser process to query
// SafeBrowsing to determine whether a URL and also its redirect URLs are safe
// to load.
//
// This throttle never defers starting the URL request or following redirects,
// no matter on mobile or desktop. If any of the checks for the original URL
// and redirect chain are not complete by the time the response headers are
// available, the request is deferred until all the checks are done. It cancels
// the load if any URLs turn out to be bad.
class BrowserURLLoaderThrottle : public blink::URLLoaderThrottle {
 public:
  using GetDelegateCallback =
      base::OnceCallback<scoped_refptr<UrlCheckerDelegate>(
          content::ResourceContext*)>;

  static std::unique_ptr<BrowserURLLoaderThrottle> Create(
      GetDelegateCallback delegate_getter,
      const base::Callback<content::WebContents*()>& web_contents_getter,
      int frame_tree_node_id,
      content::ResourceContext* resource_context);

  ~BrowserURLLoaderThrottle() override;

  // blink::URLLoaderThrottle implementation.
  void WillStartRequest(network::ResourceRequest* request,
                        bool* defer) override;
  void WillRedirectRequest(net::RedirectInfo* redirect_info,
                           const network::mojom::URLResponseHead& response_head,
                           bool* defer,
                           std::vector<std::string>* to_be_removed_headers,
                           net::HttpRequestHeaders* modified_headers) override;
  void WillProcessResponse(const GURL& response_url,
                           network::mojom::URLResponseHead* response_head,
                           bool* defer) override;

 private:
  // CheckerOnIO handles calling methods on SafeBrowsingUrlCheckerImpl, which
  // must be called on the IO thread. The results are synced back to the
  // throttle.
  // TODO(http://crbug.com/824843): Remove this if safe browsing is moved to the
  // UI thread.
  class CheckerOnIO;

  using NativeUrlCheckNotifier =
      base::OnceCallback<void(bool /* proceed */,
                              bool /* showed_interstitial */)>;

  // |web_contents_getter| is used for displaying SafeBrowsing UI when
  // necessary.
  BrowserURLLoaderThrottle(
      GetDelegateCallback delegate_getter,
      const base::Callback<content::WebContents*()>& web_contents_getter,
      int frame_tree_node_id,
      content::ResourceContext* resource_context);

  // |slow_check| indicates whether it reports the result of a slow check.
  // (Please see comments of CheckerOnIO::OnCheckUrlResult() for what slow check
  // means).
  void OnCompleteCheck(bool slow_check, bool proceed, bool showed_interstitial);

  // Called to skip future safe browsing checks and resume the request if
  // necessary.
  void SkipChecks();

  // Called when a slow safe browsing check is ongoing.
  void NotifySlowCheck();

  // Destroys |io_checker_| on the IO thread.
  void DeleteCheckerOnIO();

  size_t pending_checks_ = 0;
  // How many slow checks that haven't received results.
  size_t pending_slow_checks_ = 0;
  bool blocked_ = false;

  // The time when we started deferring the request.
  base::TimeTicks defer_start_time_;
  bool deferred_ = false;

  // The total delay caused by SafeBrowsing deferring the resource load.
  base::TimeDelta total_delay_;
  // Whether the interstitial page has been shown and therefore user action has
  // been involved.
  bool user_action_involved_ = false;

  GURL original_url_;

  // Whether future safe browsing checks should be skipped.
  bool skip_checks_ = false;

  std::unique_ptr<CheckerOnIO> io_checker_;

  base::WeakPtrFactory<BrowserURLLoaderThrottle> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(BrowserURLLoaderThrottle);
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_BROWSER_BROWSER_URL_LOADER_THROTTLE_H_
