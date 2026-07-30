// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AT_MEMORY_MEMORY_SEARCH_RESULT_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AT_MEMORY_MEMORY_SEARCH_RESULT_H_

#include <iosfwd>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "base/functional/callback.h"
#include "components/autofill/core/browser/integrators/at_memory/memory_data_type.h"
#include "components/personal_context/proto/features/at_memory.equal.h"
#include "components/personal_context/proto/features/at_memory.pb.h"

namespace autofill {

// Key-value metadata providing additional context for an entry.
struct EntryMetadata {
  EntryMetadata(MemoryDataType type,
                std::u16string type_name,
                std::u16string value,
                std::optional<personal_context::proto::TypedValue> typed_value =
                    std::nullopt);
  EntryMetadata(const EntryMetadata&);
  EntryMetadata& operator=(const EntryMetadata&);
  EntryMetadata(EntryMetadata&&);
  EntryMetadata& operator=(EntryMetadata&&);
  ~EntryMetadata();
  friend bool operator==(const EntryMetadata&, const EntryMetadata&) = default;

  // Type of metadata (a key). One of the known types or kUnknown.
  MemoryDataType type;
  // Localized name of the type (eg: "Departure Airport").
  // For unknown types, it should be filled with free-form text.
  std::u16string type_name;
  // Value of the metadata (eg: New York).
  std::u16string value;
  // Typed value of the metadata, if available, it should semantically match
  // `value`.
  std::optional<personal_context::proto::TypedValue> typed_value;
};

std::ostream& operator<<(std::ostream& os, const EntryMetadata& metadata);

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
  friend bool operator==(const MemoryEntrySource&,
                         const MemoryEntrySource&) = default;

  MemoryEntrySourceType type;
  std::optional<std::string> deeplink_url;
};

std::ostream& operator<<(std::ostream& os, const MemoryEntrySource& source);

// An individual entry in the returned suggested search results list.
struct MemorySearchResult {
  MemorySearchResult(MemoryDataType type,
                     std::u16string type_name,
                     std::u16string value,
                     double confidence_score = 0.0,
                     std::optional<personal_context::proto::TypedValue>
                         typed_value = std::nullopt);
  MemorySearchResult(const MemorySearchResult&);
  MemorySearchResult& operator=(const MemorySearchResult&);
  MemorySearchResult(MemorySearchResult&&);
  MemorySearchResult& operator=(MemorySearchResult&&);
  ~MemorySearchResult();
  friend bool operator==(const MemorySearchResult&,
                         const MemorySearchResult&) = default;

  // Type of value to be filled. One of the known types or kUnknown.
  MemoryDataType type;

  // Localized name of the entry type to be displayed on UI (eg: "Flight
  // Number"). For unknown types, it should be filled with free-form text.
  std::u16string type_name;

  // Candidate value to be filled in (eg: CX123).
  std::u16string value;

  // Typed value of the entry, if available, it should semantically match
  // `value`.
  std::optional<personal_context::proto::TypedValue> typed_value;

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
  // `MemoryDataType` does not support identifiers, it will be unset
  // (monostate).
  std::variant<std::monostate, std::string, int64_t> identifier;

  // The index of the entry in the remote response. If the entry is not a remote
  // result, it will be unset (nullopt).
  std::optional<int32_t> remote_response_index;

  // Indicates if the entry comes from a locally stored Autofill data (such as
  // a local address profile, a local credit card, or a local Autofill AI
  // entity), as opposed to server-side data (like a Wallet card or a remote
  // query response). Useful to discern local vs remote entities during
  // deduplication.
  // TODO(crbug.com/532649036): Use the real type instead.
  bool is_local = false;
};

std::ostream& operator<<(std::ostream& os, const MemorySearchResult& result);

enum class MemorySearchStatus {
  // Final response with all data-sources.
  kFinalResponseSuccess,
  // Partial response with some data-sources. Still loading more.
  kPartialResponseSuccess,
  // Query is unsupported.
  kUnsupportedQuery,
  // Call to a model inference failed.
  kInferenceFailure,
  // Failure due to lack of internet connection.
  kNoConnectionFailure,
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

  // The server request ID, used to identify the request in the logs.
  std::string server_request_id;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AT_MEMORY_MEMORY_SEARCH_RESULT_H_
