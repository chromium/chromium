// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_GOOGLE_URL_LOADER_THROTTLE_H_
#define CHROME_COMMON_GOOGLE_URL_LOADER_THROTTLE_H_

#include <optional>
#include <vector>

#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/common/renderer_configuration.mojom.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "extensions/buildflags/buildflags.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
#include "chrome/common/bound_session_request_throttled_handler.h"
#endif

// This class changes requests for Google-specific features (e.g. adding &
// removing Variations headers, Safe Search & Restricted YouTube & restricting
// consumer accounts through group policy.
class GoogleURLLoaderThrottle final : public blink::URLLoaderThrottle {
 public:
  explicit GoogleURLLoaderThrottle(
#if BUILDFLAG(IS_ANDROID)
      const std::string& client_data_header,
#endif
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
      std::unique_ptr<BoundSessionRequestThrottledHandler>
          bound_session_request_throttled_handler,
#endif
      chrome::mojom::DynamicParamsPtr dynamic_params);

  ~GoogleURLLoaderThrottle() override;

  static void UpdateCorsExemptHeader(
      network::mojom::NetworkContextParams* params);

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  // Elements of these enum are ordered in such a way that two independent
  // statuses can be merged together using `std::max()` function to get an
  // aggregate status.
  enum class RequestBoundSessionStatus {
    kNotCovered = 0,
    kCoveredWithFreshCookie = 1,
    kCoveredWithMissingCookie = 2
  };

  static RequestBoundSessionStatus GetRequestBoundSessionStatus(
      const GURL& request_url,
      const std::vector<chrome::mojom::BoundSessionThrottlerParamsPtr>&
          bound_session_throttler_params);
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)

  // blink::URLLoaderThrottle:
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
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  void WillOnCompleteWithError(
      const network::URLLoaderCompletionStatus& status) override;
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)

 private:
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  void OnDeferRequestForBoundSessionCompleted(
      BoundSessionRequestThrottledHandler::UnblockAction resume,
      chrome::mojom::ResumeBlockedRequestsTrigger resume_trigger);
  void ResumeOrCancelRequest(
      BoundSessionRequestThrottledHandler::UnblockAction resume,
      chrome::mojom::ResumeBlockedRequestsTrigger resume_trigger);

  std::unique_ptr<BoundSessionRequestThrottledHandler>
      bound_session_request_throttled_handler_;
  std::optional<base::TimeTicks> bound_session_request_throttled_start_time_;
  bool is_main_frame_navigation_ = false;
  bool sends_cookies_ = false;
  // `true` if at least one URL in the redirect chain was affected.
  bool is_covered_by_bound_session_ = false;
  bool is_deferred_for_bound_session_ = false;
  std::optional<chrome::mojom::ResumeBlockedRequestsTrigger>
      deferred_request_resume_trigger_;
#endif

#if BUILDFLAG(IS_ANDROID)
  std::string client_data_header_;
#endif

  const chrome::mojom::DynamicParamsPtr dynamic_params_;

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  base::WeakPtrFactory<GoogleURLLoaderThrottle> weak_factory_{this};
#endif
};

#endif  // CHROME_COMMON_GOOGLE_URL_LOADER_THROTTLE_H_
