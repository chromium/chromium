// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_loader_win.h"

// Must be included before lm.h
#include <windows.h>

#include <lm.h>       // For NetGetJoinInformation
// <security.h> needs this.
#define SECURITY_WIN32 1
#include <security.h>  // For GetUserNameEx()
#include <stddef.h>
#include <userenv.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/enterprise_util.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/scoped_native_library.h"
#include "base/strings/string_util.h"
#include "base/syslog_logging.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/scoped_thread_priority.h"
#include "base/values.h"
#include "base/win/shlwapi.h"  // For PathIsUNC()
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "components/policy/core/common/async_policy_loader.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_loader_common.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/registry_dict.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/core/common/scoped_critical_policy_section.h"
#include "components/policy/policy_constants.h"

namespace policy {

namespace {

// Logged to UMA - keep in sync with enums.xml.
enum WindowsProfileType {
  kApiFailure,
  kInvalid,
  kNone,
  kMandatory,
  kRoaming,
  kRoamingPreExisting,
  kTemporary,
  kMaxValue = kTemporary
};

const char kKeyMandatory[] = "policy";
const char kKeyRecommended[] = "recommended";
const char kKeyThirdParty[] = "3rdparty";

// Parses |gpo_dict| according to |schema| and writes the resulting policy
// settings to |policy| for the given |scope| and |level|.
void ParsePolicy(const RegistryDict* gpo_dict,
                 PolicyLevel level,
                 PolicyScope scope,
                 const Schema& schema,
                 PolicyMap* policy) {
  if (!gpo_dict)
    return;

  std::optional<base::Value> policy_value(gpo_dict->ConvertToJSON(schema));
  DCHECK(policy_value);
  const base::Value::Dict* policy_dict = policy_value->GetIfDict();
  if (!policy_dict) {
    SYSLOG(WARNING) << "Root policy object is not a dictionary!";
    return;
  }

  policy->LoadFrom(*policy_dict, level, scope, POLICY_SOURCE_PLATFORM);
}

// Returns a name, using the |get_name| callback, which may refuse the call if
// the name is longer than _MAX_PATH. So this helper function takes care of the
// retry with the required size.
bool GetName(const base::RepeatingCallback<BOOL(LPWSTR, LPDWORD)>& get_name,
             std::wstring* name) {
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
  // Mitigate the issues caused by loading DLLs on a background thread
  // (http://crbug/973868).
  SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY_REPEATEDLY();
  base::ScopedClosureRunner free_library;
  decltype(&::NetGetJoinInformation) net_get_join_information_function =
      &::NetGetJoinInformation;
  decltype(&::NetApiBufferFree) net_api_buffer_free_function =
      &::NetApiBufferFree;
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

      if (!net_get_join_information_function || !net_api_buffer_free_function) {
        net_get_join_information_function = &::NetGetJoinInformation;
        net_api_buffer_free_function = &::NetApiBufferFree;
      }
    }
  }

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

  base::UmaHistogramBoolean("EnterpriseCheck.IsManagedOrEnterpriseDevice",
                            base::IsManagedOrEnterpriseDevice());
  base::UmaHistogramBoolean("EnterpriseCheck.IsDomainJoined", IsDomainJoined());
  base::UmaHistogramBoolean("EnterpriseCheck.InDomain",
                            base::win::IsEnrolledToDomain());
  base::UmaHistogramBoolean("EnterpriseCheck.IsManaged2",
                            base::win::IsDeviceRegisteredWithManagement());
  base::UmaHistogramBoolean("EnterpriseCheck.IsEnterpriseUser",
                            base::IsEnterpriseDevice());
  base::UmaHistogramBoolean("EnterpriseCheck.IsJoinedToAzureAD",
                            base::win::IsJoinedToAzureAD());

  {
    WindowsProfileType profile_type = kApiFailure;
    DWORD flags = 0;
    // Although this API takes 'flags' that's shaped like a bitfield, the type
    // returned can only be one of the PT_* values below.
    if (::GetProfileType(&flags)) {
      switch (flags) {
        case 0:
          profile_type = kNone;
          break;
        case PT_MANDATORY:
          profile_type = kMandatory;
          break;
        case PT_ROAMING:
          profile_type = kRoaming;
          break;
        case PT_ROAMING_PREEXISTING:
          profile_type = kRoamingPreExisting;
          break;
        case PT_TEMPORARY:
          profile_type = kTemporary;
          break;
        default:
          profile_type = kInvalid;
          break;
      }
    }
    base::UmaHistogramEnumeration("EnterpriseCheck.WindowsProfileType",
                                  profile_type);
  }

  std::wstring machine_name;
  if (GetName(
          base::BindRepeating(&::GetComputerNameEx, ::ComputerNameDnsHostname),
          &machine_name)) {
    std::wstring user_name;
    if (GetName(base::BindRepeating(&GetUserNameExBool, ::NameSamCompatible),
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

    std::wstring full_machine_name;
    if (GetName(base::BindRepeating(&::GetComputerNameEx,
                                    ::ComputerNameDnsFullyQualified),
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
    ManagementService* management_service,
    const std::wstring& chrome_policy_key)
    : AsyncPolicyLoader(task_runner,
                        management_service,
                        /*periodic_updates=*/true),
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
    ManagementService* management_service,
    const std::wstring& chrome_policy_key) {
  return std::make_unique<PolicyLoaderWin>(task_runner, management_service,
                                           chrome_policy_key);
}

void PolicyLoaderWin::InitOnBackgroundThread() {
  is_initialized_ = true;
  SetupWatches();
  CollectEnterpriseUMAs();
}

PolicyBundle PolicyLoaderWin::Load() {
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
  PolicyBundle bundle;
  PolicyMap* chrome_policy =
      &bundle.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()));
  for (const auto& entry : kScopes) {
    PolicyScope scope = entry.scope;
    RegistryDict gpo_dict;

    gpo_dict.ReadRegistry(entry.hive, chrome_policy_key_);

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
      Load3rdPartyPolicy(third_party_dict.get(), scope, &bundle);
  }

  return bundle;
}

void PolicyLoaderWin::Reload(bool force) {
  // If we need to get management bit first, no need to enter the critical
  // section as we won't actual read the policy.
  if (NeedManagementBitBeforeLoad()) {
    AsyncPolicyLoader::Reload(force);
    return;
  }

  ScopedCriticalPolicySection::Enter(
      base::BindOnce(&PolicyLoaderWin::OnSectionEntered,
                     weak_factory_.GetWeakPtr(), force),
      task_runner());
}

void PolicyLoaderWin::OnSectionEntered(bool force) {
  AsyncPolicyLoader::Reload(force);
}

void PolicyLoaderWin::LoadChromePolicy(const RegistryDict* gpo_dict,
                                       PolicyLevel level,
                                       PolicyScope scope,
                                       PolicyMap* chrome_policy_map) {
  PolicyMap policy;
  const Schema* chrome_schema =
      schema_map()->GetSchema(PolicyNamespace(POLICY_DOMAIN_CHROME, ""));
  ParsePolicy(gpo_dict, level, scope, *chrome_schema, &policy);
  if (ShouldFilterSensitivePolicies())
    FilterSensitivePolicies(&policy);
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

  for (const auto& entry : k3rdPartyDomains) {
    const char* name = entry.name;
    const PolicyDomain domain = entry.domain;
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
      for (const auto& level : kLevels) {
        const RegistryDict* policy_dict = component->second->GetKey(level.path);
        if (!policy_dict)
          continue;

        PolicyMap policy;
        ParsePolicy(policy_dict, level.level, scope, schema, &policy);
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
