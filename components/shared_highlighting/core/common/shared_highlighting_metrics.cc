// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/shared_highlighting/core/common/shared_highlighting_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "components/search_engines/search_engine_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"

namespace shared_highlighting {

namespace {

constexpr char kUmaPrefix[] = "SharedHighlights.LinkGenerated";

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
  DCHECK_NE(reason, LinkGenerationError::kNone);
  base::UmaHistogramEnumeration("SharedHighlights.LinkGenerated.Error", reason);
}

void LogLinkRequestedErrorReason(LinkGenerationError reason) {
  DCHECK_NE(reason, LinkGenerationError::kNone);
  base::UmaHistogramEnumeration(
      "SharedHighlights.LinkGenerated.Error.Requested", reason);
}

void LogLinkGenerationStatus(LinkGenerationStatus status) {
  base::UmaHistogramBoolean("SharedHighlights.LinkGenerated",
                            status == LinkGenerationStatus::kSuccess);
}

void LogLinkRequestedStatus(LinkGenerationStatus status) {
  base::UmaHistogramBoolean("SharedHighlights.LinkGenerated.Requested",
                            status == LinkGenerationStatus::kSuccess);
}

void LogRequestedSuccessMetrics(ukm::SourceId source_id) {
  LogLinkRequestedStatus(LinkGenerationStatus::kSuccess);
  LogLinkGeneratedRequestedSuccessUkmEvent(source_id);
}

void LogRequestedFailureMetrics(ukm::SourceId source_id,
                                LinkGenerationError error) {
  LogLinkRequestedStatus(LinkGenerationStatus::kFailure);
  LogLinkRequestedErrorReason(error);
  LogLinkGeneratedRequestedErrorUkmEvent(source_id, error);
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
    NOTREACHED_IN_MIGRATION();
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

void LogGenerateSuccessLatency(base::TimeDelta latency) {
  base::UmaHistogramTimes("SharedHighlights.LinkGenerated.TimeToGenerate",
                          latency);
}

void LogGenerateErrorLatency(base::TimeDelta latency) {
  base::UmaHistogramTimes("SharedHighlights.LinkGenerated.Error.TimeToGenerate",
                          latency);
}

void LogLinkToTextReshareStatus(LinkToTextReshareStatus status) {
  base::UmaHistogramEnumeration("SharedHighlights.ObtainReshareLink.Status",
                                status);
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

void LogLinkGeneratedRequestedSuccessUkmEvent(ukm::SourceId source_id) {
  ukm::UkmRecorder* recorder = ukm::UkmRecorder::Get();

  DCHECK(recorder);
  if (source_id != ukm::kInvalidSourceId) {
    ukm::builders::SharedHighlights_LinkGenerated_Requested(source_id)
        .SetSuccess(true)
        .Record(recorder);
  }
}

void LogLinkGeneratedRequestedErrorUkmEvent(ukm::SourceId source_id,
                                            LinkGenerationError reason) {
  ukm::UkmRecorder* recorder = ukm::UkmRecorder::Get();

  DCHECK(recorder);
  if (source_id != ukm::kInvalidSourceId) {
    ukm::builders::SharedHighlights_LinkGenerated_Requested(source_id)
        .SetSuccess(false)
        .SetError(static_cast<int64_t>(reason))
        .Record(recorder);
  }
}

void LogLinkRequestedBeforeStatus(LinkGenerationStatus status,
                                  LinkGenerationReadyStatus ready_status) {
  std::string uma_name;
  if (ready_status == LinkGenerationReadyStatus::kRequestedBeforeReady) {
    uma_name = base::StrCat({kUmaPrefix, ".RequestedBeforeReady"});
  } else {
    uma_name = base::StrCat({kUmaPrefix, ".RequestedAfterReady"});
  }
  bool success = status == LinkGenerationStatus::kSuccess;
  base::UmaHistogramBoolean(uma_name, success);
}

}  // namespace shared_highlighting
