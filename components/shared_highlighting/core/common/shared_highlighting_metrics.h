// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SHARED_HIGHLIGHTING_CORE_COMMON_SHARED_HIGHLIGHTING_METRICS_H_
#define COMPONENTS_SHARED_HIGHLIGHTING_CORE_COMMON_SHARED_HIGHLIGHTING_METRICS_H_

#include "base/time/time.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/gurl.h"

namespace shared_highlighting {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// The type of copied Shared Highlighting Link on Desktop.
// Update corresponding |LinkGenerationCopiedLinkType| in enums.xml.
enum class LinkGenerationCopiedLinkType {
  kCopiedFromNewGeneration = 0,
  kCopiedFromExistingHighlight = 1,
  kMaxValue = kCopiedFromExistingHighlight
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// The type of errors that can happen during link generation.
// Update corresponding |LinkGenerationError| in enums.xml.
enum class LinkGenerationError {
  kIncorrectSelector = 0,
  kNoRange = 1,
  kNoContext = 2,
  kContextExhausted = 3,
  kContextLimitReached = 4,
  kEmptySelection = 5,

  // Android specific.
  kTabHidden = 6,
  kOmniboxNavigation = 7,
  kTabCrash = 8,

  // Catch-all bucket.
  kUnknown = 9,

  // Selection happened on iframe.
  kIFrame = 10,

  // Timed-out waiting for a link to be generated.
  kTimeout = 11,

  // Link generation is not triggered because current page is not supported.
  // Recorded on Android/Desktop.
  kBlockList = 12,

  kMaxValue = kBlockList
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// The different sources from which a text fragment URL can come from.
enum class TextFragmentLinkOpenSource {
  kUnknown = 0,
  kSearchEngine = 1,

  kMaxValue = kSearchEngine,
};

// Records the type of link generation that was copied on desktop.
void LogDesktopLinkGenerationCopiedLinkType(LinkGenerationCopiedLinkType type);

// Records the reason why the link generation failed.
void LogLinkGenerationErrorReason(LinkGenerationError reason);

// Records whether the link generation attempt was successful or not.
void LogLinkGenerationStatus(bool link_generated);

// Records whether an individual text fragment could not be scrolled to because
// there was an |ambiguous_match| (generally because more than one matching
// passage was found).
void LogTextFragmentAmbiguousMatch(bool ambiguous_match);

// Records the source of the text fragment based on its |referrer|. E.g. a
// search engine.
void LogTextFragmentLinkOpenSource(const GURL& referrer);

// Records the success rate, which is the number of |matches| over number of
// |text_fragments| in the url param.
void LogTextFragmentMatchRate(int matches, int text_fragments);

// Records the total |count| of text fragment selectors in the URL param.
void LogTextFragmentSelectorCount(int count);

// Records when tab is hidden before generation is complete.
void LogGenerateErrorTabHidden();

// Records when new navigation happens on the tab by user typing in the omnibox.
void LogGenerateErrorOmniboxNavigation();

// Records when tab crashes before generation is complete.
void LogGenerateErrorTabCrash();

// Records when link generation was not completed because selection happened on
// iframe.
void LogGenerateErrorIFrame();

// Records when link generation was not triggered because selection happened on
// a blocklisted page.
void LogGenerateErrorBlockList();

// Records when link generation was not triggered because selection happened on
// a blocklisted page.
void LogGenerateErrorTimeout();

// Records the latency for successfully generating a link.
void LogGenerateSuccessLatency(base::TimeDelta latency);

// Records the latency for failing to generate a link.
void LogGenerateErrorLatency(base::TimeDelta latency);

// Records a UKM event for opening a link with text fragments. |source_id|
// refers to the navigation action's ID, |referrer| will be used to record the
// source and |success| should be true only if fragments highlighting was a
// complete success. This event can only be recorded once per navigation, and
// this function will record using the static Recorder instance. This API can
// only be used when calling from the browser process, otherwise no event will
// be recorded.
void LogLinkOpenedUkmEvent(ukm::SourceId source_id,
                           const GURL& referrer,
                           bool success);

// Records a UKM event for opening a link with text fragments. |source_id|
// refers to the navigation action's ID, |referrer| will be used to record the
// source and |success| should be true only if fragments highlighting was a
// complete success. This event can only be recorded once per navigation, and
// will record using the given custom |recorder|. Prefer this API when calling
// from a process other than the browser process.
void LogLinkOpenedUkmEvent(ukm::UkmRecorder* recorder,
                           ukm::SourceId source_id,
                           const GURL& referrer,
                           bool success);

// Records a UKM event for successfully generating a link with text fragments.
// |source_id| refers to the current frame, and this function will record using
// the static Recorder. This API can only be used when calling from the browser
// process, otherwise no event will be recorded.
void LogLinkGeneratedSuccessUkmEvent(ukm::SourceId source_id);

// Records a UKM event for successfully generating a link with text fragments.
// |source_id| refers to the current frame. This function will record using the
// given custom |recorder|. Prefer this API when calling from a process other
// than the browser process.
void LogLinkGeneratedSuccessUkmEvent(ukm::UkmRecorder* recorder,
                                     ukm::SourceId source_id);

// Records a UKM event for failing to generate a link with text fragments.
// |source_id| refers to the current frame and |reason| highlights the cause of
// the failure. This function will record using the static Recorder. This API
// can only be used when calling from the browser process, otherwise no event
// will be recorded.
void LogLinkGeneratedErrorUkmEvent(ukm::SourceId source_id,
                                   LinkGenerationError reason);

// Records a UKM event for failing to generate a link with text fragments.
// |source_id| refers to the current frame and |reason| highlights the cause of
// the failure. This function will record using the given custom |recorder|.
// Prefer this API when calling from a process other than the browser process.
void LogLinkGeneratedErrorUkmEvent(ukm::UkmRecorder* recorder,
                                   ukm::SourceId source_id,
                                   LinkGenerationError reason);

}  // namespace shared_highlighting

#endif  // COMPONENTS_SHARED_HIGHLIGHTING_CORE_COMMON_SHARED_HIGHLIGHTING_METRICS_H_
