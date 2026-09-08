// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_ARC_OVERVIEW_TRACING_ARC_OVERVIEW_TRACING_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_ARC_OVERVIEW_TRACING_ARC_OVERVIEW_TRACING_UI_H_

#include <memory>
#include <string>

#include "base/memory/raw_ref.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"

class ApplicationLocaleStorage;
class GURL;

namespace content {
class WebUI;
}

namespace ash {

// WebUIConfig for chrome://arc-overview-tracing
class ArcOverviewTracingUIConfig : public content::WebUIConfig {
 public:
  // `application_locale_storage` must not be null and must outlive `this`.
  explicit ArcOverviewTracingUIConfig(
      const ApplicationLocaleStorage* application_locale_storage);

  ArcOverviewTracingUIConfig(const ArcOverviewTracingUIConfig&) = delete;
  ArcOverviewTracingUIConfig& operator=(const ArcOverviewTracingUIConfig&) =
      delete;
  ~ArcOverviewTracingUIConfig() override;

  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui,
      const GURL& url) override;

 private:
  const raw_ref<const ApplicationLocaleStorage> application_locale_storage_;
};

// WebUI controller for arc graphics/overview tracing.
class ArcOverviewTracingUI : public content::WebUIController {
 public:
  ArcOverviewTracingUI(content::WebUI* web_ui,
                       const std::string& application_locale);

  ArcOverviewTracingUI(const ArcOverviewTracingUI&) = delete;
  ArcOverviewTracingUI& operator=(const ArcOverviewTracingUI&) = delete;
  ~ArcOverviewTracingUI() override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_ARC_OVERVIEW_TRACING_ARC_OVERVIEW_TRACING_UI_H_
