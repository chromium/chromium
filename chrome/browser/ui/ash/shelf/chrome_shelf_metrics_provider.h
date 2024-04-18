// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SHELF_CHROME_SHELF_METRICS_PROVIDER_H_
#define CHROME_BROWSER_UI_ASH_SHELF_CHROME_SHELF_METRICS_PROVIDER_H_

#include "components/metrics/metrics_provider.h"

// A provider which records shelf related metrics such as which default apps are
// pinned to the shelf.
class ChromeShelfMetricsProvider : public metrics::MetricsProvider {
 public:
  ChromeShelfMetricsProvider();
  ChromeShelfMetricsProvider(const ChromeShelfMetricsProvider&) = delete;
  ChromeShelfMetricsProvider& operator=(const ChromeShelfMetricsProvider&) =
      delete;
  ~ChromeShelfMetricsProvider() override;

 private:
  // metrics::MetricsProvider:
  void ProvideCurrentSessionData(metrics::ChromeUserMetricsExtension*) override;
};

#endif  // CHROME_BROWSER_UI_ASH_SHELF_CHROME_SHELF_METRICS_PROVIDER_H_
