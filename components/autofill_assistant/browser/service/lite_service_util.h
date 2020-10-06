// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_LITE_SERVICE_UTIL_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_LITE_SERVICE_UTIL_H_

#include <string>
#include "base/optional.h"
#include "components/autofill_assistant/browser/service.pb.h"

namespace autofill_assistant {
namespace lite_service_util {

// A classification for a particular |ProcessedActionProto|. Contains only
// values which are of interest to a |LiteService|. All action responses not
// explicitly covered have an ActionResponseType of |UNKNOWN|.
enum class ActionResponseType {
  UNKNOWN,
  PROMPT_NAVIGATE,
  PROMPT_INVISIBLE_AUTO_SELECT,
  PROMPT_CLOSE,
  PROMPT_DONE,
};

// Splits |proto| into two parts: the first one ending in the last occurring
// prompt(browse) action, the second one ending in a regular prompt action.
// Fails if the split is not possible.
base::Optional<std::pair<ActionsResponseProto, ActionsResponseProto>>
SplitActionsAtLastBrowse(const ActionsResponseProto& proto);

// Returns true if |proto| contains only 'safe' actions, where a safe action is
// defined as an action that does not modify the website and/or require
// communicating with the backend.
bool ContainsOnlySafeActions(const ActionsResponseProto& proto);

// Classifies |proto| into a response type.
ActionResponseType GetActionResponseType(const ProcessedActionProto& proto);

// Overwrites all payloads in |proto| with unique identifiers.
void AssignUniquePayloadsToPrompts(ActionsResponseProto* proto);

}  // namespace lite_service_util
}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_LITE_SERVICE_UTIL_H_
