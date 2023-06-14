// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/test/mock_mandatory_reauth_manager.h"

namespace autofill::payments {

MockMandatoryReauthManager::MockMandatoryReauthManager()
    : MandatoryReauthManager(nullptr) {}

MockMandatoryReauthManager::~MockMandatoryReauthManager() = default;

}  // namespace autofill::payments
