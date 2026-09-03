// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AT_MEMORY_LOGGING_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AT_MEMORY_LOGGING_UTIL_H_

#include <vector>

#include "components/personal_context/proto/features/at_memory.pb.h"

namespace autofill {

class LogBuffer;
class LogManager;
struct MemorySearchResult;

// Serializes `plan` into `buffer`.
LogBuffer& operator<<(LogBuffer& buffer,
                      const personal_context::proto::AutofillFetchPlan& plan);

// Serializes `result` into `buffer`.
LogBuffer& operator<<(LogBuffer& buffer, const MemorySearchResult& result);

// Serializes `results` into `buffer`.
LogBuffer& operator<<(LogBuffer& buffer,
                      const std::vector<MemorySearchResult>& results);

// Logs a `discarded` duplicate search result and its corresponding `retained`
// result to `log_manager`.
void LogDiscardedDuplicate(LogManager& log_manager,
                           const MemorySearchResult& discarded,
                           const MemorySearchResult& retained);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AT_MEMORY_LOGGING_UTIL_H_
