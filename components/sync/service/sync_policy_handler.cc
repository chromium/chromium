// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/sync_policy_handler.h"

#include <optional>
#include <string>

#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_prefs.h"

namespace syncer {
namespace {

void DisableSyncType(const std::string& type_name, PrefValueMap* prefs) {
  std::optional<UserSelectableType> type =
      GetUserSelectableTypeFromString(type_name);
  if (type.has_value()) {
    syncer::SyncPrefs::SetTypeDisabledByPolicy(prefs, *type);

    // The autofill policy also controls payments.
    if (*type == UserSelectableType::kAutofill) {
      syncer::SyncPrefs::SetTypeDisabledByPolicy(prefs,
                                                 UserSelectableType::kPayments);
    }
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Check for OS types. This includes types that used to be browser types,
  // like "apps" and "preferences".
  std::optional<UserSelectableOsType> os_type =
      GetUserSelectableOsTypeFromString(type_name);
  if (os_type.has_value()) {
    syncer::SyncPrefs::SetOsTypeDisabledByPolicy(prefs, *os_type);
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

}  // namespace

SyncPolicyHandler::SyncPolicyHandler()
    : policy::TypeCheckingPolicyHandler(policy::key::kSyncDisabled,
                                        base::Value::Type::BOOLEAN) {}

SyncPolicyHandler::~SyncPolicyHandler() = default;

bool SyncPolicyHandler::CheckPolicySettings(const policy::PolicyMap& policies,
                                            policy::PolicyErrorMap* errors) {
  if (!policy::TypeCheckingPolicyHandler::CheckPolicySettings(policies,
                                                              errors)) {
    return false;
  }

  const base::Value* disabled_sync_types_value = policies.GetValue(
      policy::key::kSyncTypesListDisabled, base::Value::Type::LIST);
  if (disabled_sync_types_value) {
    const base::Value::List& list = disabled_sync_types_value->GetList();
    for (const base::Value& type_name : list) {
      if (!type_name.is_string()) {
        errors->AddError(policy::key::kSyncTypesListDisabled,
                         IDS_POLICY_TYPE_ERROR,
                         base::Value::GetTypeName(base::Value::Type::STRING),
                         {}, policy::PolicyMap::MessageType::kWarning);
        continue;
      }

      if (!GetUserSelectableTypeFromString(type_name.GetString())) {
        errors->AddError(policy::key::kSyncTypesListDisabled,
                         IDS_POLICY_INVALID_SELECTION_ERROR,
                         type_name.GetString(), {},
                         policy::PolicyMap::MessageType::kWarning);
      }
    }
  }

  return true;
}

void SyncPolicyHandler::ApplyPolicySettings(const policy::PolicyMap& policies,
                                            PrefValueMap* prefs) {
  const base::Value* disable_sync_value =
      policies.GetValue(policy_name(), base::Value::Type::BOOLEAN);
  if (disable_sync_value && disable_sync_value->GetBool()) {
    prefs->SetValue(prefs::internal::kSyncManaged, disable_sync_value->Clone());
  }

  const base::Value* disabled_sync_types_value = policies.GetValue(
      policy::key::kSyncTypesListDisabled, base::Value::Type::LIST);
  if (disabled_sync_types_value) {
    const base::Value::List& list = disabled_sync_types_value->GetList();
    for (const base::Value& type_name : list) {
      if (!type_name.is_string()) {
        continue;
      }
      DisableSyncType(type_name.GetString(), prefs);
    }
  }
}

}  // namespace syncer
