// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TOP_CHROME_TOP_CHROME_WEBUI_CONFIG_H_
#define CHROME_BROWSER_UI_WEBUI_TOP_CHROME_TOP_CHROME_WEBUI_CONFIG_H_

#include <string>
#include <string_view>

#include "base/logging.h"
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

  // Common Top Chrome WebUI properties -------------------------------

  // Returns the WebUI name used for logging metrics.
  virtual std::string GetWebUIName() = 0;

  // Returns true if the host should automatically resize to fit the page size.
  virtual bool ShouldAutoResizeHost() = 0;
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
    return std::make_unique<T>(web_ui);
  }
};

#endif  // CHROME_BROWSER_UI_WEBUI_TOP_CHROME_TOP_CHROME_WEBUI_CONFIG_H_
