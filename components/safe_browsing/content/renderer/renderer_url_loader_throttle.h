// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_RENDERER_RENDERER_URL_LOADER_THROTTLE_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_RENDERER_RENDERER_URL_LOADER_THROTTLE_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/safe_browsing/content/common/safe_browsing.mojom.h"
#include "components/safe_browsing/core/common/safe_browsing_url_checker.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "url/gurl.h"

namespace safe_browsing {

// RendererURLLoaderThrottle is used in renderer processes to query
// SafeBrowsing and determine whether a URL and its redirect URLs are safe to
// load. It defers response processing until all URL checks are completed;
// cancels the load if any URLs turn out to be bad.
class RendererURLLoaderThrottle : public blink::URLLoaderThrottle,
                                  public mojom::UrlCheckNotifier {
 public:
  // |safe_browsing| must stay alive until WillStartRequest() (if it is called)
  // or the end of this object.
  // |render_frame_id| is used for displaying SafeBrowsing UI when necessary.
  RendererURLLoaderThrottle(mojom::SafeBrowsing* safe_browsing,
                            int render_frame_id);
  ~RendererURLLoaderThrottle() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(SBRendererUrlLoaderThrottleTest, DefersHttpsUrl);
  FRIEND_TEST_ALL_PREFIXES(SBRendererUrlLoaderThrottleTest,
                           DoesNotDeferHttpsImageUrl);
  FRIEND_TEST_ALL_PREFIXES(SBRendererUrlLoaderThrottleTest,
                           DoesNotDeferChromeUrl);
  FRIEND_TEST_ALL_PREFIXES(SBRendererUrlLoaderThrottleTest,
                           VerifyTotalDelayHistograms_DoesNotDefer);
  FRIEND_TEST_ALL_PREFIXES(SBRendererUrlLoaderThrottleTest,
                           VerifyTotalDelayHistograms_DoesNotDeferFromCache);
  FRIEND_TEST_ALL_PREFIXES(SBRendererUrlLoaderThrottleTest,
                           VerifyTotalDelayHistograms_Defer);
  FRIEND_TEST_ALL_PREFIXES(SBRendererUrlLoaderThrottleTest,
                           VerifyTotalDelayHistograms_DeferFromCache);
  FRIEND_TEST_ALL_PREFIXES(
      SBRendererUrlLoaderThrottleDisableSkipImageCssFontTest,
      DefersHttpsImageUrl);

  // blink::URLLoaderThrottle implementation.
  void DetachFromCurrentSequence() override;
  void WillStartRequest(network::ResourceRequest* request,
                        bool* defer) override;
  void WillRedirectRequest(
      net::RedirectInfo* redirect_info,
      const network::mojom::URLResponseHead& response_head,
      bool* defer,
      std::vector<std::string>* to_be_removed_headers,
      net::HttpRequestHeaders* modified_headers,
      net::HttpRequestHeaders* modified_cors_exempt_headers) override;
  void WillProcessResponse(const GURL& response_url,
                           network::mojom::URLResponseHead* response_head,
                           bool* defer) override;
  const char* NameForLoggingWillProcessResponse() override;

  // mojom::UrlCheckNotifier implementation.
  void OnCompleteCheck(bool proceed,
                       bool showed_interstitial,
                       bool did_perform_real_time_check,
                       bool did_check_allowlist) override;

  void OnCheckUrlResult(
      mojo::PendingReceiver<mojom::UrlCheckNotifier> slow_check_notifier,
      bool proceed,
      bool showed_interstitial,
      bool did_perform_real_time_check,
      bool did_check_allowlist);

  // Called by the two methods above.
  // |slow_check| indicates whether it reports the result of a slow check.
  // (Please see comments in safe_browsing.mojom for what slow check means).
  void OnCompleteCheckInternal(bool slow_check,
                               bool proceed,
                               bool showed_interstitial);

  void OnMojoDisconnect();

  mojom::SafeBrowsing* safe_browsing_;
  const int render_frame_id_;

  // These fields hold the connection to this instance's private connection to
  // the Safe Browsing service if DetachFromCurrentThread has been called.
  mojo::PendingRemote<mojom::SafeBrowsing> safe_browsing_pending_remote_;
  mojo::Remote<mojom::SafeBrowsing> safe_browsing_remote_;

  mojo::Remote<mojom::SafeBrowsingUrlChecker> url_checker_;

  size_t pending_checks_ = 0;
  size_t pending_slow_checks_ = 0;
  bool blocked_ = false;

  // The time when |WillStartRequest| is called.
  base::TimeTicks start_request_time_;
  bool is_start_request_called_ = false;

  // The time when we started deferring the request.
  base::TimeTicks defer_start_time_;
  bool deferred_ = false;
  // Whether the response loaded is from cache.
  bool is_response_from_cache_ = false;

  // The total delay caused by SafeBrowsing deferring the resource load.
  base::TimeDelta total_delay_;

  std::unique_ptr<mojo::ReceiverSet<mojom::UrlCheckNotifier>>
      notifier_receivers_;

  GURL original_url_;

  base::WeakPtrFactory<RendererURLLoaderThrottle> weak_factory_{this};
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_RENDERER_RENDERER_URL_LOADER_THROTTLE_H_
