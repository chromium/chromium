// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_MANIFEST_WEB_APP_BROWSER_CONTROLLER_H_
#define CHROME_BROWSER_UI_MANIFEST_WEB_APP_BROWSER_CONTROLLER_H_

#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chrome/browser/extensions/extension_uninstall_dialog.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/skia/include/core/SkColor.h"

class Browser;

namespace blink {
struct Manifest;
}

namespace gfx {
class ImageSkia;
}

// Class to encapsulate logic to control the browser UI for manifest based web
// apps or focus mode.
class ManifestWebAppBrowserController : public web_app::AppBrowserController {
 public:
  explicit ManifestWebAppBrowserController(Browser* browser);
  ~ManifestWebAppBrowserController() override;

  // web_app::AppBrowserController:
  bool HasMinimalUiButtons() const override;
  bool ShouldShowCustomTabBar() const override;
  gfx::ImageSkia GetWindowAppIcon() const override;
  gfx::ImageSkia GetWindowIcon() const override;
  std::u16string GetAppShortName() const override;
  std::u16string GetFormattedUrlOrigin() const override;
  GURL GetAppStartUrl() const override;
  bool IsUrlInAppScope(const GURL& url) const override;

 protected:
  // web_app::AppBrowserController:
  void OnTabInserted(content::WebContents* contents) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(ManifestWebAppBrowserControllerTest, IsInScope);
  void OnManifestLoaded(const GURL& manifest_url,
                        const blink::Manifest& manifest);

  static bool IsInScope(const GURL& url, const GURL& scope);

  GURL app_start_url_;
  GURL manifest_scope_;
  base::WeakPtrFactory<ManifestWebAppBrowserController> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_MANIFEST_WEB_APP_BROWSER_CONTROLLER_H_
