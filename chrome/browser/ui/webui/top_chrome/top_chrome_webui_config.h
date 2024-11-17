// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TOP_CHROME_TOP_CHROME_WEBUI_CONFIG_H_
#define CHROME_BROWSER_UI_WEBUI_TOP_CHROME_TOP_CHROME_WEBUI_CONFIG_H_

#include <optional>
#include <string>
#include <string_view>
#include <type_traits>

#include "base/functional/function_ref.h"
#include "content/public/browser/webui_config.h"

namespace content {
class WebUI;
class WebUIController;
}  // namespace content

class GURL;

// This subclass of WebUIConfig provides getters to static properties of
// top-chrome WebUIs.
class TopChromeWebUIConfig : public content::WebUIConfig {
 public:
  TopChromeWebUIConfig(std::string_view scheme, std::string_view host);
  ~TopChromeWebUIConfig() override;

  // Returns the config given its URL under a browser context.
  // Returns nullptr if `url` is not a top-chrome WebUI, or if it is
  // disabled by IsWebUIEnabled().
  static TopChromeWebUIConfig* From(content::BrowserContext* browser_context,
                                    const GURL& url);

  // Calls `on_config` for every top-chrome WebUIConfig.
  static void ForEachConfig(
      base::FunctionRef<void(TopChromeWebUIConfig*)> on_config);

  // Common Top Chrome WebUI properties -------------------------------

  // Returns the WebUI name used for logging metrics.
  virtual std::string GetWebUIName() = 0;

  // Returns true if the host should automatically resize to fit the page size.
  virtual bool ShouldAutoResizeHost() = 0;

  // Returns true to allow preloading.
  //
  // Preloading runs the WebUI in an isolated WebContents managed by
  // WebUIContentsPreloadManager. This environment lacks knowledge of its
  // embedder (e.g., side panel) and only knows the associated profile. It's
  // similar to running in a tab but without TabHelpers.
  //
  // Implications for preloaded WebUIs:
  //
  // * Data Availability: Preloading may occur during startup when some data
  //   (e.g., bookmarks) is unavailable. Preloadable WebUIs must handle this.
  // * Context Availability: Some WebUIs rely on their embedder injecting a
  //   context (e.g., TabInterface) into the WebUI controller during WebUI
  //   construction. These WebUIs assume page handles are created after this
  //   injection. This assumption is invalid for preloaded WebUIs because at
  //   preload time, there is no embedder present, and by the time the injection
  //   happens at show time, the page handler could already be created.
  // * Testability: `GetCommandIdForTesting()` must return a non-null command
  //   ID for testing purposes. This ensures preloaded WebUIs can be triggered
  //   and tested for crashes.
  // * Visibility: Do not assume user visibility upon page load. Observe
  //   `OnVisibilityChanged()` on the WebContents to track visibility.
  //
  // TODO(crbug.com/360724768): Provide context (browser, tab, etc.) to WebUIs
  // at the framework level.
  virtual bool IsPreloadable() = 0;

  // Returns the command id that can be used in tests to trigger the UI.
  // Optional if this WebUI is not preloadable.
  virtual std::optional<int> GetCommandIdForTesting() = 0;
};

template <typename T>
class DefaultTopChromeWebUIConfig : public TopChromeWebUIConfig {
 public:
  DefaultTopChromeWebUIConfig(std::string_view scheme, std::string_view host)
      : TopChromeWebUIConfig(scheme, host) {}

  bool IsWebUIEnabled(content::BrowserContext* browser_context) override {
    return true;
  }

  // TopChromeWebUIConfig:
  std::string GetWebUIName() override { return T::GetWebUIName(); }
  bool ShouldAutoResizeHost() override { return false; }
  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui,
      const GURL& url) override {
    // Disallow dual constructibility.
    // The controller can be constructed either by T(WebUI*) or
    // T(WebUI*, const GURL&), but not both.
    static_assert(std::is_constructible_v<T, content::WebUI*> ||
                  std::is_constructible_v<T, content::WebUI*, const GURL&>);
    static_assert(!(std::is_constructible_v<T, content::WebUI*> &&
                    std::is_constructible_v<T, content::WebUI*, const GURL&>));
    if constexpr (std::is_constructible_v<T, content::WebUI*>) {
      return std::make_unique<T>(web_ui);
    }
    if constexpr (std::is_constructible_v<T, content::WebUI*, const GURL&>) {
      return std::make_unique<T>(web_ui, url);
    }
  }
  bool IsPreloadable() override { return false; }
  std::optional<int> GetCommandIdForTesting() override { return std::nullopt; }
};

#endif  // CHROME_BROWSER_UI_WEBUI_TOP_CHROME_TOP_CHROME_WEBUI_CONFIG_H_
