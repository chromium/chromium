// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_metrics_mapping.h"

#include <array>
#include <string_view>

#include "components/metrics/mapping/metrics_name_mapper.h"

namespace {

using metrics::MetricsNameMapper;

constexpr auto kTopChromeRules = std::to_array<MetricsNameMapper::Rule>({
    // Guardrail metric recorded by cc (cc/metrics/frame_sequence_metrics.cc);
    // split top-chrome WebUI samples from web content. crbug.com/577044464
    {"Graphics.Smoothness.PercentDroppedFrames3.AllSequences",
     "WebUI.TopChrome.Graphics.Smoothness.PercentDroppedFrames3.AllSequences"},
});

// Keep WebUI.* histograms so C++ code can record them without a rule.
constexpr auto kTopChromeAllowedPrefixes =
    std::to_array<std::string_view>({"WebUI."});

}  // namespace

void RegisterTopChromeWebUIMetricsMappingDefaults() {
  MetricsNameMapper::SetDefaults(kTopChromeRules, kTopChromeAllowedPrefixes);
}
