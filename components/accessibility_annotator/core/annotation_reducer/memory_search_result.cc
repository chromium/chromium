// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/annotation_reducer/memory_search_result.h"

namespace accessibility_annotator {

EntryMetadata::EntryMetadata(EntryType type,
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

MemorySearchResult::MemorySearchResult(EntryType type,
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

}  // namespace accessibility_annotator
