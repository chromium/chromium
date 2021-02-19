// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/trigger_scripts/static_trigger_conditions.h"

#include "base/callback.h"
#include "base/strings/string_number_conversions.h"
#include "components/autofill_assistant/browser/script_parameters.h"

namespace autofill_assistant {

StaticTriggerConditions::StaticTriggerConditions() = default;
StaticTriggerConditions::~StaticTriggerConditions() = default;

void StaticTriggerConditions::Init(
    WebsiteLoginManager* website_login_manager,
    base::RepeatingCallback<bool(void)> is_first_time_user_callback,
    const GURL& url,
    TriggerContext* trigger_context,
    base::OnceCallback<void(void)> callback) {
  DCHECK(!callback_)
      << "Call to Init while another call to Init was still pending";
  if (callback_) {
    return;
  }
  is_first_time_user_ = is_first_time_user_callback.Run();
  trigger_context_ = trigger_context;
  has_stored_login_credentials_ = false;

  callback_ = std::move(callback);
  website_login_manager->GetLoginsForUrl(
      url, base::BindOnce(&StaticTriggerConditions::OnGetLogins,
                          weak_ptr_factory_.GetWeakPtr()));
}

void StaticTriggerConditions::set_is_first_time_user(bool first_time_user) {
  is_first_time_user_ = first_time_user;
}

bool StaticTriggerConditions::is_first_time_user() const {
  return is_first_time_user_;
}

bool StaticTriggerConditions::has_stored_login_credentials() const {
  return has_stored_login_credentials_;
}

bool StaticTriggerConditions::is_in_experiment(int experiment_id) const {
  return trigger_context_->HasExperimentId(base::NumberToString(experiment_id));
}

bool StaticTriggerConditions::has_results() const {
  return has_results_;
}

bool StaticTriggerConditions::script_parameter_matches(
    const ScriptParameterMatchProto& param) const {
  return trigger_context_->GetScriptParameters().Matches(param);
}

void StaticTriggerConditions::OnGetLogins(
    std::vector<WebsiteLoginManager::Login> logins) {
  has_stored_login_credentials_ = !logins.empty();
  has_results_ = true;
  DCHECK(callback_);
  std::move(callback_).Run();
}

}  // namespace autofill_assistant
