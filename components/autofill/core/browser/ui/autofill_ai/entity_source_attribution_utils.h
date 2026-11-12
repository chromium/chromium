// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_AUTOFILL_AI_ENTITY_SOURCE_ATTRIBUTION_UTILS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_AUTOFILL_AI_ENTITY_SOURCE_ATTRIBUTION_UTILS_H_

#include <string>

#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"

namespace autofill {

// Returns the formatted display title for `source` (e.g., email subject or
// localized date for Gmail, or localized date for Photos), or an empty string
// if neither is available.
std::u16string GetSourceDisplayTitle(
    const EntityInstance::PersonalContextRecordTypePayload::Source& source);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_AUTOFILL_AI_ENTITY_SOURCE_ATTRIBUTION_UTILS_H_
