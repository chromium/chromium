// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/client_context.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_util.h"
#include "components/version_info/version_info.h"
#include "crypto/sha2.h"

namespace autofill_assistant {

ClientContextImpl::ClientContextImpl(const Client* client) : client_(client) {
  proto_.mutable_chrome()->set_chrome_version(
      version_info::GetProductNameAndVersionForUserAgent());
  proto_.set_locale(client->GetLocale());
  proto_.set_country(client->GetCountryCode());

  base::FieldTrial::ActiveGroups active_groups;
  base::FieldTrialList::GetActiveFieldTrialGroups(&active_groups);
  for (const auto& group : active_groups) {
    FieldTrialProto* field_trial =
        proto_.mutable_chrome()->add_active_field_trials();
    field_trial->set_trial_name(group.trial_name);
    field_trial->set_group_name(group.group_name);
  }

  client_->GetDeviceContext().ToProto(proto_.mutable_device_context());
  Update(TriggerContext());
}

void ClientContextImpl::Update(const TriggerContext& trigger_context) {
  proto_.set_accessibility_enabled(client_->IsAccessibilityEnabled());
  std::string chrome_signed_in_email_address =
      client_->GetChromeSignedInEmailAddress();
  proto_.set_signed_into_chrome_status(chrome_signed_in_email_address.empty()
                                           ? ClientContextProto::NOT_SIGNED_IN
                                           : ClientContextProto::SIGNED_IN);

  std::string experiment_ids = trigger_context.GetExperimentIds();
  if (!experiment_ids.empty()) {
    proto_.set_experiment_ids(experiment_ids);
  }
  if (trigger_context.GetCCT()) {
    proto_.set_is_cct(true);
  }
  if (trigger_context.GetOnboardingShown()) {
    proto_.set_is_onboarding_shown(true);
  }
  if (trigger_context.GetDirectAction()) {
    proto_.set_is_direct_action(true);
  }

  // TODO(b/156882027): Add an integration test for accounts handling.
  std::string chrome_account_sha_bin =
      crypto::SHA256HashString(chrome_signed_in_email_address);
  std::string client_account_hash = base::ToLowerASCII(base::HexEncode(
      chrome_account_sha_bin.data(), chrome_account_sha_bin.size()));
  if (trigger_context.GetCallerAccountHash().empty()) {
    proto_.set_accounts_matching_status(ClientContextProto::UNKNOWN);
  } else {
    if (trigger_context.GetCallerAccountHash() == client_account_hash) {
      proto_.set_accounts_matching_status(
          ClientContextProto::ACCOUNTS_MATCHING);
    } else {
      proto_.set_accounts_matching_status(
          ClientContextProto::ACCOUNTS_NOT_MATCHING);
    }
  }
}

ClientContextProto ClientContextImpl::AsProto() const {
  return proto_;
}

ClientContextProto EmptyClientContext::AsProto() const {
  return ClientContextProto();
}

}  // namespace autofill_assistant
