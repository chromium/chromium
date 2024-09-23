// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_CONTENT_METRICS_HELPER_H_
#define COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_CONTENT_METRICS_HELPER_H_

#include "components/captive_portal/core/buildflags.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "url/gurl.h"

namespace captive_portal {
class CaptivePortalService;
}

namespace history {
class HistoryService;
}

class CaptivePortalMetricsRecorder;

// This class adds metrics specific to the usage of CaptivePortalService to the
// security_interstitials::MetricsHelper.
// TODO(crbug.com/41370917): Refactor out the use of this class if possible.

// This class is meant to be used on the UI thread for captive portal metrics.
class ContentMetricsHelper : public security_interstitials::MetricsHelper {
 public:
  ContentMetricsHelper(
      history::HistoryService* history_service,
      const GURL& url,
      const security_interstitials::MetricsHelper::ReportDetails settings);

  ContentMetricsHelper(const ContentMetricsHelper&) = delete;
  ContentMetricsHelper& operator=(const ContentMetricsHelper&) = delete;

  ~ContentMetricsHelper() override;

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
  void StartRecordingCaptivePortalMetrics(
      captive_portal::CaptivePortalService* captive_portal_service,
      bool overridable);
#endif

 protected:
  // security_interstitials::MetricsHelper methods:
  void RecordExtraShutdownMetrics() override;

 private:
#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
  std::unique_ptr<CaptivePortalMetricsRecorder> captive_portal_recorder_;
#endif
};

#endif  // COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_CONTENT_METRICS_HELPER_H_
