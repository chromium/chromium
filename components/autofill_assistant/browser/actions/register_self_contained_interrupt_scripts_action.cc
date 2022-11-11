// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/register_self_contained_interrupt_scripts_action.h"

#include <utility>

#include "base/callback.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/script.h"
#include "components/autofill_assistant/browser/script_precondition.h"
#include "components/autofill_assistant/browser/service/local_script_store.h"
#include "components/autofill_assistant/browser/service/no_round_trip_service.h"

namespace autofill_assistant {

RegisterSelfContainedInterruptScriptsAction::
    RegisterSelfContainedInterruptScriptsAction(ActionDelegate* delegate,
                                                const ActionProto& proto)
    : Action(delegate, proto) {
  DCHECK(proto_.has_register_interrupt_scripts());
}

RegisterSelfContainedInterruptScriptsAction::
    ~RegisterSelfContainedInterruptScriptsAction() = default;

void RegisterSelfContainedInterruptScriptsAction::InternalProcessAction(
    ProcessActionCallback callback) {
  callback_ = std::move(callback);

  const GetNoRoundTripScriptsByHashPrefixResponseProto::MatchInfo& match_info =
      proto_.register_interrupt_scripts().match_info();
  if (match_info.supports_site_response().scripts().size() !=
      match_info.routine_scripts().size()) {
    LOG(ERROR) << __func__
               << ": proto contained different number of routine_scripts and "
                  "SupportsSiteResponse scripts";
    EndAction(ClientStatus(INVALID_ACTION));
    return;
  }

  for (const auto& supports_site_script :
       match_info.supports_site_response().scripts()) {
    if (!supports_site_script.presentation().interrupt()) {
      LOG(ERROR) << __func__ << ": attempted to add a non-interrupt script";
      EndAction(ClientStatus(INVALID_ACTION));
      return;
    }
  }

  for (int i = 0; i < match_info.supports_site_response().scripts().size();
       ++i) {
    const auto& supports_site_script =
        match_info.supports_site_response().scripts(i);
    const auto& routine = match_info.routine_scripts(i);

    if (supports_site_script.path() != routine.script_path()) {
      LOG(ERROR)
          << __func__
          << ": order of SupportsSite scripts and routines was different";
      EndAction(ClientStatus(INVALID_ACTION));
      return;
    }

    // Configure a self-contained service for each of the specified new
    // interrupt scripts (one service per script).
    SupportsScriptResponseProto supports_site_response;
    *supports_site_response.add_scripts() = supports_site_script;

    auto script = std::make_unique<Script>();
    script->precondition = ScriptPrecondition::FromProto(
        supports_site_script.path(),
        supports_site_script.presentation().precondition());
    script->handle.path = supports_site_script.path();
    script->handle.interrupt = true;

    delegate_->AddInterruptScript(
        std::move(script),
        std::make_unique<NoRoundTripService>(std::make_unique<LocalScriptStore>(
            std::vector<GetNoRoundTripScriptsByHashPrefixResponseProto::
                            MatchInfo::RoutineScript>{routine},
            /* domain = */ std::string(), supports_site_response)));
  }

  EndAction(ClientStatus(ACTION_APPLIED));
}

void RegisterSelfContainedInterruptScriptsAction::EndAction(
    const ClientStatus& status) {
  UpdateProcessedAction(status);
  std::move(callback_).Run(std::move(processed_action_proto_));
}

}  // namespace autofill_assistant
