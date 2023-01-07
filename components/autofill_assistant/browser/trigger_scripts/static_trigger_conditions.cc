// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/trigger_scripts/static_trigger_conditions.h"

#include "base/callback.h"
#include "base/strings/string_number_conversions.h"
#include "components/autofill_assistant/browser/script_parameters.h"
#include "components/autofill_assistant/browser/starter_platform_delegate.h"

namespace autofill_assistant {

StaticTriggerConditions::StaticTriggerConditions(
    base::WeakPtr<StarterPlatformDelegate> delegate,
    TriggerContext* trigger_context,
    const GURL& deeplink_url)
    : delegate_(delegate),
      trigger_context_(trigger_context),
      deeplink_url_(deeplink_url) {}
StaticTriggerConditions::~StaticTriggerConditions() = default;

void StaticTriggerConditions::Update(base::OnceCallback<void(void)> callback) {
  DCHECK(!callback_)
      << "Call to Update while another call to Update was still pending";
  if (callback_) {
    return;
  }

  callback_ = std::move(callback);
  is_first_time_user_ = delegate_->GetIsFirstTimeUser();
  has_stored_login_credentials_ = false;
  delegate_->GetWebsiteLoginManager()->GetLoginsForUrl(
      deeplink_url_, base::BindOnce(&StaticTriggerConditions::OnGetLogins,
                                    weak_ptr_factory_.GetWeakPtr()));
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
