// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/proxy_policy_handler.h"

#include <stddef.h>

#include "base/check.h"
#include "base/notreached.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/strings/grit/components_strings.h"

namespace {

// This is used to check whether for a given ProxyMode value, the ProxyPacUrl,
// the ProxyBypassList and the ProxyServer policies are allowed to be specified.
// |error_message_id| is the message id of the localized error message to show
// when the policies are not specified as allowed. Each value of ProxyMode
// has a ProxyModeValidationEntry in the |kProxyModeValidationMap| below.
struct ProxyModeValidationEntry {
  const char* mode_value;
  bool pac_url_allowed;
  bool bypass_list_allowed;
  bool server_allowed;
  int error_message_id;
};

// List of entries determining which proxy policies can be specified, depending
// on the ProxyMode.
const ProxyModeValidationEntry kProxyModeValidationMap[] = {
  { ProxyPrefs::kDirectProxyModeName,
    false, false, false, IDS_POLICY_PROXY_MODE_DISABLED_ERROR },
  { ProxyPrefs::kAutoDetectProxyModeName,
    false, false, false, IDS_POLICY_PROXY_MODE_AUTO_DETECT_ERROR },
  { ProxyPrefs::kPacScriptProxyModeName,
    true, false, false, IDS_POLICY_PROXY_MODE_PAC_URL_ERROR },
  { ProxyPrefs::kFixedServersProxyModeName,
    false, true, true, IDS_POLICY_PROXY_MODE_FIXED_SERVERS_ERROR },
  { ProxyPrefs::kSystemProxyModeName,
    false, false, false, IDS_POLICY_PROXY_MODE_SYSTEM_ERROR },
};

}  // namespace

namespace policy {

// The proxy policies have the peculiarity that they are loaded from individual
// policies, but the providers then expose them through a unified
// DictionaryValue.

ProxyPolicyHandler::ProxyPolicyHandler() {}

ProxyPolicyHandler::~ProxyPolicyHandler() {
}

bool ProxyPolicyHandler::CheckPolicySettings(const PolicyMap& policies,
                                             PolicyErrorMap* errors) {
  const base::Value* mode = GetProxyPolicyValue(policies, key::kProxyMode);
  const base::Value* server = GetProxyPolicyValue(policies, key::kProxyServer);
  const base::Value* server_mode =
      GetProxyPolicyValue(policies, key::kProxyServerMode);
  const base::Value* pac_url = GetProxyPolicyValue(policies, key::kProxyPacUrl);
  const base::Value* bypass_list =
      GetProxyPolicyValue(policies, key::kProxyBypassList);

  if ((server || pac_url || bypass_list) && !(mode || server_mode)) {
    errors->AddError(key::kProxySettings,
                     key::kProxyMode,
                     IDS_POLICY_NOT_SPECIFIED_ERROR);
    return false;
  }

  std::string mode_value;
  if (!CheckProxyModeAndServerMode(policies, errors, &mode_value))
    return false;

  // If neither ProxyMode nor ProxyServerMode are specified, mode_value will be
  // empty and the proxy shouldn't be configured at all.
  if (mode_value.empty())
    return true;

  bool is_valid_mode = false;
  for (size_t i = 0; i != base::size(kProxyModeValidationMap); ++i) {
    const ProxyModeValidationEntry& entry = kProxyModeValidationMap[i];
    if (entry.mode_value != mode_value)
      continue;

    is_valid_mode = true;

    if (!entry.pac_url_allowed && pac_url) {
      errors->AddError(key::kProxySettings,
                       key::kProxyPacUrl,
                       entry.error_message_id);
    }
    if (!entry.bypass_list_allowed && bypass_list) {
      errors->AddError(key::kProxySettings,
                       key::kProxyBypassList,
                       entry.error_message_id);
    }
    if (!entry.server_allowed && server) {
      errors->AddError(key::kProxySettings,
                       key::kProxyServer,
                       entry.error_message_id);
    }

    if ((!entry.pac_url_allowed && pac_url) ||
        (!entry.bypass_list_allowed && bypass_list) ||
        (!entry.server_allowed && server)) {
      return false;
    }
  }

  if (!is_valid_mode) {
    errors->AddError(key::kProxySettings,
                     mode ? key::kProxyMode : key::kProxyServerMode,
                     IDS_POLICY_OUT_OF_RANGE_ERROR,
                     mode_value);
    return false;
  }
  return true;
}

void ProxyPolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                             PrefValueMap* prefs) {
  const base::Value* mode = GetProxyPolicyValue(policies, key::kProxyMode);
  const base::Value* server = GetProxyPolicyValue(policies, key::kProxyServer);
  const base::Value* server_mode =
      GetProxyPolicyValue(policies, key::kProxyServerMode);
  const base::Value* pac_url = GetProxyPolicyValue(policies, key::kProxyPacUrl);
  const base::Value* bypass_list =
      GetProxyPolicyValue(policies, key::kProxyBypassList);

  ProxyPrefs::ProxyMode proxy_mode;
  if (mode) {
    std::string string_mode;
    CHECK(mode->GetAsString(&string_mode));
    CHECK(ProxyPrefs::StringToProxyMode(string_mode, &proxy_mode));
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
        NOTREACHED();
    }
  } else {
    return;
  }

  switch (proxy_mode) {
    case ProxyPrefs::MODE_DIRECT:
      prefs->SetValue(proxy_config::prefs::kProxy,
                      ProxyConfigDictionary::CreateDirect());
      break;
    case ProxyPrefs::MODE_AUTO_DETECT:
      prefs->SetValue(proxy_config::prefs::kProxy,
                      ProxyConfigDictionary::CreateAutoDetect());
      break;
    case ProxyPrefs::MODE_PAC_SCRIPT: {
      std::string pac_url_string;
      if (pac_url && pac_url->GetAsString(&pac_url_string)) {
        prefs->SetValue(
            proxy_config::prefs::kProxy,
            ProxyConfigDictionary::CreatePacScript(pac_url_string, false));
      } else {
        NOTREACHED();
      }
      break;
    }
    case ProxyPrefs::MODE_FIXED_SERVERS: {
      std::string proxy_server;
      std::string bypass_list_string;
      if (server->GetAsString(&proxy_server)) {
        if (bypass_list)
          bypass_list->GetAsString(&bypass_list_string);
        prefs->SetValue(proxy_config::prefs::kProxy,
                        ProxyConfigDictionary::CreateFixedServers(
                            proxy_server, bypass_list_string));
      }
      break;
    }
    case ProxyPrefs::MODE_SYSTEM:
      prefs->SetValue(proxy_config::prefs::kProxy,
                      ProxyConfigDictionary::CreateSystem());
      break;
    case ProxyPrefs::kModeCount:
      NOTREACHED();
  }
}

