// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_LAUNCH_QUEUE_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_LAUNCH_QUEUE_H_

#include <vector>

#include "base/memory/raw_ref.h"
#include "chrome/browser/web_applications/web_app_launch_params.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/web_contents_observer.h"

class GURL;

namespace content {

class WebContents;
class NavigationHandle;

}  // namespace content

namespace web_app {

class WebAppRegistrar;

// This handles passing WebAppLaunchParams through to its WebContents.
// There are three scenarios in which launch params are sent to a WebContents:
// - Navigating launches: The launch params are stored until the navigation
//   completes.
//
// - Non-navigating launches: The launch params are sent immediately to the
//   WebContents unless there was a navigating launch still pending, then it
//   gets added to the queue.
//
// - Page reloads: The last launch params sent to the page get resent.
//   Note that this is not in the spec and a bit magical. This is to cater for
//   the scenario where a user opens a web app via a file handler which provides
//   a file handle to the app. Without this reload mechanism the page would lose
//   access to the file handle if the user were to refresh the page.
class WebAppLaunchQueue : public content::WebContentsObserver {
 public:
  WebAppLaunchQueue(content::WebContents* web_contents,
                    const WebAppRegistrar& registrar);

  WebAppLaunchQueue(const WebAppLaunchQueue&) = delete;
  WebAppLaunchQueue& operator=(const WebAppLaunchQueue&) = delete;

  ~WebAppLaunchQueue() override;

  void Enqueue(WebAppLaunchParams launch_params);

  const webapps::AppId* GetPendingLaunchAppId() const;

 private:
  bool IsInScope(const WebAppLaunchParams& launch_params,
                 const GURL& current_url);

  // Reset self back to the initial state.
  void Reset();

  // content::WebContentsObserver:
  void DidFinishNavigation(content::NavigationHandle* handle) override;

  void SendQueuedLaunchParams(const GURL& current_url);
  void SendLaunchParams(WebAppLaunchParams launch_params,
                        const GURL& current_url);

  const raw_ref<const WebAppRegistrar> registrar_;

  // Launch params queued up to be sent to the WebContents.
  std::vector<WebAppLaunchParams> queue_;

  // Whether to send the queue of launch params on the next navigation.
  bool pending_navigation_ = false;

  // A copy of the last sent launch params ready to resend should the user
  // reload the page.
  std::optional<WebAppLaunchParams> last_sent_queued_launch_params_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_LAUNCH_QUEUE_H_
