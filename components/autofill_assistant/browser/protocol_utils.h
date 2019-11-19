// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PROTOCOL_UTILS_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PROTOCOL_UTILS_H_

#include <deque>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "components/autofill_assistant/browser/actions/action.h"
#include "components/autofill_assistant/browser/script.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/trigger_context.h"

class GURL;

namespace autofill_assistant {
// Autofill assistant protocol related convenient utils.
class ProtocolUtils {
 public:
  // Create getting autofill assistant scripts request for the given
  // |url|.
  static std::string CreateGetScriptsRequest(
      const GURL& url,
      const TriggerContext& trigger_context,
      const ClientContextProto& client_context);

  // Convert |script_proto| to a script struct and if the script is valid, add
  // it to |scripts|.
  static void AddScript(const SupportedScriptProto& script_proto,
                        std::vector<std::unique_ptr<Script>>* scripts);

  // Create initial request to get script actions for the given |script_path|.
  //
  // TODO(b/806868): Remove the script payload from initial requests once the
  // server has transitioned to global payloads.
  static std::string CreateInitialScriptActionsRequest(
      const std::string& script_path,
      const GURL& url,
      const TriggerContext& trigger_context,
      const std::string& global_payload,
      const std::string& script_payload,
      const ClientContextProto& client_context);

  // Create request to get next sequence of actions for a script.
  static std::string CreateNextScriptActionsRequest(
      const TriggerContext& trigger_context,
      const std::string& global_payload,
      const std::string& script_payload,
      const std::vector<ProcessedActionProto>& processed_actions,
      const ClientContextProto& client_context);

  // Parse actions from the given |response|, which can be an empty string.
  //
  // Pass in nullptr for |return_global_payload| or |return_script_payload| to
  // indicate no need to return that payload. Parsed actions are returned
  // through |actions|, which should not be nullptr. Optionally, parsed scripts
  // are returned through |scripts| and used to update the list of cached
  // scripts. The bool |should_update_scripts| makes clear the destinction
  // between an empty list of |scripts| or the scripts field not even set in the
  // proto. Return false if parse failed, otherwise return true.
  static bool ParseActions(ActionDelegate* delegate,
                           const std::string& response,
                           std::string* return_global_payload,
                           std::string* return_script_payload,
                           std::vector<std::unique_ptr<Action>>* actions,
                           std::vector<std::unique_ptr<Script>>* scripts,
                           bool* should_update_scripts);

 private:
  // To avoid instantiate this class by accident.
  ProtocolUtils() = delete;
  ~ProtocolUtils() = delete;
};
}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PROTOCOL_UTILS_H_
