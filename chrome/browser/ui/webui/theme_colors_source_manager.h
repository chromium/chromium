// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_THEME_COLORS_SOURCE_MANAGER_H_
#define CHROME_BROWSER_UI_WEBUI_THEME_COLORS_SOURCE_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "third_party/blink/public/mojom/loader/local_resource_loader_config.mojom-forward.h"
#include "url/gurl.h"
#include "url/origin.h"

class Profile;

namespace content {
class WebContents;
}

namespace ui {
class ColorProvider;
}

// ThemeColorsSourceManager manages the generation and serving of theme color
// CSS for WebUIs. This centralizes the logic for injecting
// `chrome://theme/colors.css` into the `LocalResourceLoaderConfig`, allowing
// WebUIs to load theme colors synchronously or with optimized networking,
// avoiding IPC round-trips.
class ThemeColorsSourceManager : public KeyedService {
 public:
  explicit ThemeColorsSourceManager(Profile* profile);
  ~ThemeColorsSourceManager() override;

  ThemeColorsSourceManager(const ThemeColorsSourceManager&) = delete;
  ThemeColorsSourceManager& operator=(const ThemeColorsSourceManager&) = delete;

  // Populates `config` with the calculated theme colors CSS, enabling direct
  // loading from the renderer.
  // `requesting_origin`: The origin of the WebUI requesting the resource. This
  // is used to configure Access-Control-Allow-Origin headers, permitting the
  // WebUI to access the resource cross-origin.
  // `web_contents`: The WebContents associated with the WebUI. This is used to
  // find the relevant BrowserWindow and its ColorProvider, ensuring the colors
  // match the window's specific theme instance.
  void PopulateLocalResourceLoaderConfig(
      blink::mojom::LocalResourceLoaderConfig* config,
      const url::Origin& requesting_origin,
      content::WebContents* web_contents);

  // Sets a custom ColorProvider for testing. This allows tests to simulate
  // specific theme colors without needing a full Browser/BrowserWindow.
  void SetColorProviderForTesting(const ui::ColorProvider* color_provider);

 private:
  void PopulateLocalResourceLoaderConfig(
      blink::mojom::LocalResourceLoaderConfig* config,
      const url::Origin& requesting_origin,
      const ui::ColorProvider& color_provider);

  raw_ptr<Profile> profile_;
  raw_ptr<const ui::ColorProvider> color_provider_for_testing_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_WEBUI_THEME_COLORS_SOURCE_MANAGER_H_
