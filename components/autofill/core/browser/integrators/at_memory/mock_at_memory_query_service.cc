// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/at_memory/mock_at_memory_query_service.h"

#include <memory>

#include "components/autofill/core/browser/at_memory/autofill_data_provider.h"
#include "components/autofill/core/browser/integrators/at_memory/at_memory_query_service_delegate.h"

namespace autofill {

namespace {

class StubAtMemoryQueryServiceDelegate : public AtMemoryQueryServiceDelegate {
 public:
  void RetrieveLiveTabContext(
      LiveTabContextQuery query,
      base::OnceCallback<void(LiveTabContextResponse)> callback) override {
    std::move(callback).Run({});
  }
};

}  // namespace

MockAtMemoryQueryService::MockAtMemoryQueryService()
    : AtMemoryQueryService(std::make_unique<StubAtMemoryQueryServiceDelegate>(),
                           /*data_provider=*/nullptr,
                           /*personal_context_service=*/nullptr,
                           /*locale=*/"") {}

MockAtMemoryQueryService::~MockAtMemoryQueryService() = default;

}  // namespace autofill
