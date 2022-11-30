// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/content/content_metrics_helper.h"

#include <memory>

#include "components/captive_portal/core/buildflags.h"
#include "components/history/core/browser/history_service.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
#include "components/security_interstitials/content/captive_portal_metrics_recorder.h"
#endif

ContentMetricsHelper::ContentMetricsHelper(
    history::HistoryService* history_service,
    const GURL& request_url,
    const security_interstitials::MetricsHelper::ReportDetails settings)
    : security_interstitials::MetricsHelper(request_url,
                                            settings,
                                            history_service) {}

ContentMetricsHelper::~ContentMetricsHelper() = default;

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
void ContentMetricsHelper::StartRecordingCaptivePortalMetrics(
    captive_portal::CaptivePortalService* captive_portal_service,
    bool overridable) {
  captive_portal_recorder_ = std::make_unique<CaptivePortalMetricsRecorder>(
      captive_portal_service, overridable);
}
#endif

void ContentMetricsHelper::RecordExtraShutdownMetrics() {
#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
  // The captive portal metrics should be recorded when the interstitial is
  // closing (or destructing).
  if (captive_portal_recorder_)
    captive_portal_recorder_->RecordCaptivePortalUMAStatistics();
#endif
}
