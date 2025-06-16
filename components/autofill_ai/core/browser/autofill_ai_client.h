// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_AI_CORE_BROWSER_AUTOFILL_AI_CLIENT_H_
#define COMPONENTS_AUTOFILL_AI_CORE_BROWSER_AUTOFILL_AI_CLIENT_H_

#include <optional>

#include "base/functional/callback_forward.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/integrators/autofill_ai/autofill_ai_delegate.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/optimization_guide/core/model_quality/model_quality_logs_uploader_service.h"

namespace autofill_ai {

class AutofillAiModelExecutor;
class AutofillAiManager;

// An interface for embedder actions, e.g. Chrome on Desktop.
//
// A client should be created only if
// `AutofillAiIsPlatformAndEnterprisePolicyEligible()`. However,
// `AutofillAiIsPlatformAndEnterprisePolicyEligible()` is not necessarily a
// constant over the lifetime of the client. For example, the user may disable
// Autofill in the settings while the client is alive.
class AutofillAiClient {
 public:
  virtual ~AutofillAiClient() = default;

  // Returns the AutofillClient that is scoped to the same object (e.g., tab) as
  // this AutofillAiClient.
  virtual autofill::AutofillClient& GetAutofillClient() = 0;
  const autofill::AutofillClient& GetAutofillClient() const {
    return const_cast<const autofill::AutofillClient&>(
        const_cast<AutofillAiClient*>(this)->GetAutofillClient());
  }

  // Returns the `AutofillAiManager` associated with this
  // client.
  virtual AutofillAiManager& GetManager() = 0;
};

}  // namespace autofill_ai

#endif  // COMPONENTS_AUTOFILL_AI_CORE_BROWSER_AUTOFILL_AI_CLIENT_H_
