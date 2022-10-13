// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_FORM_INTERACTIONS_FLOW_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_FORM_INTERACTIONS_FLOW_H_

#include "base/guid.h"
#include "base/types/strong_alias.h"

namespace autofill {

// GUID linking together form submissions across navigations.
using FormInteractionsFlowId =
    base::StrongAlias<class FormInteractionsFlowIdTag, base::GUID>;

// Counts of user interactions with forms.
struct FormInteractionCounts {
  int64_t form_element_user_modifications = 0;
  int64_t autofill_fills = 0;
};

}  // namespace autofill

#endif
