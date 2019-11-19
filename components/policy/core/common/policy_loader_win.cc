// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_loader_win.h"

#include <lm.h>       // For NetGetJoinInformation
// <security.h> needs this.
#define SECURITY_WIN32 1
#include <security.h>  // For GetUserNameEx()
#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/enterprise_util.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/scoped_native_library.h"
#include "base/sequenced_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "base/win/shlwapi.h"  // For PathIsUNC()
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_load_status.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/registry_dict.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/policy_constants.h"

namespace policy {

namespace {

const char kKeyMandatory[] = "policy";
const char kKeyRecommended[] = "recommended";
const char kKeyThirdParty[] = "3rdparty";

// The web store url that is the only trusted source for extensions.
const char kExpectedWebStoreUrl[] =
    ";https://clients2.google.com/service/update2/crx";
// String to be prepended to each blocked entry.
const char kBlockedExtensionPrefix[] = "[BLOCKED]";

// List of policies that are considered only if the user is part of a AD domain.
// Please document any new additions in policy_templates.json!
const char* kInsecurePolicies[] = {
    key::kChromeCleanupEnabled,
    key::kChromeCleanupReportingEnabled,
    key::kCommandLineFlagSecurityWarningsEnabled,
    key::kDefaultSearchProviderEnabled,
    key::kHomepageIsNewTabPage,
    key::kHomepageLocation,
    key::kMetricsReportingEnabled,
    key::kNewTabPageLocation,
    key::kPasswordProtectionChangePasswordURL,
    key::kPasswordProtectionLoginURLs,
    key::kRestoreOnStartup,
    key::kRestoreOnStartupURLs,
    key::kSafeBrowsingForTrustedSourcesEnabled,
    key::kSafeBrowsingEnabled,
    key::kSafeBrowsingWhitelistDomains,
};

// The list of possible errors that can occur while collecting information about
// the current enterprise environment.
// This enum is used to define the buckets for an enumerated UMA histogram.
// Hence,
//   (a) existing enumerated constants should never be deleted or reordered, and
//   (b) new constants should only be appended at the end of the enumeration.
enum DomainCheckErrors {
  // The check error below is no longer possible.
  DEPRECATED_DOMAIN_CHECK_ERROR_GET_JOIN_INFO = 0,
  DOMAIN_CHECK_ERROR_DS_BIND = 1,
  DOMAIN_CHECK_ERROR_SIZE,  // Not a DomainCheckError.  Must be last.
};

// Encapculates logic to determine if enterprise policies should be honored.
// This is used in various places below.
bool ShouldHonorPolicies() {
  bool is_enterprise_version =
      base::win::OSInfo::GetInstance()->version_type() != base::win::SUITE_HOME;
  return base::win::IsEnrolledToDomain() ||
         (base::win::IsDeviceRegisteredWithManagement() &&
          is_enterprise_version);
}

// Verifies that untrusted policies contain only safe values. Modifies the
// |policy| in place.
void FilterUntrustedPolicy(PolicyMap* policy) {
  if (ShouldHonorPolicies())
    return;

  int invalid_policies = 0;
  const PolicyMap::Entry* map_entry =
      policy->Get(key::kExtensionInstallForcelist);
  if (map_entry && map_entry->value) {
    const base::ListValue* policy_list_value = nullptr;
    if (!map_entry->value->GetAsList(&policy_list_value))
      return;

    std::unique_ptr<base::ListValue> filtered_values(new base::ListValue);
    for (const auto& list_entry : *policy_list_value) {
      std::string entry;
      if (!list_entry.GetAsString(&entry))
        continue;
      size_t pos = entry.find(';');
      if (pos == std::string::npos)
        continue;
      // Only allow custom update urls in enterprise environments.
      if (!base::LowerCaseEqualsASCII(entry.substr(pos),
                                      kExpectedWebStoreUrl)) {
        entry = kBlockedExtensionPrefix + entry;
        invalid_policies++;
      }

      filtered_values->AppendString(entry);
    }
    if (invalid_policies) {
      PolicyMap::Entry filtered_entry = map_entry->DeepCopy();
      filtered_entry.value = std::move(filtered_values);
      policy->Set(key::kExtensionInstallForcelist, std::move(filtered_entry));

      const PolicyDetails* details =
          GetChromePolicyDetails(key::kExtensionInstallForcelist);
      base::UmaHistogramSparse("EnterpriseCheck.InvalidPolicies", details->id);
    }
  }

  for (size_t i = 0; i < base::size(kInsecurePolicies); ++i) {
    if (policy->Get(kInsecurePolicies[i])) {
      policy->GetMutable(kInsecurePolicies[i])->SetBlocked();
      invalid_policies++;
      const PolicyDetails* details =
          GetChromePolicyDetails(kInsecurePolicies[i]);
      base::UmaHistogramSparse("EnterpriseCheck.InvalidPolicies", details->id);
    }
  }

  UMA_HISTOGRAM_COUNTS_1M("EnterpriseCheck.InvalidPoliciesDetected",
                          invalid_policies);
}

// Parses |gpo_dict| according to |schema| and writes the resulting policy
// settings to |policy| for the given |scope| and |level|.
void ParsePolicy(const RegistryDict* gpo_dict,
                 PolicyLevel level,
                 PolicyScope scope,
                 const Schema& schema,
                 PolicyMap* policy) {
  if (!gpo_dict)
    return;

  std::unique_ptr<base::Value> policy_value(gpo_dict->ConvertToJSON(schema));
  const base::DictionaryValue* policy_dict = nullptr;
  if (!policy_value->GetAsDictionary(&policy_dict) || !policy_dict) {
    LOG(WARNING) << "Root policy object is not a dictionary!";
    return;
  }

  policy->LoadFrom(policy_dict, level, scope, POLICY_SOURCE_PLATFORM);
}

// Returns a name, using the |get_name| callback, which may refuse the call if
// the name is longer than _MAX_PATH. So this helper function takes care of the
// retry with the required size.
bool GetName(const base::Callback<BOOL(LPWSTR, LPDWORD)>& get_name,
             base::string16* name) {
  DCHECK(name);
  DWORD size = _MAX_PATH;
  if (!get_name.Run(base::WriteInto(name, size), &size)) {
    if (::GetLastError() != ERROR_MORE_DATA)
      return false;
    // Try again with the required size. This time it must work, the size should
    // not have changed in between the two calls.
    if (!get_name.Run(base::WriteInto(name, size), &size))
      return false;
  }
  return true;
}

// To convert the weird BOOLEAN return value type of ::GetUserNameEx().
BOOL GetUserNameExBool(EXTENDED_NAME_FORMAT format, LPWSTR name, PULONG size) {
  // ::GetUserNameEx is documented to return a nonzero value on success.
  return ::GetUserNameEx(format, name, size) != 0;
}

// Make sure to use the real NetGetJoinInformation, otherwise fallback to the
// linked one.
bool IsDomainJoined() {
  base::ScopedClosureRunner free_library;
  decltype(&::NetGetJoinInformation) net_get_join_information_function =
      &::NetGetJoinInformation;
  decltype(&::NetApiBufferFree) net_api_buffer_free_function =
      &::NetApiBufferFree;
  bool got_function_addresses = false;
  // Use an absolute path to load the DLL to avoid DLL preloading attacks.
  base::FilePath path;
  if (base::PathService::Get(base::DIR_SYSTEM, &path)) {
    HINSTANCE net_api_library = ::LoadLibraryEx(
        path.Append(FILE_PATH_LITERAL("netapi32.dll")).value().c_str(), nullptr,
        LOAD_WITH_ALTERED_SEARCH_PATH);
    if (net_api_library) {
      free_library.ReplaceClosure(
          base::BindOnce(base::IgnoreResult(&::FreeLibrary), net_api_library));
      net_get_join_information_function =
          reinterpret_cast<decltype(&::NetGetJoinInformation)>(
              ::GetProcAddress(net_api_library, "NetGetJoinInformation"));
      net_api_buffer_free_function =
          reinterpret_cast<decltype(&::NetApiBufferFree)>(
              ::GetProcAddress(net_api_library, "NetApiBufferFree"));

      if (net_get_join_information_function && net_api_buffer_free_function) {
        got_function_addresses = true;
      } else {
        net_get_join_information_function = &::NetGetJoinInformation;
        net_api_buffer_free_function = &::NetApiBufferFree;
      }
    }
  }
  base::UmaHistogramBoolean("EnterpriseCheck.NetGetJoinInformationAddress",
                            got_function_addresses);

  LPWSTR buffer = nullptr;
  NETSETUP_JOIN_STATUS buffer_type = NetSetupUnknownStatus;
  bool is_joined = net_get_join_information_function(
                       nullptr, &buffer, &buffer_type) == NERR_Success &&
                   buffer_type == NetSetupDomainName;
  if (buffer)
    net_api_buffer_free_function(buffer);

  return is_joined;
}

// Collects stats about the enterprise environment that can be used to decide
// how to parse the existing policy information.
void CollectEnterpriseUMAs() {
  // Collect statistics about the windows suite.
  UMA_HISTOGRAM_ENUMERATION("EnterpriseCheck.OSType",
                            base::win::OSInfo::GetInstance()->version_type(),
                            base::win::SUITE_LAST);

  base::UmaHistogramBoolean("EnterpriseCheck.IsDomainJoined", IsDomainJoined());
  base::UmaHistogramBoolean("EnterpriseCheck.InDomain",
                            base::win::IsEnrolledToDomain());
  base::UmaHistogramBoolean("EnterpriseCheck.IsManaged",
                            base::win::IsDeviceRegisteredWithManagement());
  base::UmaHistogramBoolean("EnterpriseCheck.IsEnterpriseUser",
                            base::IsMachineExternallyManaged());

  base::string16 machine_name;
  if (GetName(base::Bind(&::GetComputerNameEx, ::ComputerNameDnsHostname),
              &machine_name)) {
    base::string16 user_name;
    if (GetName(base::Bind(&GetUserNameExBool, ::NameSamCompatible),
                &user_name)) {
      // A local user has the machine name in its sam compatible name, e.g.,
      // 'MACHINE_NAME\username', otherwise it is perfixed with the domain name
      // as opposed to the machine, e.g., 'COMPANY\username'.
      base::UmaHistogramBoolean(
          "EnterpriseCheck.IsLocalUser",
          base::StartsWith(user_name, machine_name,
                           base::CompareCase::INSENSITIVE_ASCII) &&
              user_name[machine_name.size()] == L'\\');
    }

    base::string16 full_machine_name;
    if (GetName(
            base::Bind(&::GetComputerNameEx, ::ComputerNameDnsFullyQualified),
            &full_machine_name)) {
      // ComputerNameDnsFullyQualified is the same as the
      // ComputerNameDnsHostname when not domain joined, otherwise it has a
      // suffix.
      base::UmaHistogramBoolean(
          "EnterpriseCheck.IsLocalMachine",
          base::EqualsCaseInsensitiveASCII(machine_name, full_machine_name));
    }
  }
}

}  // namespace

PolicyLoaderWin::PolicyLoaderWin(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    const base::string16& chrome_policy_key)
    : AsyncPolicyLoader(task_runner),
      is_initialized_(false),
      chrome_policy_key_(chrome_policy_key),
      user_policy_changed_event_(
          base::WaitableEvent::ResetPolicy::AUTOMATIC,
          base::WaitableEvent::InitialState::NOT_SIGNALED),
      machine_policy_changed_event_(
          base::WaitableEvent::ResetPolicy::AUTOMATIC,
          base::WaitableEvent::InitialState::NOT_SIGNALED),
      user_policy_watcher_failed_(false),
      machine_policy_watcher_failed_(false) {
  if (!::RegisterGPNotification(user_policy_changed_event_.handle(), false)) {
    DPLOG(WARNING) << "Failed to register user group policy notification";
    user_policy_watcher_failed_ = true;
  }
  if (!::RegisterGPNotification(machine_policy_changed_event_.handle(), true)) {
    DPLOG(WARNING) << "Failed to register machine group policy notification.";
    machine_policy_watcher_failed_ = true;
  }
}

PolicyLoaderWin::~PolicyLoaderWin() {
  if (!user_policy_watcher_failed_) {
    ::UnregisterGPNotification(user_policy_changed_event_.handle());
    user_policy_watcher_.StopWatching();
  }
  if (!machine_policy_watcher_failed_) {
    ::UnregisterGPNotification(machine_policy_changed_event_.handle());
    machine_policy_watcher_.StopWatching();
  }
}

// static
std::unique_ptr<PolicyLoaderWin> PolicyLoaderWin::Create(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    const base::string16& chrome_policy_key) {
  return std::make_unique<PolicyLoaderWin>(task_runner, chrome_policy_key);
}

void PolicyLoaderWin::InitOnBackgroundThread() {
  is_initialized_ = true;
  SetupWatches();
  CollectEnterpriseUMAs();
}

std::unique_ptr<PolicyBundle> PolicyLoaderWin::Load() {
  // Reset the watches BEFORE reading the individual policies to avoid
  // missing a change notification.
  if (is_initialized_)
    SetupWatches();

  // Policy scope and corresponding hive.
  static const struct {
    PolicyScope scope;
    HKEY hive;
  } kScopes[] = {
      {POLICY_SCOPE_MACHINE, HKEY_LOCAL_MACHINE},
      {POLICY_SCOPE_USER, HKEY_CURRENT_USER},
  };

  // Load policy data for the different scopes/levels and merge them.
  std::unique_ptr<PolicyBundle> bundle(new PolicyBundle());
  PolicyMap* chrome_policy =
      &bundle->Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()));
  for (size_t i = 0; i < base::size(kScopes); ++i) {
    PolicyScope scope = kScopes[i].scope;
    PolicyLoadStatusUmaReporter status;
    RegistryDict gpo_dict;

    gpo_dict.ReadRegistry(kScopes[i].hive, chrome_policy_key_);

    // Remove special-cased entries from the GPO dictionary.
    std::unique_ptr<RegistryDict> recommended_dict(
        gpo_dict.RemoveKey(kKeyRecommended));
    std::unique_ptr<RegistryDict> third_party_dict(
        gpo_dict.RemoveKey(kKeyThirdParty));

    // Load Chrome policy.
    LoadChromePolicy(&gpo_dict, POLICY_LEVEL_MANDATORY, scope, chrome_policy);
    LoadChromePolicy(recommended_dict.get(), POLICY_LEVEL_RECOMMENDED, scope,
                     chrome_policy);

    // Load 3rd-party policy.
    if (third_party_dict)
      Load3rdPartyPolicy(third_party_dict.get(), scope, bundle.get());
  }

  return bundle;
}

