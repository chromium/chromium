// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AT_MEMORY_LOGGING_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AT_MEMORY_LOGGING_UTIL_H_

#include "components/personal_context/proto/features/at_memory.pb.h"

namespace autofill {

class LogBuffer;

// Serializes `plan` into `buffer`.
LogBuffer& operator<<(LogBuffer& buffer,
                      const personal_context::proto::AutofillFetchPlan& plan);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AT_MEMORY_LOGGING_UTIL_H_
