// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ASH_MERGE_SESSION_LOADER_THROTTLE_H_
#define CHROME_RENDERER_ASH_MERGE_SESSION_LOADER_THROTTLE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/renderer/chrome_render_thread_observer.h"
#include "chrome/renderer/chromeos_delayed_callback_group.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"

// This is used to throttle XHR resource requests on Chrome OS while the
// merge session is running (or a timeout).
class AshMergeSessionLoaderThrottle final : public blink::URLLoaderThrottle {
 public:
  static base::TimeDelta GetMergeSessionTimeout();

  explicit AshMergeSessionLoaderThrottle(
      scoped_refptr<ChromeRenderThreadObserver::ChromeOSListener>
          chromeos_listener);

  AshMergeSessionLoaderThrottle(const AshMergeSessionLoaderThrottle&) = delete;
  AshMergeSessionLoaderThrottle& operator=(
      const AshMergeSessionLoaderThrottle&) = delete;

  ~AshMergeSessionLoaderThrottle() override;

  // blink::URLLoaderThrottle:
  void WillStartRequest(network::ResourceRequest* request,
                        bool* defer) override;
  void WillRedirectRequest(
      net::RedirectInfo* redirect_info,
      const network::mojom::URLResponseHead& response_head,
      bool* defer,
      std::vector<std::string>* to_be_removed_headers,
      net::HttpRequestHeaders* modified_headers,
      net::HttpRequestHeaders* modified_cors_exempt_headers) override;

 private:
  bool MaybeDeferForMergeSession(
      const GURL& url,
      DelayedCallbackGroup::Callback resume_callback);
  void DetachFromCurrentSequence() override;
  void ResumeLoader(DelayedCallbackGroup::RunReason run_reason);

  bool is_xhr_ = false;
  scoped_refptr<ChromeRenderThreadObserver::ChromeOSListener>
      chromeos_listener_;
  base::WeakPtrFactory<AshMergeSessionLoaderThrottle> weak_ptr_factory_{this};
};

#endif  // CHROME_RENDERER_ASH_MERGE_SESSION_LOADER_THROTTLE_H_
