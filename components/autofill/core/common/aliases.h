// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_ALIASES_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_ALIASES_H_

#include "base/types/strong_alias.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"

namespace autofill {

// TODO(crbug.com/40840597): Use strong aliases for other primitives in mojom
// files.

// Specifies whether a first suggestion gets auto selected.
using AutoselectFirstSuggestion =
    base::StrongAlias<struct AutoselectFirstSuggestionTag, bool>;

using AutofillSuggestionTriggerSource =
    ::autofill::mojom::AutofillSuggestionTriggerSource;

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_ALIASES_H_
