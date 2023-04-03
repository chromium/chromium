// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/mock_iban_manager.h"

namespace autofill {

MockIBANManager::MockIBANManager(PersonalDataManager* personal_data_manager)
    : IBANManager(personal_data_manager) {}

MockIBANManager::~MockIBANManager() = default;

}  // namespace autofill
