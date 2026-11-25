// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_ACTOR_ACTOR_CRITICAL_ACTION_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_ACTOR_ACTOR_CRITICAL_ACTION_UTIL_H_

#include <optional>

#include "components/autofill/core/browser/field_types.h"
#include "components/critical_actions/core/browser/critical_action_types.h"

namespace autofill {

// Maps a filled field's group to a contact info or card details attribute.
// Returns `std::nullopt` for groups that are not logged as form fills,
// including IBAN, loyalty card and Enhanced Autofill, which aren't supported
// yet.
std::optional<critical_actions::FormFillType> ToFormFillType(
    FieldTypeGroup group);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_ACTOR_ACTOR_CRITICAL_ACTION_UTIL_H_
