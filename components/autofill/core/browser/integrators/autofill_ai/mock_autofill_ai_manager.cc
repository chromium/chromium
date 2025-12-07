// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/autofill_ai/mock_autofill_ai_manager.h"

#include "components/autofill/core/browser/integrators/autofill_ai/autofill_ai_manager.h"

namespace autofill {

MockAutofillAiManager::MockAutofillAiManager(
    autofill::AutofillClient* client,
    strike_database::StrikeDatabaseBase* strike_database)
    : AutofillAiManager(client, strike_database) {}

MockAutofillAiManager::~MockAutofillAiManager() = default;

}  // namespace autofill
