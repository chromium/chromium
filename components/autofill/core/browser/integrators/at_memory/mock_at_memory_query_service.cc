// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/at_memory/mock_at_memory_query_service.h"

#include <memory>

#include "components/autofill/core/browser/at_memory/autofill_data_provider.h"

namespace autofill {

MockAtMemoryQueryService::MockAtMemoryQueryService()
    : AtMemoryQueryService(/*data_provider=*/nullptr,
                           /*personal_context_service=*/nullptr,
                           /*locale=*/"") {}

MockAtMemoryQueryService::~MockAtMemoryQueryService() = default;

}  // namespace autofill