const base::Value* ProxyPolicyHandler::GetProxyPolicyValue(
    const PolicyMap& policies, const char* policy_name) {
  // See note on the ProxyPolicyHandler implementation above.
  const base::Value* value = policies.GetValue(key::kProxySettings);
  const base::DictionaryValue* settings;
  if (!value || !value->GetAsDictionary(&settings))
    return nullptr;

  const base::Value* policy_value = nullptr;
  std::string tmp;
  if (!settings->Get(policy_name, &policy_value) || policy_value->is_none() ||
      (policy_value->is_string() && policy_value->GetAsString(&tmp) &&
       tmp.empty())) {
    return nullptr;
  }
  return policy_value;
}

bool ProxyPolicyHandler::CheckProxyModeAndServerMode(const PolicyMap& policies,
                                                     PolicyErrorMap* errors,
                                                     std::string* mode_value) {
  const base::Value* mode = GetProxyPolicyValue(policies, key::kProxyMode);
  const base::Value* server = GetProxyPolicyValue(policies, key::kProxyServer);
  const base::Value* server_mode =
      GetProxyPolicyValue(policies, key::kProxyServerMode);
  const base::Value* pac_url = GetProxyPolicyValue(policies, key::kProxyPacUrl);

  // If there's a server mode, convert it into a mode.
  // When both are specified, the mode takes precedence.
  if (mode) {
    if (server_mode) {
      errors->AddError(key::kProxySettings,
                       key::kProxyServerMode,
                       IDS_POLICY_OVERRIDDEN,
                       key::kProxyMode);
    }
    if (!mode->GetAsString(mode_value)) {
      errors->AddError(key::kProxySettings, key::kProxyMode,
                       IDS_POLICY_TYPE_ERROR,
                       base::Value::GetTypeName(base::Value::Type::BOOLEAN));
      return false;
    }

    ProxyPrefs::ProxyMode mode;
    if (!ProxyPrefs::StringToProxyMode(*mode_value, &mode)) {
      errors->AddError(key::kProxySettings,
                       key::kProxyMode,
                       IDS_POLICY_INVALID_PROXY_MODE_ERROR);
      return false;
    }

    if (mode == ProxyPrefs::MODE_PAC_SCRIPT && !pac_url) {
      errors->AddError(key::kProxySettings,
                       key::kProxyPacUrl,
                       IDS_POLICY_NOT_SPECIFIED_ERROR);
      return false;
    }
    if (mode == ProxyPrefs::MODE_FIXED_SERVERS && !server) {
      errors->AddError(key::kProxySettings,
                       key::kProxyServer,
                       IDS_POLICY_NOT_SPECIFIED_ERROR);
      return false;
    }
  } else if (server_mode) {
    if (!server_mode->is_int()) {
      errors->AddError(key::kProxySettings, key::kProxyServerMode,
                       IDS_POLICY_TYPE_ERROR,
                       base::Value::GetTypeName(base::Value::Type::INTEGER));
      return false;
    }

    switch (server_mode->GetInt()) {
      case PROXY_SERVER_MODE:
        *mode_value = ProxyPrefs::kDirectProxyModeName;
        break;
      case PROXY_AUTO_DETECT_PROXY_SERVER_MODE:
        *mode_value = ProxyPrefs::kAutoDetectProxyModeName;
        break;
      case PROXY_MANUALLY_CONFIGURED_PROXY_SERVER_MODE:
        if (server && pac_url) {
          int message_id = IDS_POLICY_PROXY_BOTH_SPECIFIED_ERROR;
          errors->AddError(key::kProxySettings,
                           key::kProxyServer,
                           message_id);
          errors->AddError(key::kProxySettings,
                           key::kProxyPacUrl,
                           message_id);
          return false;
        }
        if (!server && !pac_url) {
          int message_id = IDS_POLICY_PROXY_NEITHER_SPECIFIED_ERROR;
          errors->AddError(key::kProxySettings,
                           key::kProxyServer,
                           message_id);
          errors->AddError(key::kProxySettings,
                           key::kProxyPacUrl,
                           message_id);
          return false;
        }
        *mode_value = pac_url ? ProxyPrefs::kPacScriptProxyModeName
                              : ProxyPrefs::kFixedServersProxyModeName;
        break;
      case PROXY_USE_SYSTEM_PROXY_SERVER_MODE:
        *mode_value = ProxyPrefs::kSystemProxyModeName;
        break;
      default:
        errors->AddError(key::kProxySettings, key::kProxyServerMode,
                         IDS_POLICY_OUT_OF_RANGE_ERROR,
                         base::NumberToString(server_mode->GetInt()));
        return false;
    }
  }
  return true;
}

}  // namespace policy
