// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SHARED_HIGHLIGHTING_CORE_COMMON_SHARED_HIGHLIGHTING_METRICS_H_
#define COMPONENTS_SHARED_HIGHLIGHTING_CORE_COMMON_SHARED_HIGHLIGHTING_METRICS_H_

#include "url/gurl.h"

namespace shared_highlighting {

// Update corresponding |LinkGenerationError| in enums.xml.
enum class LinkGenerationError {
  kIncorrectSelector,
  kNoRange,
  kNoContext,
  kContextExhausted,
  kContextLimitReached,
  kEmptySelection,

  kTabHidden,
  kOmniboxNavigation,
  kTabCrash,

  kMaxValue = kTabCrash
};

// Update corresponding |TextFragmentLinkOpenSource| in enums.xml.
enum class TextFragmentLinkOpenSource {
  kUnknown,
  kSearchEngine,

  kMaxValue = kSearchEngine,
};

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

}  // namespace shared_highlighting

#endif  // COMPONENTS_SHARED_HIGHLIGHTING_CORE_COMMON_SHARED_HIGHLIGHTING_METRICS_H_