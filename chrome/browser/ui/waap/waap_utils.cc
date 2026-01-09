// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/waap/waap_utils.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/waap/waap_ui_metrics_service.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/common/url_constants.h"
#include "url/gurl.h"

namespace waap {

bool IsForInitialWebUI(const GURL& url) {
  if (base::FeatureList::IsEnabled(features::kInitialWebUI) &&
      base::FeatureList::IsEnabled(features::kWebUIReloadButton)) {
    return url.SchemeIs(content::kChromeUIScheme) &&
           url.host() == chrome::kChromeUIReloadButtonHost;
  }
  return false;
}

bool IsInitialWebUIMetricsLoggingEnabled() {
  return base::FeatureList::IsEnabled(features::kInitialWebUIMetrics);
}

void RecordBrowserWindowFirstPresentation(Profile* profile,
                                          base::TimeTicks presentation_time) {
  CHECK(profile);
  if (!IsInitialWebUIMetricsLoggingEnabled()) {
    return;
  }

  static bool has_recorded = false;
  if (has_recorded) {
    return;
  }

  has_recorded = true;
  if (WaapUIMetricsService* service = WaapUIMetricsService::Get(profile)) {
    service->OnBrowserWindowFirstPresentation(presentation_time);
  }
}

}  // namespace waap
