// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAPTIVE_PORTAL_CONTENT_CAPTIVE_PORTAL_URL_LOADER_THROTTLE_H_
#define COMPONENTS_CAPTIVE_PORTAL_CONTENT_CAPTIVE_PORTAL_URL_LOADER_THROTTLE_H_

#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"

namespace captive_portal {

// CaptivePortalURLLoaderThrottle is used in the browser process to
// disable secure DNS for requests made from WebContents that belong to a
// window that was created for captive portal resolution.
class CaptivePortalURLLoaderThrottle : public blink::URLLoaderThrottle {
 public:
  explicit CaptivePortalURLLoaderThrottle(content::WebContents* web_contents);

 private:
  // blink::URLLoaderThrottle implementation.
  void WillStartRequest(network::ResourceRequest* request,
                        bool* defer) override;

  // Whether the WebContents associated with this throttle belong to a window
  // that was created for captive portal resolution.
  bool is_captive_portal_window_;
};

}  // namespace captive_portal

#endif  // COMPONENTS_CAPTIVE_PORTAL_CONTENT_CAPTIVE_PORTAL_URL_LOADER_THROTTLE_H_
