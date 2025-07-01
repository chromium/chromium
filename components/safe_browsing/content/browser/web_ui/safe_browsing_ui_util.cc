// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/web_ui/safe_browsing_ui_util.h"

namespace safe_browsing {

#if BUILDFLAG(SAFE_BROWSING_DOWNLOAD_PROTECTION) && !BUILDFLAG(IS_ANDROID)
DeepScanDebugData::DeepScanDebugData() = default;
DeepScanDebugData::DeepScanDebugData(const DeepScanDebugData&) = default;
DeepScanDebugData::~DeepScanDebugData() = default;

TailoredVerdictOverrideData::TailoredVerdictOverrideData() = default;
TailoredVerdictOverrideData::~TailoredVerdictOverrideData() = default;

void TailoredVerdictOverrideData::Set(
    ClientDownloadResponse::TailoredVerdict new_value,
    const SafeBrowsingUIHandler* new_source) {
  override_value = std::move(new_value);
  source = reinterpret_cast<SourceId>(new_source);
}

bool TailoredVerdictOverrideData::IsFromSource(
    const SafeBrowsingUIHandler* maybe_source) const {
  return reinterpret_cast<SourceId>(maybe_source) == source;
}

void TailoredVerdictOverrideData::Clear() {
  override_value.reset();
  source = 0u;
}
#endif

}  // namespace safe_browsing
