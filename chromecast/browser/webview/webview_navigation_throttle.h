// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_WEBVIEW_WEBVIEW_NAVIGATION_THROTTLE_H_
#define CHROMECAST_BROWSER_WEBVIEW_WEBVIEW_NAVIGATION_THROTTLE_H_

#include "base/macros.h"
#include "base/sequenced_task_runner.h"
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
                            WebviewController* controller);

  ~WebviewNavigationThrottle() override;

  ThrottleCheckResult WillStartRequest() override;
  const char* GetNameForLogging() override;

 protected:
  friend class WebviewController;
  void ProcessNavigationDecision(webview::NavigationDecision decision);

 private:
  scoped_refptr<base::SequencedTaskRunner> response_task_runner_;

  const GURL url_;
  bool is_in_main_frame_;
  WebviewController* controller_;

  DISALLOW_COPY_AND_ASSIGN(WebviewNavigationThrottle);
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_WEBVIEW_WEBVIEW_NAVIGATION_THROTTLE_H_
