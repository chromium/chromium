// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_RENDERER_RENDERER_URL_LOADER_THROTTLE_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_RENDERER_RENDERER_URL_LOADER_THROTTLE_H_

#include <memory>
#include <optional>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/types/optional_ref.h"
#include "components/safe_browsing/content/common/safe_browsing.mojom.h"
#include "components/safe_browsing/core/common/safe_browsing_url_checker.mojom.h"
#include "extensions/buildflags/buildflags.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "url/gurl.h"

namespace safe_browsing {

// RendererURLLoaderThrottle is used in renderer processes to query
// SafeBrowsing and determine whether a URL and its redirect URLs are safe to
// load. It defers response processing until all URL checks are completed;
// cancels the load if any URLs turn out to be bad.
class RendererURLLoaderThrottle : public blink::URLLoaderThrottle {
 public:
  // |safe_browsing| must stay alive until WillStartRequest() (if it is called)
  // or the end of this object.
  // |local_frame_token| is used for displaying SafeBrowsing UI when necessary.
  RendererURLLoaderThrottle(
      mojom::SafeBrowsing* safe_browsing,
      base::optional_ref<const blink::LocalFrameToken> local_frame_token);
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // |extension_web_request_reporter_pending_remote| is used for sending
  // extension web requests to the browser.
  RendererURLLoaderThrottle(
      mojom::SafeBrowsing* safe_browsing,
      base::optional_ref<const blink::LocalFrameToken> local_frame_token,
      mojom::ExtensionWebRequestReporter* extension_web_request_reporter);
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

  void OnCheckUrlResult(
      bool proceed,
      bool showed_interstitial);

  void OnMojoDisconnect();

  // TODO(crbug.com/324108312): Remove `safe_browsing_`, `frame_token_`,
  // `safe_browsing_pending_remote_`, `safe_browsing_remote_` that are unused.
  raw_ptr<mojom::SafeBrowsing, DanglingUntriaged> safe_browsing_;
  const std::optional<blink::LocalFrameToken> frame_token_;

  // These fields hold the connection to this instance's private connection to
  // the Safe Browsing service if DetachFromCurrentThread has been called.
  mojo::PendingRemote<mojom::SafeBrowsing> safe_browsing_pending_remote_;
  mojo::Remote<mojom::SafeBrowsing> safe_browsing_remote_;

  mojo::Remote<mojom::SafeBrowsingUrlChecker> url_checker_;

  size_t pending_checks_ = 0;
  bool blocked_ = false;

  bool deferred_ = false;

  GURL original_url_;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Bind the pipe created in DetachFromCurrentSequence to the current
  // sequence.
  void BindExtensionWebRequestReporterPipeIfDetached();

  // Send web request data to the browser if the request
  // originated from an extension and destination is HTTP/HTTPS scheme only.
  void MaybeSendExtensionWebRequestData(network::ResourceRequest* request);

  raw_ptr<mojom::ExtensionWebRequestReporter, DanglingUntriaged>
      extension_web_request_reporter_;
  mojo::PendingRemote<mojom::ExtensionWebRequestReporter>
      extension_web_request_reporter_pending_remote_;
  mojo::Remote<mojom::ExtensionWebRequestReporter>
      extension_web_request_reporter_remote_;
  // Tracks if the request originated from an extension, used during redirects
  // to send web request data to the telemetry service.
  std::string origin_extension_id_;
  bool initiated_from_content_script_ = false;
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

  base::WeakPtrFactory<RendererURLLoaderThrottle> weak_factory_{this};
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_RENDERER_RENDERER_URL_LOADER_THROTTLE_H_
