// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AT_MEMORY_MEMORY_DATA_TYPE_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AT_MEMORY_MEMORY_DATA_TYPE_UTIL_H_

#include <string_view>

#include "base/containers/span.h"
#include "components/accessibility_annotator/core/annotation_reducer/memory_data_type.h"
#include "components/accessibility_annotator/core/annotation_reducer/memory_search_result.h"
#include "components/personal_context/proto/features/common_data.pb.h"

namespace autofill {

// Returns true if the given `type` is considered sensitive personal
// information.
bool IsSpiiMemoryDataType(accessibility_annotator::MemoryDataType type);

// Converts a set of memory entry values into `personal_context::proto::Entity`.
// `value` is the primary value of the memory entry corresponding to the
// `memory_data_type` (for example, the actual passport number if the type is
// `kPassportNumber`). `metadata_list` contains the associated attributes (e.g.,
// expiration date, issuing country) for the memory entry.
personal_context::proto::Entity ToPersonalContextEntity(
    std::u16string_view value,
    accessibility_annotator::MemoryDataType memory_data_type,
    base::span<const accessibility_annotator::EntryMetadata> metadata_list);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AT_MEMORY_MEMORY_DATA_TYPE_UTIL_H_
