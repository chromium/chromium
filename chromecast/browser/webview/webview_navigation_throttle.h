// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_WEBVIEW_WEBVIEW_NAVIGATION_THROTTLE_H_
#define CHROMECAST_BROWSER_WEBVIEW_WEBVIEW_NAVIGATION_THROTTLE_H_

#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chromecast/browser/webview/proto/webview.grpc.pb.h"
#include "content/public/browser/navigation_throttle.h"
#include "url/gurl.h"

namespace chromecast {

class WebviewController;

// A NavigationDelegate that defers navigation events until a NavigationDecision
// is returned over RPC.
class WebviewNavigationThrottle : public content::NavigationThrottle {
 public:
  WebviewNavigationThrottle(content::NavigationHandle* handle,
                            base::WeakPtr<WebviewController> controller);

  WebviewNavigationThrottle(const WebviewNavigationThrottle&) = delete;
  WebviewNavigationThrottle& operator=(const WebviewNavigationThrottle&) =
      delete;

  ~WebviewNavigationThrottle() override;

  ThrottleCheckResult WillStartRequest() override;
  const char* GetNameForLogging() override;

 protected:
  friend class WebviewController;
  void ProcessNavigationDecision(webview::NavigationDecision decision);

 private:
  base::WeakPtr<WebviewController> controller_;
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_WEBVIEW_WEBVIEW_NAVIGATION_THROTTLE_H_
