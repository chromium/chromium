// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/proxy_config/proxy_policy_handler.h"

#include <stddef.h>

#include "base/check.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/proxy_settings_constants.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/strings/grit/components_strings.h"

namespace proxy_config {

namespace {

using policy::kProxyPacMandatory;
using policy::PolicyErrorMap;
using policy::PolicyErrorPath;
using policy::PolicyMap;
using policy::key::kProxyBypassList;
using policy::key::kProxyMode;
using policy::key::kProxyPacUrl;
using policy::key::kProxyServer;
using policy::key::kProxyServerMode;
using policy::key::kProxySettings;

// This is used to check whether for a given ProxyMode value, the ProxyPacUrl,
// the ProxyBypassList and the ProxyServer policies are allowed to be specified.
// |error_message_id| is the message id of the localized error message to show
// when the policies are not specified as allowed. Each value of ProxyMode
// has a ProxyModeValidationEntry in the |kProxyModeValidationMap| below.
struct ProxyModeValidationEntry {
  const char* mode_value;
  bool pac_url_allowed;
  bool pac_mandatory_allowed;
  bool bypass_list_allowed;
  bool server_allowed;
  int error_message_id;
};

// List of entries determining which proxy policies can be specified, depending
// on the ProxyMode.
constexpr ProxyModeValidationEntry kProxyModeValidationMap[] = {
    {ProxyPrefs::kDirectProxyModeName, false, false, false, false,
     IDS_POLICY_PROXY_MODE_DISABLED_ERROR},
    {ProxyPrefs::kAutoDetectProxyModeName, false, false, false, false,
     IDS_POLICY_PROXY_MODE_AUTO_DETECT_ERROR},
    {ProxyPrefs::kPacScriptProxyModeName, true, true, false, false,
     IDS_POLICY_PROXY_MODE_PAC_URL_ERROR},
    {ProxyPrefs::kFixedServersProxyModeName, false, false, true, true,
     IDS_POLICY_PROXY_MODE_FIXED_SERVERS_ERROR},
    {ProxyPrefs::kSystemProxyModeName, false, false, false, false,
     IDS_POLICY_PROXY_MODE_SYSTEM_ERROR},
};

// Cannot be constexpr because the values of the strings are defined in an
// automatically generated .cc file.
const char* const kDeprecatedProxyPolicies[] = {
    kProxyMode, kProxyServerMode, kProxyServer, kProxyPacUrl, kProxyBypassList,
};

const base::Value* GetProxyPolicyValue(const base::Value* value,
                                       const char* policy_name) {
  if (!value) {
    return nullptr;
  }
  const base::Value::Dict* settings = value->GetIfDict();
  if (!settings) {
    return nullptr;
  }

  const base::Value* policy_value = settings->Find(policy_name);
  if (!policy_value || policy_value->is_none()) {
    return nullptr;
  }
  const std::string* tmp = policy_value->GetIfString();
  if (tmp && tmp->empty()) {
    return nullptr;
  }
  return policy_value;
}

// Converts the deprecated ProxyServerMode policy value to a ProxyMode value
// and places the result in |mode_value|. Returns whether the conversion
// succeeded.
bool CheckProxyModeAndServerMode(const base::Value* proxy_settings,
                                 PolicyErrorMap* errors,
                                 std::string* mode_value) {
  const base::Value* mode = GetProxyPolicyValue(proxy_settings, kProxyMode);
  const base::Value* server = GetProxyPolicyValue(proxy_settings, kProxyServer);
  const base::Value* server_mode =
      GetProxyPolicyValue(proxy_settings, kProxyServerMode);
  const base::Value* pac_url =
      GetProxyPolicyValue(proxy_settings, kProxyPacUrl);

  // If there's a server mode, convert it into a mode.
  // When both are specified, the mode takes precedence.
  if (mode) {
    if (server_mode) {
      errors->AddError(kProxySettings, IDS_POLICY_OVERRIDDEN, kProxyMode,
                       PolicyErrorPath{kProxyServerMode});
    }
    if (!mode->is_string()) {
      errors->AddError(kProxySettings, IDS_POLICY_TYPE_ERROR,
                       base::Value::GetTypeName(base::Value::Type::BOOLEAN),
                       PolicyErrorPath{kProxyMode});
      return false;
    }
    *mode_value = mode->GetString();

    ProxyPrefs::ProxyMode proxy_mode;
    if (!ProxyPrefs::StringToProxyMode(*mode_value, &proxy_mode)) {
      errors->AddError(kProxySettings, IDS_POLICY_INVALID_PROXY_MODE_ERROR,
                       PolicyErrorPath{kProxyMode});
      return false;
    }

    if (proxy_mode == ProxyPrefs::MODE_PAC_SCRIPT && !pac_url) {
      errors->AddError(kProxySettings, IDS_POLICY_NOT_SPECIFIED_ERROR,
                       PolicyErrorPath{kProxyPacUrl});
      return false;
    }
    if (proxy_mode == ProxyPrefs::MODE_FIXED_SERVERS && !server) {
      errors->AddError(kProxySettings, IDS_POLICY_NOT_SPECIFIED_ERROR,
                       PolicyErrorPath{kProxyServer});
      return false;
    }
  } else if (server_mode) {
    if (!server_mode->is_int()) {
      errors->AddError(kProxySettings, IDS_POLICY_TYPE_ERROR,
                       base::Value::GetTypeName(base::Value::Type::INTEGER),
                       PolicyErrorPath{kProxyServerMode});
      return false;
    }

    switch (server_mode->GetInt()) {
      case ProxyPolicyHandler::PROXY_SERVER_MODE:
        *mode_value = ProxyPrefs::kDirectProxyModeName;
        break;
      case ProxyPolicyHandler::PROXY_AUTO_DETECT_PROXY_SERVER_MODE:
        *mode_value = ProxyPrefs::kAutoDetectProxyModeName;
        break;
      case ProxyPolicyHandler::PROXY_MANUALLY_CONFIGURED_PROXY_SERVER_MODE:
        if (server && pac_url) {
          int message_id = IDS_POLICY_PROXY_BOTH_SPECIFIED_ERROR;
          errors->AddError(kProxySettings, message_id,
                           PolicyErrorPath{kProxyServer});
          errors->AddError(kProxySettings, message_id,
                           PolicyErrorPath{kProxyPacUrl});
          return false;
        }
        if (!server && !pac_url) {
          int message_id = IDS_POLICY_PROXY_NEITHER_SPECIFIED_ERROR;
          errors->AddError(kProxySettings, message_id,
                           PolicyErrorPath{kProxyServer});
          errors->AddError(kProxySettings, message_id,
                           PolicyErrorPath{kProxyPacUrl});
          return false;
        }
        *mode_value = pac_url ? ProxyPrefs::kPacScriptProxyModeName
                              : ProxyPrefs::kFixedServersProxyModeName;
        break;
      case ProxyPolicyHandler::PROXY_USE_SYSTEM_PROXY_SERVER_MODE:
        *mode_value = ProxyPrefs::kSystemProxyModeName;
        break;
      default:
        errors->AddError(kProxySettings, IDS_POLICY_OUT_OF_RANGE_ERROR,
                         base::NumberToString(server_mode->GetInt()),
                         PolicyErrorPath{kProxyServerMode});
        return false;
    }
  }
  return true;
}

// Maps the separate deprecated policies for proxy settings into a single
// Dictionary policy. This allows to keep the logic of merging policies from
// different sources simple, as all separate proxy policies should be considered
// as a single whole during merging. Returns proxy_settings value.
base::Value RemapProxyPolicies(const PolicyMap& policies) {
  // The highest (level, scope) pair for an existing proxy policy is determined
  // first, and then only policies with those exact attributes are merged.
  PolicyMap::Entry current_priority;  // Defaults to the lowest priority.
  policy::PolicySource inherited_source =
      policy::POLICY_SOURCE_ENTERPRISE_DEFAULT;
  base::Value::Dict proxy_settings;
  for (auto* policy : kDeprecatedProxyPolicies) {
    const PolicyMap::Entry* entry = policies.Get(policy);
    if (!entry)
      continue;
    if (policies.EntryHasHigherPriority(*entry, current_priority)) {
      current_priority = entry->DeepCopy();
      if (entry->source > inherited_source)  // Higher priority?
        inherited_source = entry->source;
    }
    // If two priorities are the same.
    if (!policies.EntryHasHigherPriority(*entry, current_priority) &&
        !policies.EntryHasHigherPriority(current_priority, *entry)) {
      // |value_unsafe| is used due to multiple policy types being handled.
      proxy_settings.Set(policy, entry->value_unsafe()->Clone());
    }
  }
  // Sets the new |proxy_settings| if kProxySettings isn't set yet, or if the
  // new priority is higher.
  const PolicyMap::Entry* existing = policies.Get(kProxySettings);
  if (!proxy_settings.empty() &&
      (!existing ||
       policies.EntryHasHigherPriority(current_priority, *existing))) {
    return base::Value(std::move(proxy_settings));
  } else if (existing && existing->value(base::Value::Type::DICT)) {
    return existing->value(base::Value::Type::DICT)->Clone();
  }
  return base::Value();
}

}  // namespace

// The proxy policies have the peculiarity that they are loaded from individual
// policies, but the providers then expose them through a unified
// DictionaryValue.

ProxyPolicyHandler::ProxyPolicyHandler() {}

ProxyPolicyHandler::~ProxyPolicyHandler() {}

bool ProxyPolicyHandler::CheckPolicySettings(const PolicyMap& policies,
                                             PolicyErrorMap* errors) {
  base::Value proxy_settings = RemapProxyPolicies(policies);
  const base::Value* mode = GetProxyPolicyValue(&proxy_settings, kProxyMode);
  const base::Value* server =
      GetProxyPolicyValue(&proxy_settings, kProxyServer);
  const base::Value* server_mode =
      GetProxyPolicyValue(&proxy_settings, kProxyServerMode);
  const base::Value* pac_url =
      GetProxyPolicyValue(&proxy_settings, kProxyPacUrl);
  const base::Value* pac_mandatory =
      GetProxyPolicyValue(&proxy_settings, kProxyPacMandatory);
  const base::Value* bypass_list =
      GetProxyPolicyValue(&proxy_settings, kProxyBypassList);

  if ((server || pac_url || bypass_list) && !(mode || server_mode)) {
    errors->AddError(kProxySettings, IDS_POLICY_NOT_SPECIFIED_ERROR,
                     PolicyErrorPath{kProxyMode});
    return false;
  }

  std::string mode_value;
  if (!CheckProxyModeAndServerMode(&proxy_settings, errors, &mode_value))
    return false;

  // If neither ProxyMode nor ProxyServerMode are specified, mode_value will be
  // empty and the proxy shouldn't be configured at all.
  if (mode_value.empty())
    return true;

  bool is_valid_mode = false;
  for (size_t i = 0; i != std::size(kProxyModeValidationMap); ++i) {
    const ProxyModeValidationEntry& entry = kProxyModeValidationMap[i];
    if (entry.mode_value != mode_value)
      continue;

    is_valid_mode = true;

    if (!entry.pac_url_allowed && pac_url) {
      errors->AddError(kProxySettings, entry.error_message_id,
                       PolicyErrorPath{kProxyPacUrl});
    }
    if (!entry.pac_mandatory_allowed && pac_mandatory) {
      errors->AddError(kProxySettings, entry.error_message_id,
                       PolicyErrorPath{kProxyPacMandatory});
    }
    if (!entry.bypass_list_allowed && bypass_list) {
      errors->AddError(kProxySettings, entry.error_message_id,
                       PolicyErrorPath{kProxyBypassList});
    }
    if (!entry.server_allowed && server) {
      errors->AddError(kProxySettings, entry.error_message_id,
                       PolicyErrorPath{kProxyServer});
    }

    if ((!entry.pac_url_allowed && pac_url) ||
        (!entry.pac_mandatory_allowed && pac_mandatory) ||
        (!entry.bypass_list_allowed && bypass_list) ||
        (!entry.server_allowed && server)) {
      return false;
    }
  }

  if (!is_valid_mode) {
    errors->AddError(kProxySettings, IDS_POLICY_OUT_OF_RANGE_ERROR, mode_value,
                     PolicyErrorPath{mode ? kProxyMode : kProxyServerMode});
    return false;
  }
  return true;
}

void ProxyPolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                             PrefValueMap* prefs) {
  base::Value proxy_settings = RemapProxyPolicies(policies);
  const base::Value* mode = GetProxyPolicyValue(&proxy_settings, kProxyMode);
  const base::Value* server =
      GetProxyPolicyValue(&proxy_settings, kProxyServer);
  const base::Value* server_mode =
      GetProxyPolicyValue(&proxy_settings, kProxyServerMode);
  const base::Value* pac_url =
      GetProxyPolicyValue(&proxy_settings, kProxyPacUrl);
  const base::Value* pac_mandatory =
      GetProxyPolicyValue(&proxy_settings, kProxyPacMandatory);
  const base::Value* bypass_list =
      GetProxyPolicyValue(&proxy_settings, kProxyBypassList);

  ProxyPrefs::ProxyMode proxy_mode;
  if (mode) {
    CHECK(mode->is_string());
    CHECK(ProxyPrefs::StringToProxyMode(mode->GetString(), &proxy_mode));
  } else if (server_mode) {
    switch (server_mode->GetInt()) {
      case PROXY_SERVER_MODE:
        proxy_mode = ProxyPrefs::MODE_DIRECT;
        break;
      case PROXY_AUTO_DETECT_PROXY_SERVER_MODE:
        proxy_mode = ProxyPrefs::MODE_AUTO_DETECT;
        break;
      case PROXY_MANUALLY_CONFIGURED_PROXY_SERVER_MODE:
        proxy_mode = ProxyPrefs::MODE_FIXED_SERVERS;
        if (pac_url)
          proxy_mode = ProxyPrefs::MODE_PAC_SCRIPT;
        break;
      case PROXY_USE_SYSTEM_PROXY_SERVER_MODE:
        proxy_mode = ProxyPrefs::MODE_SYSTEM;
        break;
      default:
        proxy_mode = ProxyPrefs::MODE_DIRECT;
        NOTREACHED_IN_MIGRATION();
    }
  } else {
    return;
  }

  auto set_proxy_pref_value = [&prefs](base::Value::Dict dict) {
    prefs->SetValue(prefs::kProxy, base::Value(std::move(dict)));
  };

  switch (proxy_mode) {
    case ProxyPrefs::MODE_DIRECT:
      set_proxy_pref_value(ProxyConfigDictionary::CreateDirect());
      break;
    case ProxyPrefs::MODE_AUTO_DETECT:
      set_proxy_pref_value(ProxyConfigDictionary::CreateAutoDetect());
      break;
    case ProxyPrefs::MODE_PAC_SCRIPT: {
      if (pac_url && pac_url->is_string()) {
        bool mandatory =
            pac_mandatory && pac_mandatory->GetIfBool().value_or(false);
        set_proxy_pref_value(ProxyConfigDictionary::CreatePacScript(
            pac_url->GetString(), mandatory));
      } else {
        NOTREACHED_IN_MIGRATION();
      }
      break;
    }
    case ProxyPrefs::MODE_FIXED_SERVERS: {
      if (server->is_string()) {
        set_proxy_pref_value(ProxyConfigDictionary::CreateFixedServers(
            server->GetString(), bypass_list && bypass_list->is_string()
                                     ? bypass_list->GetString()
                                     : std::string()));
      }
      break;
    }
    case ProxyPrefs::MODE_SYSTEM:
      set_proxy_pref_value(ProxyConfigDictionary::CreateSystem());
      break;
    case ProxyPrefs::kModeCount:
      NOTREACHED_IN_MIGRATION();
  }
}

}  // namespace proxy_config
