// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_LAUNCH_PARAMS_HELPER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_LAUNCH_PARAMS_HELPER_H_

#include <vector>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/gurl.h"

namespace content {

class WebContents;
class NavigationHandle;

}  // namespace content

namespace web_app {

class WebAppRegistrar;

// A helper for sending launch paths to the renderer process.
//
// If `await_navigation` is set, it waits for the first DidFinishNavigation
// before enqueuing launch params, and otherwise enqueues them immediately.
// DidFinishNavigation takes into account server redirects. Will re-enqueue if
// the page reloads without navigating away.
//
// Note: The lifetime of this class is tied to the WebContents it is attached
// to. However, it will often destroy itself before the WebContents, such as
// when the WebContents navigates away from the initial URL.
class WebLaunchParamsHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<WebLaunchParamsHelper> {
 public:
  WEB_CONTENTS_USER_DATA_KEY_DECL();

  WebLaunchParamsHelper(const WebLaunchParamsHelper&) = delete;
  WebLaunchParamsHelper& operator=(const WebLaunchParamsHelper&) = delete;

  ~WebLaunchParamsHelper() override;

  static WebLaunchParamsHelper* GetForWebContents(
      content::WebContents* web_contents);

  // Enqueues a LaunchParams into the `web_contents` with the provided launch_*
  // params. Only enqueues into pages inside `app_scope`. Will enqueue
  // immediately unless `await_navigation` is set in which case it waits for the
  // next DidFinishNavigation event. Only system web apps may provide a
  // |launch_dir|.
  static void EnqueueLaunchParams(content::WebContents* web_contents,
                                  const WebAppRegistrar& web_app_registrar,
                                  AppId app_id,
                                  bool await_navigation,
                                  GURL launch_url,
                                  base::FilePath launch_dir,
                                  std::vector<base::FilePath> launch_paths);

  const std::vector<base::FilePath>& launch_paths() { return launch_paths_; }

 private:
  WebLaunchParamsHelper(content::WebContents* web_contents,
                        const WebAppRegistrar& web_app_registrar,
                        AppId app_id,
                        GURL launch_url,
                        base::FilePath launch_dir,
                        std::vector<base::FilePath> launch_paths);

  void Start(bool await_navigation);

  // content::WebContentsObserver:
  void DidFinishNavigation(content::NavigationHandle* handle) override;

  // Sends the launch entries to the renderer if they have been created and the
  // renderer is ready to receive them.
  void MaybeSendLaunchEntries();

  // Send the launch entries to the renderer.
  void SendLaunchEntries();

  // Closes the app window/tab. `this` will be deleted, return immediately after
  // calling.
  void CloseApp();

  // Removes self from the parent WebContents. `this` will be deleted, return
  // immediately after calling.
  void DestroySelf();

  const WebAppRegistrar& web_app_registrar_;
  const AppId app_id_;

  // The URL the launch entries are for. Note that redirects may cause us to
  // enqueue in a different URL, we still report the original launch target URL
  // in the launch params.
  GURL launch_url_;

  // The directory to launch with (may be empty).
  base::FilePath launch_dir_;

  // The files to launch with (may be empty).
  std::vector<base::FilePath> launch_paths_;

  // Which URL we first enqueued launch params in.
  GURL url_params_enqueued_in_;

  base::WeakPtrFactory<WebLaunchParamsHelper> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_LAUNCH_PARAMS_HELPER_H_
