// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_COMPOSE_COMPOSE_AVAILABILITY_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_COMPOSE_COMPOSE_AVAILABILITY_H_

#include "base/types/strong_alias.h"

namespace autofill {

// A strong alias for a boolean that indicates whether compose suggestion should
// be shown.
using ComposeAvailability =
    base::StrongAlias<class ComnposeAvailabilityTag, bool>;

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_COMPOSE_COMPOSE_AVAILABILITY_H_
