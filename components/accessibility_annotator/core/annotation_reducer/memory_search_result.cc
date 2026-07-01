// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/annotation_reducer/memory_search_result.h"

#include <ostream>

#include "base/strings/utf_ostream_operators.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace accessibility_annotator {

EntryMetadata::EntryMetadata(MemoryDataType type,
                             std::u16string type_name,
                             std::u16string value)
    : type(type), type_name(std::move(type_name)), value(std::move(value)) {}
EntryMetadata::EntryMetadata(const EntryMetadata&) = default;
EntryMetadata& EntryMetadata::operator=(const EntryMetadata&) = default;
EntryMetadata::EntryMetadata(EntryMetadata&&) = default;
EntryMetadata& EntryMetadata::operator=(EntryMetadata&&) = default;
EntryMetadata::~EntryMetadata() = default;

MemoryEntrySource::MemoryEntrySource(MemoryEntrySourceType type,
                                     std::optional<std::string> deeplink_url)
    : type(type), deeplink_url(std::move(deeplink_url)) {}
MemoryEntrySource::MemoryEntrySource(const MemoryEntrySource&) = default;
MemoryEntrySource& MemoryEntrySource::operator=(const MemoryEntrySource&) =
    default;
MemoryEntrySource::MemoryEntrySource(MemoryEntrySource&&) = default;
MemoryEntrySource& MemoryEntrySource::operator=(MemoryEntrySource&&) = default;
MemoryEntrySource::~MemoryEntrySource() = default;

MemorySearchResult::MemorySearchResult(MemoryDataType type,
                                       std::u16string type_name,
                                       std::u16string value,
                                       double confidence_score)
    : type(type),
      type_name(std::move(type_name)),
      value(std::move(value)),
      confidence_score(confidence_score) {}
MemorySearchResult::MemorySearchResult(const MemorySearchResult&) = default;
MemorySearchResult& MemorySearchResult::operator=(const MemorySearchResult&) =
    default;
MemorySearchResult::MemorySearchResult(MemorySearchResult&&) = default;
MemorySearchResult& MemorySearchResult::operator=(MemorySearchResult&&) =
    default;
MemorySearchResult::~MemorySearchResult() = default;

MemorySearchResults::MemorySearchResults(MemorySearchStatus status)
    : status(status) {}
MemorySearchResults::MemorySearchResults(
    MemorySearchStatus status,
    std::vector<MemorySearchResult> entries)
    : status(status), entries(std::move(entries)) {}
MemorySearchResults::MemorySearchResults(const MemorySearchResults&) = default;
MemorySearchResults& MemorySearchResults::operator=(
    const MemorySearchResults&) = default;
MemorySearchResults::MemorySearchResults(MemorySearchResults&&) = default;
MemorySearchResults& MemorySearchResults::operator=(MemorySearchResults&&) =
    default;
MemorySearchResults::~MemorySearchResults() = default;

std::ostream& operator<<(std::ostream& os, const EntryMetadata& metadata) {
  os << static_cast<int>(metadata.type) << " (" << metadata.type_name
     << "): " << metadata.value;
  return os;
}

std::ostream& operator<<(std::ostream& os, const MemoryEntrySource& source) {
  os << static_cast<int>(source.type);
  if (source.deeplink_url) {
    os << " (" << *source.deeplink_url << ")";
  }
  return os;
}

std::ostream& operator<<(std::ostream& os, const MemorySearchResult& result) {
  os << "- type: " << static_cast<int>(result.type) << std::endl;
  os << "- type_name: " << result.type_name << std::endl;
  os << "- value: " << result.value << std::endl;
  os << "- confidence_score: " << result.confidence_score << std::endl;
  os << "- is_obfuscated: " << (result.is_obfuscated ? "true" : "false")
     << std::endl;
  os << "- sources: " << std::endl;
  for (const auto& source : result.sources) {
    os << "  - " << source << std::endl;
  }
  os << "- metadata_list: " << std::endl;
  for (const auto& metadata : result.metadata_list) {
    os << "  - " << metadata << std::endl;
  }
  os << "- identifier: ";
  std::visit(absl::Overload{
                 [&os](std::monostate) { os << "monostate"; },
                 [&os](const auto& arg) { os << arg; },
             },
             result.identifier);
  os << std::endl;
  return os;
}

}  // namespace accessibility_annotator
