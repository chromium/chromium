// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/test/mock_iban_manager.h"

namespace autofill {

MockIbanManager::MockIbanManager(PersonalDataManager* personal_data_manager)
    : IbanManager(personal_data_manager) {}

MockIbanManager::~MockIbanManager() = default;

}  // namespace autofill
