// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AT_MEMORY_MEMORY_DATA_TYPE_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AT_MEMORY_MEMORY_DATA_TYPE_UTIL_H_

#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/integrators/at_memory/memory_data_type.h"
#include "components/autofill/core/browser/integrators/at_memory/memory_search_result.h"

namespace personal_context::proto {
class AtMemoryQueryResponse;
class AtMemorySearchResult;
class Entity;
enum MemoryDataType : int;
}  // namespace personal_context::proto

namespace autofill {

// Returns true if the given `type` is considered sensitive personal
// information.
bool IsSpiiMemoryDataType(MemoryDataType type);

// Converts a set of memory entry values into `personal_context::proto::Entity`.
// `value` is the primary value of the memory entry corresponding to the
// `memory_data_type` (for example, the actual passport number if the type is
// `kPassportNumber`). `metadata_list` contains the associated attributes (e.g.,
// expiration date, issuing country) for the memory entry.
personal_context::proto::Entity ToPersonalContextEntity(
    std::u16string_view value,
    MemoryDataType memory_data_type,
    base::span<const EntryMetadata> metadata_list);

// Translates Autofill attribute names to entry types.
MemoryDataType AttributeTypeToMemoryDataType(AttributeType type);

// Returns the localized name of the entry type.
std::u16string GetMemoryDataTypeNameForI18n(MemoryDataType type);

// Converts an `AtMemoryQueryResponse` proto into a list of
// `MemorySearchResult.`
std::vector<MemorySearchResult> ExtractRemoteResults(
    const personal_context::proto::AtMemoryQueryResponse& response,
    std::string_view app_locale);

// The following functions are exposed in the header for testing purposes only:

// Converts a `proto::MemoryDataType` to a local `MemoryDataType`.
MemoryDataType ToMemoryDataType(
    personal_context::proto::MemoryDataType data_type);

// Extracts data sources (e.g. Gmail, Photos) from an `AtMemorySearchResult`
// proto.
std::vector<MemoryEntrySource> ExtractSources(
    const personal_context::proto::AtMemorySearchResult& proto_result);

// Extracts secondary metadata attributes from an `AtMemorySearchResult` proto.
std::vector<EntryMetadata> ExtractMetadata(
    const personal_context::proto::AtMemorySearchResult& proto_result,
    std::string_view app_locale);

// Converts a single `AtMemorySearchResult` proto into a `MemorySearchResult`
// struct.
MemorySearchResult ConvertToMemorySearchResult(
    const personal_context::proto::AtMemorySearchResult& proto_result,
    std::string_view app_locale);

// Returns the primary attribute type for a given entity type.
AttributeType GetPrimaryAttributeType(EntityType entity_type);
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AT_MEMORY_MEMORY_DATA_TYPE_UTIL_H_
