// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TOP_CHROME_TOP_CHROME_WEBUI_METRICS_MAPPING_H_
#define CHROME_BROWSER_UI_WEBUI_TOP_CHROME_TOP_CHROME_WEBUI_METRICS_MAPPING_H_

// Registers Chrome's built-in `WebiumMetricsMapping` rules, which apply to
// histograms from top-chrome WebUI renderers without a served config. They
// cover the histograms that reach `metrics::MetricsNameMapper`, such as those
// that C++ code records. See crrev.com/c/8793768 for
// `base::HistogramBase::kExemptFromNameMappingFlag`, which lets
// `chrome.histograms` histograms skip the mapper altogether.
//
// `ChromeBrowserMainParts::SetupMetrics()` calls this once, before it creates
// `SubprocessMetricsProvider`.
void RegisterTopChromeWebUIMetricsMappingDefaults();

#endif  // CHROME_BROWSER_UI_WEBUI_TOP_CHROME_TOP_CHROME_WEBUI_METRICS_MAPPING_H_
