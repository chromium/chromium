// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/installable/installable_metrics.h"

#include <atomic>
#include <ostream>

#include "base/check.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "components/webapps/browser/webapps_client.h"
#include "content/public/browser/service_worker_context.h"

namespace webapps {

std::ostream& operator<<(std::ostream& os, WebappInstallSource source) {
  switch (source) {
    case WebappInstallSource::MENU_BROWSER_TAB:
      return os << "menu browser tab";
    case WebappInstallSource::MENU_CUSTOM_TAB:
      return os << "menu custom tab";
    case WebappInstallSource::AUTOMATIC_PROMPT_BROWSER_TAB:
      return os << "automatic prompt browser tab";
    case WebappInstallSource::AUTOMATIC_PROMPT_CUSTOM_TAB:
      return os << "automatic prompt custom tab";
    case WebappInstallSource::API_BROWSER_TAB:
      return os << "api browser tab";
    case WebappInstallSource::API_CUSTOM_TAB:
      return os << "api custom tab";
    case WebappInstallSource::DEVTOOLS:
      return os << "devtools";
    case WebappInstallSource::MANAGEMENT_API:
      return os << "management api";
    case WebappInstallSource::AMBIENT_BADGE_BROWSER_TAB:
      return os << "ambient badge browser tab";
    case WebappInstallSource::AMBIENT_BADGE_CUSTOM_TAB:
      return os << "ambient badge custom tab";
    case WebappInstallSource::ARC:
      return os << "arc";
    case WebappInstallSource::INTERNAL_DEFAULT:
      return os << "internal default";
    case WebappInstallSource::EXTERNAL_DEFAULT:
      return os << "external default";
    case WebappInstallSource::EXTERNAL_POLICY:
      return os << "external policy";
    case WebappInstallSource::SYSTEM_DEFAULT:
      return os << "system default";
    case WebappInstallSource::OMNIBOX_INSTALL_ICON:
      return os << "omnibox install icon";
    case WebappInstallSource::SYNC:
      return os << "sync";
    case WebappInstallSource::MENU_CREATE_SHORTCUT:
      return os << "menu create shortcut";
    case WebappInstallSource::SUB_APP:
      return os << "sub app";
    case WebappInstallSource::CHROME_SERVICE:
      return os << "chrome service";
    case WebappInstallSource::RICH_INSTALL_UI_WEBLAYER:
      return os << "rich install ui weblayer";
    case WebappInstallSource::KIOSK:
      return os << "kiosk";
    case WebappInstallSource::ISOLATED_APP_DEV_INSTALL:
      return os << "isolated app dev install";
    case WebappInstallSource::EXTERNAL_LOCK_SCREEN:
      return os << "external lock screen";
    case WebappInstallSource::PRELOADED_OEM:
      return os << "preloaded oem";
    case WebappInstallSource::MICROSOFT_365_SETUP:
      return os << "microsoft 365 setup";
    case WebappInstallSource::PROFILE_MENU:
      return os << "profile menu";
    case WebappInstallSource::ML_PROMOTION:
      return os << "ml promotion";
    case WebappInstallSource::PRELOADED_DEFAULT:
      return os << "preloaded default";
    case WebappInstallSource::COUNT:
      return os << "count";
  }
}

std::ostream& operator<<(std::ostream& os, WebappUninstallSource source) {
  switch (source) {
    case webapps::WebappUninstallSource::kUnknown:
      return os << "Unknown";
    case webapps::WebappUninstallSource::kAppMenu:
      return os << "AppMenu";
    case webapps::WebappUninstallSource::kAppsPage:
      return os << "AppsPage";
    case webapps::WebappUninstallSource::kOsSettings:
      return os << "OS Settings";
    case webapps::WebappUninstallSource::kSync:
      return os << "Sync";
    case webapps::WebappUninstallSource::kAppManagement:
      return os << "App Management";
    case webapps::WebappUninstallSource::kMigration:
      return os << "Migration";
    case webapps::WebappUninstallSource::kAppList:
      return os << "App List";
    case webapps::WebappUninstallSource::kShelf:
      return os << "Shelf";
    case webapps::WebappUninstallSource::kInternalPreinstalled:
      return os << "Internal Preinstalled";
    case webapps::WebappUninstallSource::kExternalPreinstalled:
      return os << "External Preinstalled";
    case webapps::WebappUninstallSource::kExternalPolicy:
      return os << "External Policy";
    case webapps::WebappUninstallSource::kSystemPreinstalled:
      return os << "System Preinstalled";
    case webapps::WebappUninstallSource::kPlaceholderReplacement:
      return os << "Placeholder Replacement";
    case webapps::WebappUninstallSource::kArc:
      return os << "Arc";
    case webapps::WebappUninstallSource::kSubApp:
      return os << "SubApp";
    case webapps::WebappUninstallSource::kStartupCleanup:
      return os << "Startup Cleanup";
    case webapps::WebappUninstallSource::kParentUninstall:
      return os << "Parent App Uninstalled";
    case webapps::WebappUninstallSource::kExternalLockScreen:
      return os << "External Lock Screen";
    case webapps::WebappUninstallSource::kTestCleanup:
      return os << "Test cleanup";
    case webapps::WebappUninstallSource::kInstallUrlDeduping:
      return os << "Install URL deduping";
    case webapps::WebappUninstallSource::kHealthcareUserInstallCleanup:
      return os << "Healthcare User Install Cleanup";
    case webapps::WebappUninstallSource::kIwaEnterprisePolicy:
      return os << "Isolated Web Apps Enterprise Policy";
  }
}

bool IsUserUninstall(WebappUninstallSource source) {
  switch (source) {
    case webapps::WebappUninstallSource::kSync:
    case webapps::WebappUninstallSource::kMigration:
    case webapps::WebappUninstallSource::kInternalPreinstalled:
    case webapps::WebappUninstallSource::kExternalPreinstalled:
    case webapps::WebappUninstallSource::kExternalPolicy:
    case webapps::WebappUninstallSource::kSystemPreinstalled:
    case webapps::WebappUninstallSource::kPlaceholderReplacement:
    case webapps::WebappUninstallSource::kArc:
    case webapps::WebappUninstallSource::kSubApp:
    case webapps::WebappUninstallSource::kStartupCleanup:
    case webapps::WebappUninstallSource::kParentUninstall:
    case webapps::WebappUninstallSource::kTestCleanup:
    case webapps::WebappUninstallSource::kInstallUrlDeduping:
    case webapps::WebappUninstallSource::kHealthcareUserInstallCleanup:
    case webapps::WebappUninstallSource::kIwaEnterprisePolicy:
      return false;
    case webapps::WebappUninstallSource::kUnknown:
    case webapps::WebappUninstallSource::kAppMenu:
    case webapps::WebappUninstallSource::kAppsPage:
    case webapps::WebappUninstallSource::kOsSettings:
    case webapps::WebappUninstallSource::kAppManagement:
    case webapps::WebappUninstallSource::kAppList:
    case webapps::WebappUninstallSource::kShelf:
    case webapps::WebappUninstallSource::kExternalLockScreen:
      return true;
  }
}

// static
void InstallableMetrics::TrackInstallEvent(WebappInstallSource source) {
  DCHECK(IsReportableInstallSource(source));
  base::UmaHistogramEnumeration("Webapp.Install.InstallEvent", source,
                                WebappInstallSource::COUNT);
}

// static
bool InstallableMetrics::IsReportableInstallSource(WebappInstallSource source) {
  switch (source) {
    case WebappInstallSource::AMBIENT_BADGE_BROWSER_TAB:
    case WebappInstallSource::AMBIENT_BADGE_CUSTOM_TAB:
    case WebappInstallSource::API_BROWSER_TAB:
    case WebappInstallSource::API_CUSTOM_TAB:
    case WebappInstallSource::ARC:
    case WebappInstallSource::AUTOMATIC_PROMPT_BROWSER_TAB:
    case WebappInstallSource::AUTOMATIC_PROMPT_CUSTOM_TAB:
    case WebappInstallSource::CHROME_SERVICE:
    case WebappInstallSource::DEVTOOLS:
    case WebappInstallSource::EXTERNAL_DEFAULT:
    case WebappInstallSource::EXTERNAL_LOCK_SCREEN:
    case WebappInstallSource::EXTERNAL_POLICY:
    case WebappInstallSource::INTERNAL_DEFAULT:
    case WebappInstallSource::KIOSK:
    case WebappInstallSource::MENU_BROWSER_TAB:
    case WebappInstallSource::MENU_CREATE_SHORTCUT:
    case WebappInstallSource::MENU_CUSTOM_TAB:
    case WebappInstallSource::MICROSOFT_365_SETUP:
    case WebappInstallSource::ML_PROMOTION:
    case WebappInstallSource::OMNIBOX_INSTALL_ICON:
    case WebappInstallSource::PRELOADED_OEM:
    case WebappInstallSource::PROFILE_MENU:
    case WebappInstallSource::RICH_INSTALL_UI_WEBLAYER:
    case WebappInstallSource::SYSTEM_DEFAULT:
    case WebappInstallSource::PRELOADED_DEFAULT:
      return true;
    case WebappInstallSource::ISOLATED_APP_DEV_INSTALL:
    case WebappInstallSource::MANAGEMENT_API:
    case WebappInstallSource::SUB_APP:
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

// static
void InstallableMetrics::TrackUninstallEvent(WebappUninstallSource source) {
  base::UmaHistogramEnumeration("Webapp.Install.UninstallEvent", source);
}

// static
void InstallableMetrics::TrackInstallResult(bool result) {
  base::UmaHistogramBoolean("WebApp.Install.Result", result);
}
}  // namespace webapps
