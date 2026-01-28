// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WALLET_CORE_BROWSER_DATA_MODELS_DATA_MODEL_UTILS_H_
#define COMPONENTS_WALLET_CORE_BROWSER_DATA_MODELS_DATA_MODEL_UTILS_H_

#include <string>

#include "components/wallet/core/browser/data_models/wallet_pass.h"

namespace wallet {

// Returns the string name of the pass category.
std::string PassCategoryToString(PassCategory category);

}  // namespace wallet

#endif  // COMPONENTS_WALLET_CORE_BROWSER_DATA_MODELS_DATA_MODEL_UTILS_H_
