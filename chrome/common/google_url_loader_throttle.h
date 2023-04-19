// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_GOOGLE_URL_LOADER_THROTTLE_H_
#define CHROME_COMMON_GOOGLE_URL_LOADER_THROTTLE_H_

#include "build/build_config.h"
#include "chrome/common/renderer_configuration.mojom.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "extensions/buildflags/buildflags.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
#include "chrome/common/bound_session_request_throttled_listener.h"
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
      std::unique_ptr<BoundSessionRequestThrottledListener>
          bound_session_request_throttled_listener,
#endif
      chrome::mojom::DynamicParamsPtr dynamic_params);

  ~GoogleURLLoaderThrottle() override;

  static void UpdateCorsExemptHeader(
      network::mojom::NetworkContextParams* params);

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
  bool ShouldDeferRequestForBoundSession(const GURL& request_url) const;
  void OnDeferRequestForBoundSessionCompleted(
      BoundSessionRequestThrottledListener::UnblockAction resume);
  void ResumeOrCancelRequest(
      BoundSessionRequestThrottledListener::UnblockAction resume);

  std::unique_ptr<BoundSessionRequestThrottledListener>
      bound_session_request_throttled_listener_;
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
