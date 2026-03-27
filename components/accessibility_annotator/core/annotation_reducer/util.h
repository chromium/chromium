// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ANNOTATION_REDUCER_UTIL_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ANNOTATION_REDUCER_UTIL_H_

#include <string_view>

#include "components/accessibility_annotator/core/annotation_reducer/query_intent_type.h"

namespace accessibility_annotator {

// Strips markdown code blocks (e.g., ```json...``` or ```...```) from the
// provided string. Models often wrap their JSON output in these blocks.
std::string_view StripMarkdownCodeBlocks(std::string_view text);

// Maps the intent string returned from Gemini back to the corresponding
// QueryIntentType enum.
QueryIntentType StringToQueryIntentType(std::string_view intent_str);

}  // namespace accessibility_annotator

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ANNOTATION_REDUCER_UTIL_H_
