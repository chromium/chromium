// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPDATES_UPDATE_METRICS_PROVIDER_H_
#define CHROME_BROWSER_UPDATES_UPDATE_METRICS_PROVIDER_H_

#include "components/metrics/metrics_provider.h"

// A metrics provider that adds metrics to every uploaded histogram report
// to include the time since an update has been available and the state of the
// client.
class UpdateMetricsProvider : public metrics::MetricsProvider {
 public:
  // Enum logged as a histogram - do not renumber entries.
  enum class PendingUpdateState {
    // State could not be determined.
    kUnknown = 0,
    // No update was detected.
    kNoUpdate = 1,
    // An update is available and both the window & tab counts == 1.
    kOneWindowOneTab = 2,
    // An update is available and either the window or tab counts are > 1.
    kMultipleTabsOrWindows = 3,
    // An update is available but Chrome has no windows and is and prevented
    // from restarting by a registered keepalive.
    kBackgrounded = 4,
    // For histogram machinery.
    kMaxValue = kBackgrounded,
  };

  // Enum logged as a histogram - do not renumber entries.
  // Only logged when kOneWindowOneTab is emitted.
  enum class PageBlockingUpdate {
    // Any tab not covered by the categories below.
    kUnspecified = 0,
    // The only tab is a chrome:// scheme page not covered by another value.
    kChromeScheme = 1,
    // The only tab is an NTP.
    kNtp = 2,
    // The only tab is about:blank.
    kAboutBlank = 3,
    // The only tab is chrome://whats-new.
    kWhatsNew = 4,
    // Page could not be determined as there was no Browser object.
    kErrorNoBrowser = 5,
    // Page could not be determined as there were no tabs.
    kErrorNoTabs = 6,
    // For histogram machinery.
    kMaxValue = kErrorNoTabs,
  };

  // metrics::MetricsProvider:
  void ProvideCurrentSessionData(
      metrics::ChromeUserMetricsExtension* uma_proto) override;
};

#endif  // CHROME_BROWSER_UPDATES_UPDATE_METRICS_PROVIDER_H_