void PolicyLoaderWin::LoadChromePolicy(const RegistryDict* gpo_dict,
                                       PolicyLevel level,
                                       PolicyScope scope,
                                       PolicyMap* chrome_policy_map) {
  PolicyMap policy;
  const Schema* chrome_schema =
      schema_map()->GetSchema(PolicyNamespace(POLICY_DOMAIN_CHROME, ""));
  ParsePolicy(gpo_dict, level, scope, *chrome_schema, &policy);
  FilterUntrustedPolicy(&policy);
  chrome_policy_map->MergeFrom(policy);
}

void PolicyLoaderWin::Load3rdPartyPolicy(const RegistryDict* gpo_dict,
                                         PolicyScope scope,
                                         PolicyBundle* bundle) {
  // Map of known 3rd party policy domain name to their enum values.
  static const struct {
    const char* name;
    PolicyDomain domain;
  } k3rdPartyDomains[] = {
      {"extensions", POLICY_DOMAIN_EXTENSIONS},
  };

  // Policy level and corresponding path.
  static const struct {
    PolicyLevel level;
    const char* path;
  } kLevels[] = {
      {POLICY_LEVEL_MANDATORY, kKeyMandatory},
      {POLICY_LEVEL_RECOMMENDED, kKeyRecommended},
  };

  for (size_t i = 0; i < base::size(k3rdPartyDomains); i++) {
    const char* name = k3rdPartyDomains[i].name;
    const PolicyDomain domain = k3rdPartyDomains[i].domain;
    const RegistryDict* domain_dict = gpo_dict->GetKey(name);
    if (!domain_dict)
      continue;

    for (RegistryDict::KeyMap::const_iterator component(
             domain_dict->keys().begin());
         component != domain_dict->keys().end(); ++component) {
      const PolicyNamespace policy_namespace(domain, component->first);

      const Schema* schema_from_map = schema_map()->GetSchema(policy_namespace);
      if (!schema_from_map) {
        // This extension isn't installed or doesn't support policies.
        continue;
      }
      Schema schema = *schema_from_map;

      // Parse policy.
      for (size_t j = 0; j < base::size(kLevels); j++) {
        const RegistryDict* policy_dict =
            component->second->GetKey(kLevels[j].path);
        if (!policy_dict)
          continue;

        PolicyMap policy;
        ParsePolicy(policy_dict, kLevels[j].level, scope, schema, &policy);
        bundle->Get(policy_namespace).MergeFrom(policy);
      }
    }
  }
}

void PolicyLoaderWin::SetupWatches() {
  DCHECK(is_initialized_);
  if (!user_policy_watcher_failed_ &&
      !user_policy_watcher_.GetWatchedObject() &&
      !user_policy_watcher_.StartWatchingOnce(
          user_policy_changed_event_.handle(), this)) {
    DLOG(WARNING) << "Failed to start watch for user policy change event";
    user_policy_watcher_failed_ = true;
  }
  if (!machine_policy_watcher_failed_ &&
      !machine_policy_watcher_.GetWatchedObject() &&
      !machine_policy_watcher_.StartWatchingOnce(
          machine_policy_changed_event_.handle(), this)) {
    DLOG(WARNING) << "Failed to start watch for machine policy change event";
    machine_policy_watcher_failed_ = true;
  }
}

void PolicyLoaderWin::OnObjectSignaled(HANDLE object) {
  DCHECK(object == user_policy_changed_event_.handle() ||
         object == machine_policy_changed_event_.handle())
      << "unexpected object signaled policy reload, obj = " << std::showbase
      << std::hex << object;
  Reload(false);
}

}  // namespace policy
