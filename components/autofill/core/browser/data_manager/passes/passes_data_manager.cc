// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_manager/passes/passes_data_manager.h"

namespace autofill {

PassesDataManager::PassesDataManager(
    scoped_refptr<AutofillWebDataService> webdata_service)
    : webdata_service_(std::move(webdata_service)) {}

PassesDataManager::~PassesDataManager() = default;

}  // namespace autofill
