// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/mock_personal_data_manager.h"

namespace autofill_assistant {

MockPersonalDataManager::MockPersonalDataManager()
    : PersonalDataManager("en-US") {}
MockPersonalDataManager::~MockPersonalDataManager() = default;

}  // namespace autofill_assistant
