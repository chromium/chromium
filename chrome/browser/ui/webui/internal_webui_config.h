// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_INTERNAL_WEBUI_CONFIG_H_
#define CHROME_BROWSER_UI_WEBUI_INTERNAL_WEBUI_CONFIG_H_

#include <string>
#include <string_view>
#include <type_traits>

#include "base/functional/function_ref.h"
#include "content/public/browser/webui_config.h"

namespace content {
class WebUI;
class WebUIController;
}  // namespace content

namespace webui {

// Returns whether `url` is an internal debugging WebUI page.
bool IsInternalWebUI(const GURL& url);

// This subclass of WebUIConfig sets IsWebUIEnabled() to the value of the
// InternalWebUisEnabled pref.
class InternalWebUIConfig : public content::WebUIConfig {
 public:
  explicit InternalWebUIConfig(std::string_view host);
  ~InternalWebUIConfig() override;

  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui,
      const GURL& url) override;
};

template <typename T>
class DefaultInternalWebUIConfig : public InternalWebUIConfig {
 public:
  explicit DefaultInternalWebUIConfig(std::string_view host)
      : InternalWebUIConfig(host) {}

  // InternalWebUIConfig:
  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui,
      const GURL& url) override {
    std::unique_ptr<content::WebUIController> default_controller =
        InternalWebUIConfig::CreateWebUIController(web_ui, url);
    if (default_controller) {
      return default_controller;
    }

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
};

}  // namespace webui

#endif  // CHROME_BROWSER_UI_WEBUI_INTERNAL_WEBUI_CONFIG_H_
