// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_VALUABLES_VALUABLE_TYPES_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_VALUABLES_VALUABLE_TYPES_H_

#include <string>

#include "base/types/strong_alias.h"

namespace autofill {

// Id for valuables coming from Google Wallet.
using ValuableId = base::StrongAlias<class ValuableIdTag, std::string>;

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_VALUABLES_VALUABLE_TYPES_H_
