// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SHARED_HIGHLIGHTING_CORE_COMMON_SHARED_HIGHLIGHTING_METRICS_H_
#define COMPONENTS_SHARED_HIGHLIGHTING_CORE_COMMON_SHARED_HIGHLIGHTING_METRICS_H_

#include "url/gurl.h"

namespace shared_highlighting {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// The type of errors that can happen during link generation.
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

  kMaxValue = kIFrame
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// The different sources from which a text fragment URL can come from.
enum class TextFragmentLinkOpenSource {
  kUnknown = 0,
  kSearchEngine = 1,

  kMaxValue = kSearchEngine,
};

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

}  // namespace shared_highlighting

#endif  // COMPONENTS_SHARED_HIGHLIGHTING_CORE_COMMON_SHARED_HIGHLIGHTING_METRICS_H_
