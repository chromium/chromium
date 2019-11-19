// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APPS_CHROME_APP_DELEGATE_H_
#define CHROME_BROWSER_UI_APPS_CHROME_APP_DELEGATE_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/browser/app_window/app_delegate.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/geometry/rect.h"

class ScopedKeepAlive;

class ChromeAppDelegate : public extensions::AppDelegate,
                          public content::NotificationObserver {
 public:
  // Params:
  //   keep_alive: Whether this object should keep the browser alive.
  explicit ChromeAppDelegate(bool keep_alive);
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
  void RenderViewCreated(content::RenderViewHost* render_view_host) override;
  void ResizeWebContents(content::WebContents* web_contents,
                         const gfx::Size& size) override;
  content::WebContents* OpenURLFromTab(
      content::BrowserContext* context,
      content::WebContents* source,
      const content::OpenURLParams& params) override;
  void AddNewContents(content::BrowserContext* context,
                      std::unique_ptr<content::WebContents> new_contents,
                      WindowOpenDisposition disposition,
                      const gfx::Rect& initial_rect,
                      bool user_gesture) override;
  content::ColorChooser* ShowColorChooser(content::WebContents* web_contents,
                                          SkColor initial_color) override;
  void RunFileChooser(content::RenderFrameHost* render_frame_host,
                      std::unique_ptr<content::FileSelectListener> listener,
                      const blink::mojom::FileChooserParams& params) override;
  void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback,
      const extensions::Extension* extension) override;
  bool CheckMediaAccessPermission(
      content::RenderFrameHost* render_frame_host,
      const GURL& security_origin,
      blink::mojom::MediaStreamType type,
      const extensions::Extension* extension) override;
  int PreferredIconSize() const override;
  void SetWebContentsBlocked(content::WebContents* web_contents,
                             bool blocked) override;
  bool IsWebContentsVisible(content::WebContents* web_contents) override;
  void SetTerminatingCallback(const base::Closure& callback) override;
  void OnHide() override;
  void OnShow() override;
  bool TakeFocus(content::WebContents* web_contents, bool reverse) override;
  content::PictureInPictureResult EnterPictureInPicture(
      content::WebContents* web_contents,
      const viz::SurfaceId& surface_id,
      const gfx::Size& natural_size) override;
  void ExitPictureInPicture() override;

  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  bool has_been_shown_;
  bool is_hidden_;
  bool for_lock_screen_app_;
  std::unique_ptr<ScopedKeepAlive> keep_alive_;
  std::unique_ptr<NewWindowContentsDelegate> new_window_contents_delegate_;
  base::Closure terminating_callback_;
  content::NotificationRegistrar registrar_;
  base::WeakPtrFactory<ChromeAppDelegate> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ChromeAppDelegate);
};

#endif  // CHROME_BROWSER_UI_APPS_CHROME_APP_DELEGATE_H_
