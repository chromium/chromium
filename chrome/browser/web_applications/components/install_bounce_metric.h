// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_INSTALL_BOUNCE_METRIC_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_INSTALL_BOUNCE_METRIC_H_

#include "base/macros.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "chrome/browser/installable/installable_metrics.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"

class PrefService;
class PrefRegistrySimple;

namespace web_app {

void SetInstallBounceMetricTimeForTesting(base::Optional<base::Time> time);

void RegisterInstallBounceMetricProfilePrefs(PrefRegistrySimple* registry);

void RecordWebAppInstallationTimestamp(PrefService* pref_service,
                                       const AppId& app_id,
                                       WebappInstallSource install_source);

void RecordWebAppUninstallation(PrefService* pref_service, const AppId& app_id);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_INSTALL_BOUNCE_METRIC_H_
