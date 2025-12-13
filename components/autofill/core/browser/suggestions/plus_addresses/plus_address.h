// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_PLUS_ADDRESSES_PLUS_ADDRESS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_PLUS_ADDRESSES_PLUS_ADDRESS_H_

#include <string>
#include <vector>

#include "base/types/strong_alias.h"

namespace autofill {

using PlusAddress = base::StrongAlias<class PlusAddressTag, std::string>;

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_PLUS_ADDRESSES_PLUS_ADDRESS_H_
