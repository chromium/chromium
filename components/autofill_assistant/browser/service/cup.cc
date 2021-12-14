// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cup.h"

#include "base/feature_list.h"
#include "components/autofill_assistant/browser/features.h"

namespace {

bool ShouldSignGetActionsRequests() {
  return base::FeatureList::IsEnabled(
      autofill_assistant::features::kAutofillAssistantSignGetActionsRequests);
}

bool ShouldVerifyGetActionsResponses() {
  return ShouldSignGetActionsRequests() &&
         base::FeatureList::IsEnabled(
             autofill_assistant::features::
                 kAutofillAssistantVerifyGetActionsResponses);
}

}  // namespace

namespace autofill_assistant {

namespace cup {

bool ShouldSignRequests(RpcType rpc_type) {
  return ShouldSignGetActionsRequests() && rpc_type == RpcType::GET_ACTIONS;
}

bool ShouldVerifyResponses(RpcType rpc_type) {
  return ShouldVerifyGetActionsResponses() && rpc_type == RpcType::GET_ACTIONS;
}

}  // namespace cup

}  // namespace autofill_assistant
