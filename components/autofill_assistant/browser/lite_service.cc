// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/lite_service.h"
#include "components/autofill_assistant/browser/lite_service_util.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"

namespace autofill_assistant {

LiteService::LiteService(
    std::unique_ptr<Service> service_impl,
    const std::string& trigger_script_path,
    base::OnceCallback<void(Metrics::LiteScriptFinishedState)>
        notify_finished_callback,
    base::RepeatingCallback<void(bool)> notify_script_running_callback)
    : service_impl_(std::move(service_impl)),
      trigger_script_path_(trigger_script_path),
      notify_finished_callback_(std::move(notify_finished_callback)),
      notify_script_running_callback_(
          std::move(notify_script_running_callback)) {}

LiteService::~LiteService() {
  if (notify_finished_callback_) {
    std::move(notify_finished_callback_)
        .Run(Metrics::LiteScriptFinishedState::LITE_SCRIPT_SERVICE_DELETED);
  }
}

bool LiteService::IsLiteService() const {
  return true;
}

void LiteService::GetScriptsForUrl(const GURL& url,
                                   const TriggerContext& trigger_context,
                                   ResponseCallback callback) {
  SupportsScriptResponseProto response;
  auto* lite_script = response.add_scripts();
  lite_script->set_path(trigger_script_path_);
  lite_script->mutable_presentation()->set_autostart(true);
  lite_script->mutable_presentation()->set_needs_ui(false);
  lite_script->mutable_presentation()->mutable_chip()->set_text("autostart");

  std::string serialized_response;
  response.SerializeToString(&serialized_response);
  std::move(callback).Run(true, serialized_response);
}

void LiteService::GetActions(const std::string& script_path,
                             const GURL& url,
                             const TriggerContext& do_not_use_trigger_context,
                             const std::string& do_not_use_global_payload,
                             const std::string& do_not_use_script_payload,
                             ResponseCallback callback) {
  // Should never happen, but let's guard for this just in case.
  if (script_path != trigger_script_path_) {
    StopWithoutErrorMessage(
        std::move(callback),
        Metrics::LiteScriptFinishedState::LITE_SCRIPT_PATH_MISMATCH);
    return;
  }

  // Note: trigger context and payloads should not be sent to the backend for
  // privacy reasons.
  service_impl_->GetActions(
      trigger_script_path_, GURL(), TriggerContextImpl(),
      /* global_payload = */ std::string(),
      /* script_payload = */ std::string(),
      base::BindOnce(&LiteService::OnGetActions, weak_ptr_factory_.GetWeakPtr(),
                     std::move(callback)));
}

void LiteService::OnGetActions(ResponseCallback callback,
                               bool result,
                               const std::string& response) {
  if (!result) {
    StopWithoutErrorMessage(
        std::move(callback),
        Metrics::LiteScriptFinishedState::LITE_SCRIPT_GET_ACTIONS_FAILED);
    return;
  }

  ActionsResponseProto response_proto;
  if (!response_proto.ParseFromString(response)) {
    StopWithoutErrorMessage(
        std::move(callback),
        Metrics::LiteScriptFinishedState::LITE_SCRIPT_GET_ACTIONS_PARSE_ERROR);
    return;
  }

  if (!lite_service_util::ContainsOnlySafeActions(response_proto)) {
    StopWithoutErrorMessage(
        std::move(callback),
        Metrics::LiteScriptFinishedState::LITE_SCRIPT_UNSAFE_ACTIONS);
    return;
  }

  // Ensure that each prompt choice has a unique payload. This is necessary in
  // order to map the chosen payload back to the action it originated from.
  lite_service_util::AssignUniquePayloadsToPrompts(&response_proto);

  // Prepend a ConfigureUi action to ensure that the overlay will not be shown.
  ActionProto configure_ui;
  configure_ui.mutable_configure_ui_state()->set_overlay_behavior(
      ConfigureUiStateProto::HIDDEN);
  *response_proto.add_actions() = configure_ui;
  for (int i = response_proto.actions().size() - 1; i > 0; --i) {
    response_proto.mutable_actions()->SwapElements(i, i - 1);
  }

  auto split_actions =
      lite_service_util::SplitActionsAtLastBrowse(response_proto);
  if (!split_actions.has_value()) {
    StopWithoutErrorMessage(
        std::move(callback),
        Metrics::LiteScriptFinishedState::LITE_SCRIPT_INVALID_SCRIPT);
    return;
  }

  // Serve the first part of the actions now, and the second part in
  // |GetNextActions| upon successful completion of the first part.
  trigger_script_second_part_ =
      std::make_unique<ActionsResponseProto>(split_actions->second);

  std::string serialized_first_part;
  split_actions->first.SerializeToString(&serialized_first_part);
  std::move(callback).Run(result, serialized_first_part);
  notify_script_running_callback_.Run(/*ui_shown = */ false);
}

void LiteService::GetNextActions(
    const TriggerContext& trigger_context,
    const std::string& previous_global_payload,
    const std::string& previous_script_payload,
    const std::vector<ProcessedActionProto>& processed_actions,
    ResponseCallback callback) {
  if (!notify_finished_callback_) {
    // The lite script has already terminated. We need to run |callback| with
    // |success|=true and an empty response to ensure a graceful stop of the
    // script (i.e., without error message).
    std::move(callback).Run(true, std::string());
    return;
  }

  if (processed_actions.empty()) {
    StopWithoutErrorMessage(
        std::move(callback),
        Metrics::LiteScriptFinishedState::LITE_SCRIPT_UNKNOWN_FAILURE);
    return;
  }

  if (trigger_script_second_part_ != nullptr) {
    // First part (browse) finished. It can only finish successfully if the
    // auto-select condition is fulfilled, i.e., PROMPT_INVISIBLE_AUTO_SELECT.
    switch (
        lite_service_util::GetActionResponseType(processed_actions.back())) {
      case lite_service_util::ActionResponseType::UNKNOWN:
      case lite_service_util::ActionResponseType::PROMPT_CLOSE:
      case lite_service_util::ActionResponseType::PROMPT_DONE:
        StopWithoutErrorMessage(
            std::move(callback),
            Metrics::LiteScriptFinishedState::LITE_SCRIPT_BROWSE_FAILED_OTHER);
        return;
      case lite_service_util::ActionResponseType::PROMPT_NAVIGATE:
        StopWithoutErrorMessage(std::move(callback),
                                Metrics::LiteScriptFinishedState::
                                    LITE_SCRIPT_BROWSE_FAILED_NAVIGATE);
        return;
      case lite_service_util::ActionResponseType::PROMPT_INVISIBLE_AUTO_SELECT:
        // Success, serve the second part of the actions now.
        std::string serialized_second_part;
        trigger_script_second_part_->SerializeToString(&serialized_second_part);
        trigger_script_second_part_.reset();
        std::move(callback).Run(true, serialized_second_part);
        notify_script_running_callback_.Run(/*ui_shown = */ true);
        return;
    }
  } else {
    // Second part (prompt) finished, the lite script must end now. There are
    // three possible valid outcomes: user cancels (PROMPT_CLOSE), user agrees
    // (PROMPT_DONE), or user navigated away from trigger page
    // (PROMPT_INVISIBLE_AUTO_SELECT).
    switch (
        lite_service_util::GetActionResponseType(processed_actions.back())) {
      case lite_service_util::ActionResponseType::UNKNOWN:
        StopWithoutErrorMessage(
            std::move(callback),
            Metrics::LiteScriptFinishedState::LITE_SCRIPT_PROMPT_FAILED_OTHER);
        return;
      case lite_service_util::ActionResponseType::PROMPT_CLOSE:
        StopWithoutErrorMessage(
            std::move(callback),
            Metrics::LiteScriptFinishedState::LITE_SCRIPT_PROMPT_FAILED_CLOSE);
        return;
      case lite_service_util::ActionResponseType::PROMPT_INVISIBLE_AUTO_SELECT:
        StopWithoutErrorMessage(
            std::move(callback),
            Metrics::LiteScriptFinishedState::
                LITE_SCRIPT_PROMPT_FAILED_CONDITION_NO_LONGER_TRUE);
        return;
      case lite_service_util::ActionResponseType::PROMPT_NAVIGATE:
        StopWithoutErrorMessage(std::move(callback),
                                Metrics::LiteScriptFinishedState::
                                    LITE_SCRIPT_PROMPT_FAILED_NAVIGATE);
        return;
      case lite_service_util::ActionResponseType::PROMPT_DONE:
        StopWithoutErrorMessage(
            std::move(callback),
            Metrics::LiteScriptFinishedState::LITE_SCRIPT_PROMPT_SUCCEEDED);
        return;
    }
  }
}

void LiteService::StopWithoutErrorMessage(
    ResponseCallback callback,
    Metrics::LiteScriptFinishedState state) {
  // Notify delegate BEFORE terminating the controller. See comment in header
  // for |OnFinished|.
  if (notify_finished_callback_) {
    std::move(notify_finished_callback_).Run(state);
  }

  // Stop script.
  ActionsResponseProto response;
  response.add_actions()->mutable_stop();
  std::string serialized_response;
  response.SerializeToString(&serialized_response);
  std::move(callback).Run(true, serialized_response);
}

}  // namespace autofill_assistant
