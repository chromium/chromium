// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ANNOTATION_REDUCER_UTIL_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ANNOTATION_REDUCER_UTIL_H_

#include <optional>
#include <string_view>

#include "components/accessibility_annotator/core/annotation_reducer/entry_type.h"
#include "components/accessibility_annotator/core/annotation_reducer/memory_search_result.h"
#include "components/optimization_guide/proto/features/annotation_reducer_one_p_resolver.pb.h"

namespace accessibility_annotator {

// Strips markdown code blocks (e.g., ```json...``` or ```...```) from the
// provided string. Models often wrap their JSON output in these blocks.
std::string_view StripMarkdownCodeBlocks(std::string_view text);

// Maps the intent string returned from Gemini back to the corresponding
// EntryType enum.
EntryType StringToEntryType(std::string_view intent_str);

// Maps the intent type returned from the 1P service back to the corresponding
// EntryType enum.
std::optional<EntryType> AnswerTypeToEntryType(
    optimization_guide::proto::ReducedAnswer::AnswerType answer_type);

// Maps the source type returned from the 1P service back to the corresponding
// MemoryEntrySourceType enum.
std::optional<MemoryEntrySourceType> SourceTypeToMemoryEntrySourceType(
    optimization_guide::proto::ReducedAnswer::Source::SourceType type);

}  // namespace accessibility_annotator

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ANNOTATION_REDUCER_UTIL_H_
