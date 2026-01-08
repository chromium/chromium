// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/theme_colors_source_manager.h"

#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/check_is_test.h"
#include "base/debug/dump_without_crashing.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/loader/local_resource_loader_config.mojom.h"
#include "ui/base/ui_base_types.h"
#include "ui/color/color_provider.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

constexpr char kThemeColorsCssUrl[] =
    "chrome://theme/colors.css?sets=ui,chrome";

}  // namespace

ThemeColorsSourceManager::ThemeColorsSourceManager(Profile* profile)
    : profile_(profile) {}

ThemeColorsSourceManager::~ThemeColorsSourceManager() = default;

void ThemeColorsSourceManager::PopulateLocalResourceLoaderConfig(
    blink::mojom::LocalResourceLoaderConfig* config,
    const url::Origin& requesting_origin,
    content::WebContents* web_contents) {
  CHECK(web_contents);
  const ui::ColorProvider* color_provider = nullptr;
  // TODO(crbug.com/457618790): Ensure we have ColorProviders available early in
  // View init separately.
  if (color_provider_for_testing_) {
    CHECK_IS_TEST();
    color_provider = color_provider_for_testing_.get();
  } else {
    // Use the Widget's ColorProvider to ensure matching the window's theme.
    // TODO(crbug.com/457618790): Explore making the browser early via
    // webui::GetBrowserWindowInterface().
    auto* browser_window =
        BrowserWindow::FindBrowserWindowWithWebContents(web_contents);
    if (browser_window) {
      color_provider = browser_window->GetColorProvider();
    }
    // Fallback to ThemeService if we couldn't get the ColorProvider from the
    // BrowserWindow. We prioritize the BrowserWindow's ColorProvider to ensure
    // consistency with the native window's theme, but during early platform
    // initialization, e.g. before the native view is fully attached, the
    // BrowserWindow might not be available yet.
    if (!color_provider) {
      auto* theme_service = ThemeServiceFactory::GetForProfile(profile_);
      if (theme_service) {
        color_provider = theme_service->GetColorProvider();
        // Should not happen in normal operation.
        base::debug::DumpWithoutCrashing();
      }
    }
  }
  CHECK(color_provider);

  PopulateLocalResourceLoaderConfig(config, requesting_origin, *color_provider);
}

void ThemeColorsSourceManager::PopulateLocalResourceLoaderConfig(
    blink::mojom::LocalResourceLoaderConfig* config,
    const url::Origin& requesting_origin,
    const ui::ColorProvider& color_provider) {
  const auto* theme_service =
      ThemeServiceFactory::GetForProfile(profile_->GetOriginalProfile());
  CHECK(theme_service);
  GURL theme_url(kThemeColorsCssUrl);

  std::optional<std::string> css_content = ThemeSource::GenerateColorsCss(
      color_provider, theme_url, theme_service->GetIsGrayscale(),
      theme_service->GetIsBaseline());

  if (!css_content) {
    return;
  }

  // Add the generated CSS to the config.
  url::Origin theme_origin = url::Origin::Create(theme_url);
  auto& source = config->sources[theme_origin];
  if (source.is_null()) {
    source = blink::mojom::LocalResourceSource::New();
  }

  // Allow the requesting origin to access this resource.
  // Since the resource is served from `chrome://theme/`, we must explicitly
  // allow the `requesting_origin` via Access-Control-Allow-Origin if it
  // differs.
  if (!requesting_origin.IsSameOriginWith(theme_origin)) {
    source->headers =
        "Access-Control-Allow-Origin: " + requesting_origin.Serialize();
  }

  auto resource_path = std::string(theme_url.path());
  if (!resource_path.empty() && resource_path[0] == '/') {
    resource_path = resource_path.substr(1);
  }

  source->path_to_resource_map[resource_path] =
      blink::mojom::LocalResourceValue::NewResponseBody(
          std::move(*css_content));
}

void ThemeColorsSourceManager::SetColorProviderForTesting(
    const ui::ColorProvider* color_provider) {
  color_provider_for_testing_ = color_provider;
}
