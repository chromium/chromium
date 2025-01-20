// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_HISTORY_CLUSTERS_HISTORY_CLUSTERS_INTERNALS_UI_CONFIG_H_
#define CHROME_BROWSER_UI_WEBUI_HISTORY_CLUSTERS_HISTORY_CLUSTERS_INTERNALS_UI_CONFIG_H_

#include "content/public/browser/internal_webui_config.h"
#include "content/public/common/url_constants.h"

namespace history_clusters_internals {

class HistoryClustersInternalsUIConfig : public content::InternalWebUIConfig {
 public:
  HistoryClustersInternalsUIConfig();
  ~HistoryClustersInternalsUIConfig() override;

  // content::InternalWebUIConfig:
  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui,
      const GURL& url) override;
};

}  // namespace history_clusters_internals

#endif  // CHROME_BROWSER_UI_WEBUI_HISTORY_CLUSTERS_HISTORY_CLUSTERS_INTERNALS_UI_CONFIG_H_
