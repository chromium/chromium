// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_FORM_INTERACTIONS_FLOW_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_FORM_INTERACTIONS_FLOW_H_

#include "base/rand_util.h"
#include "base/types/id_type.h"

namespace autofill {

// A flow ID links together form submissions across navigations.
//
// It's implemented as a random 64 bit number so it identifies flows across
// installations, in particular for UKM metrics.
struct FormInteractionsFlowId
    : public base::IdTypeU64<struct FormInteractionsFlowIdTag> {
 public:
  FormInteractionsFlowId()
      : base::IdTypeU64<struct FormInteractionsFlowIdTag>::IdType(
            base::RandUint64()) {}
};

// Counts of user interactions with forms.
struct FormInteractionCounts {
  int64_t form_element_user_modifications = 0;
  int64_t autofill_fills = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_FORM_INTERACTIONS_FLOW_H_
