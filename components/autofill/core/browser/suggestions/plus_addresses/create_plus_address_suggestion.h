// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_PLUS_ADDRESSES_CREATE_PLUS_ADDRESS_SUGGESTION_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_PLUS_ADDRESSES_CREATE_PLUS_ADDRESS_SUGGESTION_H_

#include "base/types/strong_alias.h"

namespace autofill {

// A strong alias for a boolean that indicates whether save and fill is
// available.
using CreatePlusAddressSuggestion =
    base::StrongAlias<class CreatePlusAddressSuggestionTag, bool>;

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_PLUS_ADDRESSES_CREATE_PLUS_ADDRESS_SUGGESTION_H_
