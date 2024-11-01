// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/history_clusters/history_clusters_internals_ui_config.h"

#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history_clusters/history_clusters_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "components/history_clusters/history_clusters_internals/webui/history_clusters_internals_ui.h"
#include "components/history_clusters/history_clusters_internals/webui/url_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"

namespace history_clusters_internals {

// TODO(crbug.com/40222519): Move SetupWebUIDataSource() to a location
// accessible from components/, such as in //ui/base/webui, so that the only
// 'wrapping' in this Config class is HistoryClustersServiceFactory.
void SetUpWebUIDataSource(content::WebUI* web_ui,
                          const char* web_ui_host,
                          base::span<const webui::ResourcePath> resources,
                          int default_resource) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(), web_ui_host);
  webui::SetupWebUIDataSource(source, resources, default_resource);
}

HistoryClustersInternalsUIConfig::HistoryClustersInternalsUIConfig()
    : WebUIConfig(
          content::kChromeUIScheme,
          history_clusters_internals::kChromeUIHistoryClustersInternalsHost) {}

HistoryClustersInternalsUIConfig::~HistoryClustersInternalsUIConfig() = default;

std::unique_ptr<content::WebUIController>
HistoryClustersInternalsUIConfig::CreateWebUIController(content::WebUI* web_ui,
                                                        const GURL& url) {
  Profile* profile = Profile::FromWebUI(web_ui);
  return std::make_unique<HistoryClustersInternalsUI>(
      web_ui, HistoryClustersServiceFactory::GetForBrowserContext(profile),
      HistoryServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS),
      base::BindOnce(
          &SetUpWebUIDataSource, web_ui,
          history_clusters_internals::kChromeUIHistoryClustersInternalsHost));
}

}  // namespace history_clusters_internals
