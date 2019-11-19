// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_EXTENSION_HOST_H_
#define CHROMECAST_BROWSER_CAST_EXTENSION_HOST_H_

#include "base/macros.h"
#include "base/strings/string16.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/browser/extension_host.h"

namespace content {
class BrowserContext;
class NotificationSource;
}

namespace extensions {
class Extension;
}

class GURL;

namespace chromecast {

class CastExtensionHost : public extensions::ExtensionHost,
                          public content::NotificationObserver {
 public:
  CastExtensionHost(content::BrowserContext* browser_context,
                    const extensions::Extension* extension,
                    const GURL& initial_url,
                    scoped_refptr<content::SiteInstance> site_instance);
  ~CastExtensionHost() override;

  // extensions::ExtensionHost implementation:
  bool IsBackgroundPage() const override;
  void OnDidStopFirstLoad() override;
  void LoadInitialURL() override;
  void ActivateContents(content::WebContents* contents) override;
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  bool DidAddMessageToConsole(content::WebContents* source,
                              blink::mojom::ConsoleMessageLevel log_level,
                              const base::string16& message,
                              int32_t line_no,
                              const base::string16& source_id) override;
  void EnterFullscreenModeForTab(
      content::WebContents* web_contents,
      const GURL& origin,
      const blink::mojom::FullscreenOptions& options) override;
  void ExitFullscreenModeForTab(content::WebContents*) override;
  bool IsFullscreenForTabOrPending(
      const content::WebContents* web_contents) override;

 private:
  void SetFullscreen(content::WebContents* web_contents, bool value);

  // content::NotificationObserver implementation:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  content::NotificationRegistrar registrar_;
  content::BrowserContext* const browser_context_;
  bool is_fullscreen_ = false;

  DISALLOW_COPY_AND_ASSIGN(CastExtensionHost);
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_EXTENSION_HOST_H_
