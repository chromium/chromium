// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_GOOGLE_URL_LOADER_THROTTLE_H_
#define CHROME_COMMON_GOOGLE_URL_LOADER_THROTTLE_H_

#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/common/renderer_configuration.mojom.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "extensions/buildflags/buildflags.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
#include "chrome/common/bound_session_request_throttled_handler.h"
#endif

// This class changes requests for Google-specific features (e.g. adding &
// removing Varitaions headers, Safe Search & Restricted YouTube & restricting
// consumer accounts through group policy.
class GoogleURLLoaderThrottle
    : public blink::URLLoaderThrottle,
      public base::SupportsWeakPtr<GoogleURLLoaderThrottle> {
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
  static bool ShouldDeferRequestForBoundSession(
      const GURL& request_url,
      chrome::mojom::BoundSessionThrottlerParams*
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
#if BUILDFLAG(ENABLE_EXTENSIONS)
  void WillProcessResponse(const GURL& response_url,
                           network::mojom::URLResponseHead* response_head,
                           bool* defer) override;
#endif
 private:
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  void OnDeferRequestForBoundSessionCompleted(
      BoundSessionRequestThrottledHandler::UnblockAction resume);
  void ResumeOrCancelRequest(
      BoundSessionRequestThrottledHandler::UnblockAction resume);

  std::unique_ptr<BoundSessionRequestThrottledHandler>
      bound_session_request_throttled_handler_;
  absl::optional<base::TimeTicks> bound_session_request_throttled_start_time_;
  bool is_main_frame_navigation_ = false;
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
