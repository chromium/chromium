// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/installable/installable_metrics.h"

#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "components/webapps/browser/webapps_client.h"
#include "content/public/browser/web_contents.h"

namespace webapps {

// static
void InstallableMetrics::TrackInstallEvent(WebappInstallSource source) {
  DCHECK(IsReportableInstallSource(source));
  UMA_HISTOGRAM_ENUMERATION("Webapp.Install.InstallEvent", source,
                            WebappInstallSource::COUNT);
}

// static
bool InstallableMetrics::IsReportableInstallSource(WebappInstallSource source) {
  return source == WebappInstallSource::MENU_BROWSER_TAB ||
         source == WebappInstallSource::MENU_CUSTOM_TAB ||
         source == WebappInstallSource::AUTOMATIC_PROMPT_BROWSER_TAB ||
         source == WebappInstallSource::AUTOMATIC_PROMPT_CUSTOM_TAB ||
         source == WebappInstallSource::API_BROWSER_TAB ||
         source == WebappInstallSource::API_CUSTOM_TAB ||
         source == WebappInstallSource::DEVTOOLS ||
         source == WebappInstallSource::AMBIENT_BADGE_BROWSER_TAB ||
         source == WebappInstallSource::AMBIENT_BADGE_CUSTOM_TAB ||
         source == WebappInstallSource::ARC ||
         source == WebappInstallSource::INTERNAL_DEFAULT ||
         source == WebappInstallSource::EXTERNAL_DEFAULT ||
         source == WebappInstallSource::EXTERNAL_POLICY ||
         source == WebappInstallSource::SYSTEM_DEFAULT ||
         source == WebappInstallSource::OMNIBOX_INSTALL_ICON ||
         source == WebappInstallSource::MENU_CREATE_SHORTCUT;
}

// static
bool InstallableMetrics::IsUserInitiatedInstallSource(
    WebappInstallSource source) {
  switch (source) {
    case WebappInstallSource::MENU_BROWSER_TAB:
    case WebappInstallSource::MENU_CUSTOM_TAB:
    case WebappInstallSource::AUTOMATIC_PROMPT_BROWSER_TAB:
    case WebappInstallSource::AUTOMATIC_PROMPT_CUSTOM_TAB:
    case WebappInstallSource::API_BROWSER_TAB:
    case WebappInstallSource::API_CUSTOM_TAB:
    case WebappInstallSource::AMBIENT_BADGE_BROWSER_TAB:
    case WebappInstallSource::AMBIENT_BADGE_CUSTOM_TAB:
    case WebappInstallSource::ARC:
    case WebappInstallSource::OMNIBOX_INSTALL_ICON:
    case WebappInstallSource::MENU_CREATE_SHORTCUT:
      return true;
    case WebappInstallSource::DEVTOOLS:
    case WebappInstallSource::MANAGEMENT_API:
    case WebappInstallSource::INTERNAL_DEFAULT:
    case WebappInstallSource::EXTERNAL_DEFAULT:
    case WebappInstallSource::EXTERNAL_POLICY:
    case WebappInstallSource::SYSTEM_DEFAULT:
    case WebappInstallSource::SYNC:
      return false;
    case WebappInstallSource::COUNT:
      NOTREACHED();
      return false;
  }
}

// static
WebappInstallSource InstallableMetrics::GetInstallSource(
    content::WebContents* web_contents,
    InstallTrigger trigger) {
  return WebappsClient::Get()->GetInstallSource(web_contents, trigger);
}

// static
void InstallableMetrics::RecordCheckServiceWorkerTime(base::TimeDelta time) {
  UMA_HISTOGRAM_MEDIUM_TIMES("Webapp.CheckServiceWorker.Time", time);
}

// static
void InstallableMetrics::RecordCheckServiceWorkerStatus(
    ServiceWorkerOfflineCapability status) {
  UMA_HISTOGRAM_ENUMERATION("Webapp.CheckServiceWorker.Status", status);
}

// static
ServiceWorkerOfflineCapability
InstallableMetrics::ConvertFromServiceWorkerCapability(
    content::ServiceWorkerCapability capability) {
  switch (capability) {
    case content::ServiceWorkerCapability::SERVICE_WORKER_WITH_FETCH_HANDLER:
      return ServiceWorkerOfflineCapability::kServiceWorkerWithOfflineSupport;
    case content::ServiceWorkerCapability::SERVICE_WORKER_NO_FETCH_HANDLER:
      return ServiceWorkerOfflineCapability::kServiceWorkerNoFetchHandler;
    case content::ServiceWorkerCapability::NO_SERVICE_WORKER:
      return ServiceWorkerOfflineCapability::kNoServiceWorker;
  }
  NOTREACHED();
}

// static
ServiceWorkerOfflineCapability InstallableMetrics::ConvertFromOfflineCapability(
    content::OfflineCapability capability) {
  switch (capability) {
    case content::OfflineCapability::kSupported:
      return ServiceWorkerOfflineCapability::kServiceWorkerWithOfflineSupport;
    case content::OfflineCapability::kUnsupported:
      return ServiceWorkerOfflineCapability::kServiceWorkerNoOfflineSupport;
  }
  NOTREACHED();
}

}  // namespace webapps
