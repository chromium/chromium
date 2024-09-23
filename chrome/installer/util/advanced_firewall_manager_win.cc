// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/installer/util/advanced_firewall_manager_win.h"

#include <objbase.h>

#include <stddef.h>

#include "base/logging.h"
#include "base/strings/string_number_conversions_win.h"
#include "base/strings/utf_string_conversions.h"
#include "base/uuid.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_variant.h"

namespace installer {

AdvancedFirewallManager::AdvancedFirewallManager() {}

AdvancedFirewallManager::~AdvancedFirewallManager() {}

bool AdvancedFirewallManager::Init(const std::wstring& app_name,
                                   const base::FilePath& app_path) {
  firewall_rules_ = nullptr;
  HRESULT hr = ::CoCreateInstance(CLSID_NetFwPolicy2, nullptr, CLSCTX_ALL,
                                  IID_PPV_ARGS(&firewall_policy_));
  if (FAILED(hr)) {
    DLOG(ERROR) << logging::SystemErrorCodeToString(hr);
    firewall_policy_ = nullptr;
    return false;
  }
  hr = firewall_policy_->get_Rules(&firewall_rules_);
  if (FAILED(hr)) {
    DLOG(ERROR) << logging::SystemErrorCodeToString(hr);
    firewall_rules_ = nullptr;
    return false;
  }
  app_name_ = app_name;
  app_path_ = app_path;
  return true;
}

bool AdvancedFirewallManager::IsFirewallEnabled() {
  long profile_types = 0;
  HRESULT hr = firewall_policy_->get_CurrentProfileTypes(&profile_types);
  if (FAILED(hr))
    return false;
  // The most-restrictive active profile takes precedence.
  const NET_FW_PROFILE_TYPE2 kProfileTypes[] = {
      NET_FW_PROFILE2_PUBLIC, NET_FW_PROFILE2_PRIVATE, NET_FW_PROFILE2_DOMAIN};
  for (size_t i = 0; i < std::size(kProfileTypes); ++i) {
    if ((profile_types & kProfileTypes[i]) != 0) {
      VARIANT_BOOL enabled = VARIANT_TRUE;
      hr = firewall_policy_->get_FirewallEnabled(kProfileTypes[i], &enabled);
      // Assume the firewall is enabled if we can't determine.
      if (FAILED(hr) || enabled != VARIANT_FALSE)
        return true;
    }
  }
  return false;
}

bool AdvancedFirewallManager::HasAnyRule() {
  std::vector<Microsoft::WRL::ComPtr<INetFwRule>> rules;
  GetAllRules(&rules);
  return !rules.empty();
}

bool AdvancedFirewallManager::AddUDPRule(const std::wstring& rule_name,
                                         const std::wstring& description,
                                         uint16_t port) {
  // Delete the rule. According MDSN |INetFwRules::Add| should replace rule with
  // same "rule identifier". It's not clear what is "rule identifier", but it
  // can successfully create many rule with same name.
  DeleteRuleByName(rule_name);

  // Create the rule and add it to the rule set (only succeeds if elevated).
  Microsoft::WRL::ComPtr<INetFwRule> udp_rule =
      CreateUDPRule(rule_name, description, port);
  if (!udp_rule.Get())
    return false;

  HRESULT hr = firewall_rules_->Add(udp_rule.Get());
  DLOG_IF(ERROR, FAILED(hr)) << logging::SystemErrorCodeToString(hr);
  return SUCCEEDED(hr);
}

void AdvancedFirewallManager::DeleteRuleByName(const std::wstring& rule_name) {
  std::vector<Microsoft::WRL::ComPtr<INetFwRule>> rules;
  GetAllRules(&rules);
  for (size_t i = 0; i < rules.size(); ++i) {
    base::win::ScopedBstr name;
    HRESULT hr = rules[i]->get_Name(name.Receive());
    if (SUCCEEDED(hr) && name.Get() && std::wstring(name.Get()) == rule_name) {
      DeleteRule(rules[i]);
    }
  }
}

void AdvancedFirewallManager::DeleteRule(
    Microsoft::WRL::ComPtr<INetFwRule> rule) {
  // Rename rule to unique name and delete by unique name. We can't just delete
  // rule by name. Multiple rules with the same name and different app are
  // possible.
  base::win::ScopedBstr unique_name(
      base::ASCIIToWide(base::Uuid::GenerateRandomV4().AsLowercaseString()));
  rule->put_Name(unique_name.Get());
  firewall_rules_->Remove(unique_name.Get());
}

void AdvancedFirewallManager::DeleteAllRules() {
  std::vector<Microsoft::WRL::ComPtr<INetFwRule>> rules;
  GetAllRules(&rules);
  for (size_t i = 0; i < rules.size(); ++i) {
    DeleteRule(rules[i]);
  }
}

Microsoft::WRL::ComPtr<INetFwRule> AdvancedFirewallManager::CreateUDPRule(
    const std::wstring& rule_name,
    const std::wstring& description,
    uint16_t port) {
  Microsoft::WRL::ComPtr<INetFwRule> udp_rule;

  HRESULT hr = ::CoCreateInstance(CLSID_NetFwRule, nullptr, CLSCTX_ALL,
                                  IID_PPV_ARGS(&udp_rule));
  if (FAILED(hr)) {
    DLOG(ERROR) << logging::SystemErrorCodeToString(hr);
    return Microsoft::WRL::ComPtr<INetFwRule>();
  }

  udp_rule->put_Name(base::win::ScopedBstr(rule_name).Get());
  udp_rule->put_Description(base::win::ScopedBstr(description).Get());
  udp_rule->put_ApplicationName(base::win::ScopedBstr(app_path_.value()).Get());
  udp_rule->put_Protocol(NET_FW_IP_PROTOCOL_UDP);
  udp_rule->put_Direction(NET_FW_RULE_DIR_IN);
  udp_rule->put_Enabled(VARIANT_TRUE);
  udp_rule->put_LocalPorts(
      base::win::ScopedBstr(base::NumberToWString(port)).Get());
  udp_rule->put_Grouping(base::win::ScopedBstr(app_name_).Get());
  udp_rule->put_Profiles(NET_FW_PROFILE2_ALL);
  udp_rule->put_Action(NET_FW_ACTION_ALLOW);

  return udp_rule;
}

void AdvancedFirewallManager::GetAllRules(
    std::vector<Microsoft::WRL::ComPtr<INetFwRule>>* rules) {
  Microsoft::WRL::ComPtr<IUnknown> rules_enum_unknown;
  HRESULT hr = firewall_rules_->get__NewEnum(&rules_enum_unknown);
  if (FAILED(hr)) {
    DLOG(ERROR) << logging::SystemErrorCodeToString(hr);
    return;
  }

  Microsoft::WRL::ComPtr<IEnumVARIANT> rules_enum;
  hr = rules_enum_unknown.As(&rules_enum);
  if (FAILED(hr)) {
    DLOG(ERROR) << logging::SystemErrorCodeToString(hr);
    return;
  }

  for (;;) {
    base::win::ScopedVariant rule_var;
    hr = rules_enum->Next(1, rule_var.Receive(), nullptr);
    DLOG_IF(ERROR, FAILED(hr)) << logging::SystemErrorCodeToString(hr);
    if (hr != S_OK)
      break;
    DCHECK_EQ(VT_DISPATCH, rule_var.type());
    if (VT_DISPATCH != rule_var.type()) {
      DLOG(ERROR) << "Unexpected type";
      continue;
    }
    Microsoft::WRL::ComPtr<INetFwRule> rule;
    hr = V_DISPATCH(rule_var.ptr())->QueryInterface(IID_PPV_ARGS(&rule));
    if (FAILED(hr)) {
      DLOG(ERROR) << logging::SystemErrorCodeToString(hr);
      continue;
    }

    base::win::ScopedBstr path;
    hr = rule->get_ApplicationName(path.Receive());
    if (FAILED(hr)) {
      DLOG(ERROR) << logging::SystemErrorCodeToString(hr);
      continue;
    }

    if (!path.Get() || !base::FilePath::CompareEqualIgnoreCase(
                           path.Get(), app_path_.value())) {
      continue;
    }

    rules->push_back(rule);
  }
}

}  // namespace installer
