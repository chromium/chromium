// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_INSTALL_BOUNCE_METRIC_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_INSTALL_BOUNCE_METRIC_H_

#include <optional>

#include "base/time/time.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/common/web_app_id.h"

class PrefService;
class PrefRegistrySimple;

namespace web_app {

void SetInstallBounceMetricTimeForTesting(std::optional<base::Time> time);

void RegisterInstallBounceMetricProfilePrefs(PrefRegistrySimple* registry);

void RecordWebAppInstallationTimestamp(
    PrefService* pref_service,
    const webapps::AppId& app_id,
    webapps::WebappInstallSource install_source);

void RecordWebAppUninstallation(PrefService* pref_service,
                                const webapps::AppId& app_id);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_INSTALL_BOUNCE_METRIC_H_
