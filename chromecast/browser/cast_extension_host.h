// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_EXTENSION_HOST_H_
#define CHROMECAST_BROWSER_CAST_EXTENSION_HOST_H_

#include "base/macros.h"
#include "base/strings/string16.h"
#include "chromecast/browser/cast_web_view.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_host.h"

namespace content {
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
                    CastWebView::Delegate* delegate,
                    const extensions::Extension* extension,
                    const GURL& initial_url,
                    content::SiteInstance* site_instance,
                    extensions::ViewType host_type);
  ~CastExtensionHost() override;

  // extensions::ExtensionHost implementation:
  bool IsBackgroundPage() const override;
  void OnDidStopFirstLoad() override;
  void LoadInitialURL() override;
  void ActivateContents(content::WebContents* contents) override;
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  bool DidAddMessageToConsole(content::WebContents* source,
                              int32_t level,
                              const base::string16& message,
                              int32_t line_no,
                              const base::string16& source_id) override;

 private:
  // content::NotificationObserver implementation:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  content::NotificationRegistrar registrar_;
  content::BrowserContext* const browser_context_;
  CastWebView::Delegate* const delegate_;

  DISALLOW_COPY_AND_ASSIGN(CastExtensionHost);
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_EXTENSION_HOST_H_
