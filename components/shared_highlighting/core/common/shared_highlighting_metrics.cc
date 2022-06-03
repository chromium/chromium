// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/shared_highlighting/core/common/shared_highlighting_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "components/search_engines/search_engine_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"

namespace shared_highlighting {

namespace {

TextFragmentLinkOpenSource GetLinkSource(const GURL& referrer) {
  bool from_search_engine =
      referrer.is_valid() && SearchEngineUtils::GetEngineType(referrer) > 0;
  return from_search_engine ? TextFragmentLinkOpenSource::kSearchEngine
                            : TextFragmentLinkOpenSource::kUnknown;
}

}  // namespace

void LogDesktopLinkGenerationCopiedLinkType(LinkGenerationCopiedLinkType type) {
  base::UmaHistogramEnumeration("SharedHighlights.Desktop.CopiedLinkType",
                                type);
}

void LogLinkGenerationErrorReason(LinkGenerationError reason) {
  base::UmaHistogramEnumeration("SharedHighlights.LinkGenerated.Error", reason);
}

void LogLinkGenerationStatus(bool link_generated) {
  base::UmaHistogramBoolean("SharedHighlights.LinkGenerated", link_generated);
}

void LogTextFragmentAmbiguousMatch(bool ambiguous_match) {
  base::UmaHistogramBoolean("TextFragmentAnchor.AmbiguousMatch",
                            ambiguous_match);
}

void LogTextFragmentLinkOpenSource(const GURL& referrer) {
  base::UmaHistogramEnumeration("TextFragmentAnchor.LinkOpenSource",
                                GetLinkSource(referrer));
}

void LogTextFragmentMatchRate(int matches, int text_fragments) {
  if (text_fragments == 0) {
    NOTREACHED();
    return;
  }

  const int match_rate_percent =
      static_cast<int>(100 * ((matches + 0.0) / text_fragments));
  base::UmaHistogramPercentage("TextFragmentAnchor.MatchRate",
                               match_rate_percent);
}

void LogTextFragmentSelectorCount(int count) {
  base::UmaHistogramCounts100("TextFragmentAnchor.SelectorCount", count);
}

// TODO(gayane): Replace by one function LogGenerateError(Error).
void LogGenerateErrorTabHidden() {
  LogLinkGenerationErrorReason(LinkGenerationError::kTabHidden);
}

void LogGenerateErrorOmniboxNavigation() {
  LogLinkGenerationErrorReason(LinkGenerationError::kOmniboxNavigation);
}

void LogGenerateErrorTabCrash() {
  LogLinkGenerationErrorReason(LinkGenerationError::kTabCrash);
}

void LogGenerateErrorIFrame() {
  LogLinkGenerationErrorReason(LinkGenerationError::kIFrame);
}

void LogGenerateErrorBlockList() {
  LogLinkGenerationErrorReason(LinkGenerationError::kBlockList);
}

void LogGenerateErrorTimeout() {
  LogLinkGenerationErrorReason(LinkGenerationError::kTimeout);
}

void LogGenerateSuccessLatency(base::TimeDelta latency) {
  base::UmaHistogramTimes("SharedHighlights.LinkGenerated.TimeToGenerate",
                          latency);
}

void LogGenerateErrorLatency(base::TimeDelta latency) {
  base::UmaHistogramTimes("SharedHighlights.LinkGenerated.Error.TimeToGenerate",
                          latency);
}

void LogLinkOpenedUkmEvent(ukm::SourceId source_id,
                           const GURL& referrer,
                           bool success) {
  LogLinkOpenedUkmEvent(ukm::UkmRecorder::Get(), source_id, referrer, success);
}

void LogLinkOpenedUkmEvent(ukm::UkmRecorder* recorder,
                           ukm::SourceId source_id,
                           const GURL& referrer,
                           bool success) {
  DCHECK(recorder);
  if (source_id != ukm::kInvalidSourceId) {
    ukm::builders::SharedHighlights_LinkOpened(source_id)
        .SetSuccess(success)
        .SetSource(static_cast<int64_t>(GetLinkSource(referrer)))
        .Record(recorder);
  }
}

void LogLinkGeneratedSuccessUkmEvent(ukm::SourceId source_id) {
  LogLinkGeneratedSuccessUkmEvent(ukm::UkmRecorder::Get(), source_id);
}

void LogLinkGeneratedSuccessUkmEvent(ukm::UkmRecorder* recorder,
                                     ukm::SourceId source_id) {
  DCHECK(recorder);
  if (source_id != ukm::kInvalidSourceId) {
    ukm::builders::SharedHighlights_LinkGenerated(source_id)
        .SetSuccess(true)
        .Record(recorder);
  }
}

void LogLinkGeneratedErrorUkmEvent(ukm::SourceId source_id,
                                   LinkGenerationError reason) {
  LogLinkGeneratedErrorUkmEvent(ukm::UkmRecorder::Get(), source_id, reason);
}

void LogLinkGeneratedErrorUkmEvent(ukm::UkmRecorder* recorder,
                                   ukm::SourceId source_id,
                                   LinkGenerationError reason) {
  DCHECK(recorder);
  if (source_id != ukm::kInvalidSourceId) {
    ukm::builders::SharedHighlights_LinkGenerated(source_id)
        .SetSuccess(false)
        .SetError(static_cast<int64_t>(reason))
        .Record(recorder);
  }
}

}  // namespace shared_highlighting
