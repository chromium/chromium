// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/iwa_source_url_recorder.h"

#include "base/logging.h"
#include "services/metrics/public/cpp/delegating_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/gurl.h"

namespace ukm {

// static
SourceId IwaSourceUrlRecorder::GetSourceIdForIwaUrl(const GURL& iwa_url) {
  return UkmRecorder::GetSourceIdForIwaUrl(
      base::PassKey<IwaSourceUrlRecorder>(), iwa_url);
}

// static
void IwaSourceUrlRecorder::MarkSourceForDeletion(SourceId source_id) {
  if (GetSourceIdType(source_id) != SourceIdType::IWA_BUNDLE_ID) {
    DLOG(FATAL) << "IwaSourceUrlRecorder::MarkSourceForDeletion invoked on "
                << "non-IWA_BUNDLE_ID type SourceId: " << source_id;
    return;
  }

  ukm::DelegatingUkmRecorder* const recorder =
      ukm::DelegatingUkmRecorder::Get();
  if (recorder) {
    recorder->MarkSourceForDeletion(source_id);
  }
}

}  // namespace ukm
