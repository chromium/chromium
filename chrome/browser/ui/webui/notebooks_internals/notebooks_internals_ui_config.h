// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NOTEBOOKS_INTERNALS_NOTEBOOKS_INTERNALS_UI_CONFIG_H_
#define CHROME_BROWSER_UI_WEBUI_NOTEBOOKS_INTERNALS_NOTEBOOKS_INTERNALS_UI_CONFIG_H_

#include "content/public/browser/internal_webui_config.h"

namespace notebooks {

class NotebooksInternalsUIConfig : public content::InternalWebUIConfig {
 public:
  NotebooksInternalsUIConfig();
  ~NotebooksInternalsUIConfig() override;

  // content::InternalWebUIConfig:
  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui,
      const GURL& url) override;
};

}  // namespace notebooks

#endif  // CHROME_BROWSER_UI_WEBUI_NOTEBOOKS_INTERNALS_NOTEBOOKS_INTERNALS_UI_CONFIG_H_
