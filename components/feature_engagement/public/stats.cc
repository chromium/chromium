// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/public/stats.h"

#include "base/metrics/histogram_macros.h"

namespace feature_engagement::stats {

void RecordConfigParsingEvent(ConfigParsingEvent event) {
  UMA_HISTOGRAM_ENUMERATION("InProductHelp.Config.ParsingEvent", event,
                            ConfigParsingEvent::COUNT);
}

}  // namespace feature_engagement::stats
