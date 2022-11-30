// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cup.h"

#include "base/feature_list.h"
#include "components/autofill_assistant/browser/features.h"

namespace autofill_assistant::cup {

using ::autofill_assistant::features::kAutofillAssistantSignGetActionsRequests;
using ::autofill_assistant::features::
    kAutofillAssistantSignGetNoRoundTripScriptsByHashRequests;
using ::autofill_assistant::features::
    kAutofillAssistantVerifyGetActionsResponses;
using ::autofill_assistant::features::
    kAutofillAssistantVerifyGetNoRoundTripScriptsByHashResponses;
using ::base::FeatureList;

bool ShouldSignRequests(RpcType rpc_type) {
  switch (rpc_type) {
    case RpcType::GET_ACTIONS:
      return FeatureList::IsEnabled(kAutofillAssistantSignGetActionsRequests);
    case RpcType::GET_NO_ROUNDTRIP_SCRIPTS_BY_HASH_PREFIX:
      return FeatureList::IsEnabled(
          kAutofillAssistantSignGetNoRoundTripScriptsByHashRequests);
    default:
      return false;
  }
}

bool ShouldVerifyResponses(RpcType rpc_type) {
  if (!ShouldSignRequests(rpc_type))
    return false;
  switch (rpc_type) {
    case RpcType::GET_ACTIONS:
      return FeatureList::IsEnabled(
          kAutofillAssistantVerifyGetActionsResponses);
    case RpcType::GET_NO_ROUNDTRIP_SCRIPTS_BY_HASH_PREFIX:
      return FeatureList::IsEnabled(
          kAutofillAssistantVerifyGetNoRoundTripScriptsByHashResponses);
    default:
      return false;
  }
}

bool IsRpcTypeSupported(RpcType rpc_type) {
  return rpc_type == RpcType::GET_ACTIONS ||
         rpc_type == RpcType::GET_NO_ROUNDTRIP_SCRIPTS_BY_HASH_PREFIX;
}

}  // namespace autofill_assistant::cup
