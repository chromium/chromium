// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ANNOTATION_REDUCER_MEMORY_SEARCH_RESULT_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ANNOTATION_REDUCER_MEMORY_SEARCH_RESULT_H_

#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "base/functional/callback.h"
#include "components/accessibility_annotator/core/annotation_reducer/entry_type.h"

namespace accessibility_annotator {

// Key-value metadata providing additional context for an entry.
struct EntryMetadata {
  EntryMetadata(EntryType type, std::u16string type_name, std::u16string value);
  EntryMetadata(const EntryMetadata&);
  EntryMetadata& operator=(const EntryMetadata&);
  EntryMetadata(EntryMetadata&&);
  EntryMetadata& operator=(EntryMetadata&&);
  ~EntryMetadata();
  bool operator==(const EntryMetadata& other) const = default;

  // Type of metadata (a key). One of the known types or kUnknown.
  EntryType type;
  // Localized name of the type (eg: "Departure Airport").
  // For unknown types, it should be filled with free-form text.
  std::u16string type_name;
  // Value of the metadata (eg: New York).
  std::u16string value;
};

// Type of the data source.
// LINT.IfChange(MemoryEntrySourceType)
enum class MemoryEntrySourceType {
  kAutofill,
  kGmail,
  kCalendar,
  kPhotos,
  kAmbient,
  kLiveTabs,
  kMaxValue = kLiveTabs,
};
// LINT.ThenChange(//components/accessibility_annotator/core/annotation_reducer/util.cc:SourceTypeToMemoryEntrySourceType)

// Source of the search result entry, including the data source type and an
// optional direct attribution.
struct MemoryEntrySource {
  explicit MemoryEntrySource(
      MemoryEntrySourceType type,
      std::optional<std::string> deeplink_url = std::nullopt);
  MemoryEntrySource(const MemoryEntrySource&);
  MemoryEntrySource& operator=(const MemoryEntrySource&);
  MemoryEntrySource(MemoryEntrySource&&);
  MemoryEntrySource& operator=(MemoryEntrySource&&);
  ~MemoryEntrySource();
  bool operator==(const MemoryEntrySource& other) const = default;

  MemoryEntrySourceType type;
  std::optional<std::string> deeplink_url;
};

// An individual entry in the returned suggested search results list.
struct MemorySearchResult {
  MemorySearchResult(EntryType type,
                     std::u16string type_name,
                     std::u16string value,
                     double confidence_score = 0.0);
  MemorySearchResult(const MemorySearchResult&);
  MemorySearchResult& operator=(const MemorySearchResult&);
  MemorySearchResult(MemorySearchResult&&);
  MemorySearchResult& operator=(MemorySearchResult&&);
  ~MemorySearchResult();

  // Type of value to be filled. One of the known types or kUnknown.
  EntryType type;

  // Localized name of the entry type to be displayed on UI (eg: "Flight
  // Number"). For unknown types, it should be filled with free-form text.
  std::u16string type_name;

  // Candidate value to be filled in (eg: CX123).
  std::u16string value;

  // Sources of the search result entry.
  std::vector<MemoryEntrySource> sources;

  // Full list of metadata associated with this entry.
  // The list is ordered to have the most relevant metadata first such
  // that can be used to uniquely identify the current entry.
  std::vector<EntryMetadata> metadata_list;

  // Relevance affecting ordering, the higher the better.
  double confidence_score = 0.0;

  // Whether the value is obfuscated.
  bool is_obfuscated = false;

  // The identifier of the entry (e.g. IBAN Guid or InstrumentId). If
  // `EntryType` does not support identifiers, it will be unset (monostate).
  std::variant<std::monostate, std::string, int64_t> identifier;
};

enum class MemorySearchStatus {
  // Final response with all data-sources.
  kFinalResponseSuccess,
  // Partial response with some data-sources. Still loading more.
  kPartialResponseSuccess,
  // Query is unsupported.
  kUnsupportedQuery,
  // Call to a model inference failed.
  kInferenceFailure,
  // Failure obtaining from 1P data sources.
  kDataFetchFailure,
  // Other internal Failures.
  kInternalFailure
};

// A collection of search results and the status of the query execution.
struct MemorySearchResults {
  explicit MemorySearchResults(MemorySearchStatus status);
  MemorySearchResults(MemorySearchStatus status,
                      std::vector<MemorySearchResult> entries);
  MemorySearchResults(const MemorySearchResults&);
  MemorySearchResults& operator=(const MemorySearchResults&);
  MemorySearchResults(MemorySearchResults&&);
  MemorySearchResults& operator=(MemorySearchResults&&);
  ~MemorySearchResults();

  // The status of the search.
  MemorySearchStatus status;

  // List of suggested entries.
  std::vector<MemorySearchResult> entries;
};

}  // namespace accessibility_annotator

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ANNOTATION_REDUCER_MEMORY_SEARCH_RESULT_H_
