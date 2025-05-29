// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/form_interactions_flow.h"

#include "base/rand_util.h"

namespace autofill {

FormInteractionsFlowId::FormInteractionsFlowId()
    : base::IdTypeU64<struct FormInteractionsFlowIdTag>::IdType(
          base::RandUint64()) {}

}  // namespace autofill
