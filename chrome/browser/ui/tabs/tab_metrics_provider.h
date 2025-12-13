// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_METRICS_PROVIDER_H_
#define CHROME_BROWSER_UI_TABS_TAB_METRICS_PROVIDER_H_

#include "base/memory/raw_ptr.h"
#include "components/metrics/metrics_provider.h"

class ProfileManager;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(VerticalTabsState)
enum class VerticalTabsState {
  kAllHorizontal = 0,
  kMixed = 1,
  kAllVertical = 2,
  kMaxValue = kAllVertical,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/tab/enums.xml:VerticalTabsState)

// TabMetricsProvider provides tab-related metrics.
class TabMetricsProvider : public metrics::MetricsProvider {
 public:
  explicit TabMetricsProvider(ProfileManager* profile_manager);

  TabMetricsProvider(const TabMetricsProvider&) = delete;
  TabMetricsProvider& operator=(const TabMetricsProvider&) = delete;

  ~TabMetricsProvider() override;

  // Returns the VerticalTabState for the stored ProfileManager.
  VerticalTabsState GetVerticalTabsState();

  // metrics::MetricsProvider:
  void ProvideCurrentSessionData(
      metrics::ChromeUserMetricsExtension* uma_proto) override;

 private:
  const raw_ptr<ProfileManager> profile_manager_;
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_METRICS_PROVIDER_H_
