// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APPS_CHROME_APP_DELEGATE_H_
#define CHROME_BROWSER_UI_APPS_CHROME_APP_DELEGATE_H_

#include <memory>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "extensions/browser/app_window/app_delegate.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/geometry/rect.h"

class Profile;
class ScopedKeepAlive;
class ScopedProfileKeepAlive;

class ChromeAppDelegate : public extensions::AppDelegate {
 public:
  // Params:
  //   keep_alive: Whether this object should keep the browser alive.
  explicit ChromeAppDelegate(Profile* profile, bool keep_alive);

  ChromeAppDelegate(const ChromeAppDelegate&) = delete;
  ChromeAppDelegate& operator=(const ChromeAppDelegate&) = delete;

  ~ChromeAppDelegate() override;

  static void DisableExternalOpenForTesting();

  void set_for_lock_screen_app(bool for_lock_screen_app) {
    for_lock_screen_app_ = for_lock_screen_app;
  }

 private:
  static void RelinquishKeepAliveAfterTimeout(
      const base::WeakPtr<ChromeAppDelegate>& chrome_app_delegate);

  class NewWindowContentsDelegate;

  // extensions::AppDelegate:
  void InitWebContents(content::WebContents* web_contents) override;
  void RenderFrameCreated(content::RenderFrameHost* frame_host) override;
  void ResizeWebContents(content::WebContents* web_contents,
                         const gfx::Size& size) override;
  content::WebContents* OpenURLFromTab(
      content::BrowserContext* context,
      content::WebContents* source,
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::NavigationHandle&)>
          navigation_handle_callback) override;
  void AddNewContents(content::BrowserContext* context,
                      std::unique_ptr<content::WebContents> new_contents,
                      const GURL& target_url,
                      WindowOpenDisposition disposition,
                      const blink::mojom::WindowFeatures& window_features,
                      bool user_gesture) override;
  void RunFileChooser(content::RenderFrameHost* render_frame_host,
                      scoped_refptr<content::FileSelectListener> listener,
                      const blink::mojom::FileChooserParams& params) override;
  void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback,
      const extensions::Extension* extension) override;
  bool CheckMediaAccessPermission(
      content::RenderFrameHost* render_frame_host,
      const url::Origin& security_origin,
      blink::mojom::MediaStreamType type,
      const extensions::Extension* extension) override;
  int PreferredIconSize() const override;
  void SetWebContentsBlocked(content::WebContents* web_contents,
                             bool blocked) override;
  bool IsWebContentsVisible(content::WebContents* web_contents) override;
  void SetTerminatingCallback(base::OnceClosure callback) override;
  void OnHide() override;
  void OnShow() override;
  bool TakeFocus(content::WebContents* web_contents, bool reverse) override;
  content::PictureInPictureResult EnterPictureInPicture(
      content::WebContents* web_contents) override;
  void ExitPictureInPicture() override;

  void OnAppTerminating();

  bool has_been_shown_;
  bool is_hidden_;
  bool for_lock_screen_app_;
  const raw_ptr<Profile> profile_;
  std::unique_ptr<ScopedKeepAlive> keep_alive_;
  std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive_;
  std::unique_ptr<NewWindowContentsDelegate> new_window_contents_delegate_;
  base::OnceClosure terminating_callback_;
  base::CallbackListSubscription subscription_;
  base::WeakPtrFactory<ChromeAppDelegate> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_APPS_CHROME_APP_DELEGATE_H_
