// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_RENDERER_RENDERER_URL_LOADER_THROTTLE_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_RENDERER_RENDERER_URL_LOADER_THROTTLE_H_

#include <memory>
#include <string>

#include "base/gtest_prod_util.h"
#include "components/safe_browsing/content/common/safe_browsing.mojom.h"
#include "extensions/buildflags/buildflags.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "url/gurl.h"

namespace safe_browsing {

// RendererURLLoaderThrottle is used in renderer processes to send extension web
// request data to the browser process for extension telemetry.
class RendererURLLoaderThrottle : public blink::URLLoaderThrottle {
 public:
  // TODO(crbug.com/324108312): Remove this default constructor once
  // `RendererURLLoaderThrottle` creation in
  // `URLLoaderThrottleProviderImpl::CreateThrottles()` and
  // `AwURLLoaderThrottleProvider::CreateThrottles()` is gated on
  // `BUILDFLAG(ENABLE_EXTENSIONS)`.
  RendererURLLoaderThrottle();
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // |extension_web_request_reporter| is used for sending
  // extension web requests to the browser.
  explicit RendererURLLoaderThrottle(
      mojo::PendingRemote<mojom::ExtensionWebRequestReporter>
          extension_web_request_reporter);
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
  ~RendererURLLoaderThrottle() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(SBRendererUrlLoaderThrottleTest,
                           DoesNotDeferHttpsImageUrl);
  FRIEND_TEST_ALL_PREFIXES(SBRendererUrlLoaderThrottleTest,
                           DoesNotDeferHttpsScriptUrl);
  FRIEND_TEST_ALL_PREFIXES(SBRendererUrlLoaderThrottleTest,
                           DoesNotDeferChromeUrl);
  FRIEND_TEST_ALL_PREFIXES(SBRendererUrlLoaderThrottleTest,
                           DoesNotDeferIframeUrl);
  FRIEND_TEST_ALL_PREFIXES(SBRendererUrlLoaderThrottleTest,
                           WillRedirectRequest_ProviderDestroyed_NoCrash);

  // blink::URLLoaderThrottle implementation.
  void DetachFromCurrentSequence() override;
  void WillStartRequest(network::ResourceRequest* request,
                        bool* defer) override;
  void WillRedirectRequest(
      net::RedirectInfo* redirect_info,
      const network::mojom::URLResponseHead& response_head,
      bool* defer,
      network::HttpRequestHeadersUpdateParams* headers_update_params) override;
  void WillProcessResponse(const GURL& response_url,
                           network::mojom::URLResponseHead* response_head,
                           bool* defer) override;
  const char* NameForLoggingWillProcessResponse() override;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Bind the pipe created in DetachFromCurrentSequence to the current
  // sequence.
  void BindExtensionWebRequestReporterPipeIfDetached();

  // Send web request data to the browser if the request
  // originated from an extension and destination is HTTP/HTTPS scheme only.
  void MaybeSendExtensionWebRequestData(network::ResourceRequest* request);

  mojo::Remote<mojom::ExtensionWebRequestReporter>
      extension_web_request_reporter_;
  mojo::PendingRemote<mojom::ExtensionWebRequestReporter>
      pending_extension_web_request_reporter_;
  // Tracks if the request originated from an extension, used during redirects
  // to send web request data to the telemetry service.
  std::string origin_extension_id_;
  bool initiated_from_content_script_ = false;
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_RENDERER_RENDERER_URL_LOADER_THROTTLE_H_
