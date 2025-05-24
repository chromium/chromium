// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/history_clusters/history_clusters_internals_ui_config.h"

#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history_clusters/history_clusters_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/history_clusters/history_clusters_internals/webui/history_clusters_internals_ui.h"
#include "components/history_clusters/history_clusters_internals/webui/url_constants.h"

namespace history_clusters_internals {

HistoryClustersInternalsUIConfig::HistoryClustersInternalsUIConfig()
    : InternalWebUIConfig(
          history_clusters_internals::kChromeUIHistoryClustersInternalsHost) {}

HistoryClustersInternalsUIConfig::~HistoryClustersInternalsUIConfig() = default;

std::unique_ptr<content::WebUIController>
HistoryClustersInternalsUIConfig::CreateWebUIController(content::WebUI* web_ui,
                                                        const GURL& url) {
  Profile* profile = Profile::FromWebUI(web_ui);
  return std::make_unique<HistoryClustersInternalsUI>(
      web_ui, HistoryClustersServiceFactory::GetForBrowserContext(profile),
      HistoryServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS));
}

}  // namespace history_clusters_internals
