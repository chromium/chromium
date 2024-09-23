// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/conflicts/incompatible_applications_updater.h"

#include <string>
#include <utility>

#include "base/base_paths.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/not_fatal_until.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "base/win/registry.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/win/conflicts/module_database.h"
#include "chrome/browser/win/conflicts/module_info.h"
#include "chrome/browser/win/conflicts/module_info_util.h"
#include "chrome/browser/win/conflicts/module_list_filter.h"
#include "chrome/browser/win/conflicts/third_party_metrics_recorder.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace {

// Serializes a vector of IncompatibleApplications to JSON.
base::Value::Dict ConvertToDictionary(
    const std::vector<IncompatibleApplicationsUpdater::IncompatibleApplication>&
        applications) {
  base::Value::Dict result;

  for (const auto& application : applications) {
    base::Value::Dict element;

    // The registry location is necessary to quickly figure out if that
    // application is still installed on the computer.
    element.Set("registry_is_hkcu",
                application.info.registry_root == HKEY_CURRENT_USER);
    element.Set("registry_key_path",
                base::WideToUTF8(application.info.registry_key_path));
    element.Set("registry_wow64_access",
                static_cast<int>(application.info.registry_wow64_access));

    // And then the actual information needed to display a warning to the user.
    element.Set("allow_load", application.blocklist_action->allow_load());
    element.Set("type", application.blocklist_action->message_type());
    element.Set("message_url", application.blocklist_action->message_url());

    result.Set(base::WideToUTF8(application.info.name), std::move(element));
  }

  return result;
}

// Deserializes a IncompatibleApplication named |name| from |value|. Returns
// null if |value| is not a dict containing all required fields.
std::unique_ptr<IncompatibleApplicationsUpdater::IncompatibleApplication>
ConvertToIncompatibleApplication(const std::string& name,
                                 const base::Value& value) {
  if (!value.is_dict())
    return nullptr;

  const base::Value::Dict& dict = value.GetDict();
  std::optional<bool> registry_is_hkcu = dict.FindBool("registry_is_hkcu");
  const std::string* registry_key_path = dict.FindString("registry_key_path");
  std::optional<int> registry_wow64_access =
      dict.FindInt("registry_wow64_access");
  std::optional<bool> allow_load = dict.FindBool("allow_load");
  std::optional<int> type = dict.FindInt("type");
  const std::string* message_url = dict.FindString("message_url");

  // All of the above are required for a valid application.
  if (!registry_is_hkcu || !registry_key_path || !registry_wow64_access ||
      !allow_load || !type || !message_url) {
    return nullptr;
  }

  InstalledApplications::ApplicationInfo application_info = {
      base::UTF8ToWide(name),
      *registry_is_hkcu ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE,
      base::UTF8ToWide(*registry_key_path),
      static_cast<REGSAM>(*registry_wow64_access)};

  auto blocklist_action =
      std::make_unique<chrome::conflicts::BlocklistAction>();
  blocklist_action->set_allow_load(*allow_load);
  blocklist_action->set_message_type(
      static_cast<chrome::conflicts::BlocklistMessageType>(*type));
  blocklist_action->set_message_url(*message_url);

  return std::make_unique<
      IncompatibleApplicationsUpdater::IncompatibleApplication>(
      std::move(application_info), std::move(blocklist_action));
}

// Returns true if |application| references an existing application in the
// registry.
//
// Used to filter out stale applications from the cache. This can happen if a
// application was uninstalled between the time it was found and Chrome was
// relaunched.
bool IsValidApplication(
    const IncompatibleApplicationsUpdater::IncompatibleApplication&
        application) {
  return base::win::RegKey(
             application.info.registry_root,
             application.info.registry_key_path.c_str(),
             KEY_QUERY_VALUE | application.info.registry_wow64_access)
      .Valid();
}

// Clears the cache of all the applications whose name is in
// |state_application_names|.
void RemoveStaleApplications(
    const std::vector<std::string>& stale_application_names) {
  // Early exit because ScopedDictPrefUpdate will write to the pref even if it
  // doesn't contain a value.
  if (stale_application_names.empty())
    return;

  ScopedDictPrefUpdate update(g_browser_process->local_state(),
                              prefs::kIncompatibleApplications);
  base::Value::Dict& existing_applications = update.Get();

  for (const auto& application_name : stale_application_names) {
    bool removed = existing_applications.Remove(application_name);
    DCHECK(removed);
  }
}

// Applies the given |function| object to each valid IncompatibleApplication
// found in the kIncompatibleApplications preference.
//
// The signature of the function must be equivalent to the following:
//   bool Function(std::unique_ptr<IncompatibleApplication> application));
//
// The return value of |function| indicates if the enumeration should continue
// (true) or be stopped (false).
//
// This function takes care of removing invalid entries that are found during
// the enumeration.
template <class UnaryFunction>
void EnumerateAndTrimIncompatibleApplications(UnaryFunction function) {
  std::vector<std::string> stale_application_names;
  for (const auto item : g_browser_process->local_state()
                             ->FindPreference(prefs::kIncompatibleApplications)
                             ->GetValue()
                             ->GetDict()) {
    auto application =
        ConvertToIncompatibleApplication(item.first, item.second);

    if (!application || !IsValidApplication(*application)) {
      // Mark every invalid application as stale so they are removed from the
      // cache.
      stale_application_names.push_back(item.first);
      continue;
    }

    // Notify the caller and stop the enumeration if requested by the function.
    if (!function(std::move(application)))
      break;
  }

  RemoveStaleApplications(stale_application_names);
}

// Updates the kIncompatibleApplications pref with those contained in
// |incompatible_applications|.
void UpdateIncompatibleApplications(
    bool should_clear_pref,
    std::vector<IncompatibleApplicationsUpdater::IncompatibleApplication>
        incompatible_applications) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Clear pref if requested.
  if (should_clear_pref) {
    g_browser_process->local_state()->ClearPref(
        prefs::kIncompatibleApplications);
  }

  // If there is no new incompatible application, there is nothing to do.
  if (incompatible_applications.empty())
    return;

  // The conversion of the accumulated applications to a json dictionary takes
  // care of eliminating duplicates.
  base::Value::Dict new_applications =
      ConvertToDictionary(incompatible_applications);

  // Update the existing dictionary.
  ScopedDictPrefUpdate update(g_browser_process->local_state(),
                              prefs::kIncompatibleApplications);
  base::Value::Dict& existing_applications = update.Get();
  for (auto&& element : new_applications) {
    existing_applications.Set(std::move(element.first),
                              std::move(element.second));
  }
}

}  // namespace

// -----------------------------------------------------------------------------
// IncompatibleApplication

IncompatibleApplicationsUpdater::IncompatibleApplication::
    IncompatibleApplication(
        InstalledApplications::ApplicationInfo info,
        std::unique_ptr<chrome::conflicts::BlocklistAction> blocklist_action)
    : info(std::move(info)), blocklist_action(std::move(blocklist_action)) {}

IncompatibleApplicationsUpdater::IncompatibleApplication::
    ~IncompatibleApplication() = default;

IncompatibleApplicationsUpdater::IncompatibleApplication::
    IncompatibleApplication(
        IncompatibleApplication&& incompatible_application) = default;

IncompatibleApplicationsUpdater::IncompatibleApplication&
IncompatibleApplicationsUpdater::IncompatibleApplication::operator=(
    IncompatibleApplication&& incompatible_application) = default;

// -----------------------------------------------------------------------------
// IncompatibleApplicationsUpdater

IncompatibleApplicationsUpdater::IncompatibleApplicationsUpdater(
    ModuleDatabaseEventSource* module_database_event_source,
    const CertificateInfo& exe_certificate_info,
    scoped_refptr<ModuleListFilter> module_list_filter,
    const InstalledApplications& installed_applications,
    bool module_analysis_disabled)
    : module_database_event_source_(module_database_event_source),
      exe_certificate_info_(exe_certificate_info),
      module_list_filter_(std::move(module_list_filter)),
      installed_applications_(installed_applications),
      module_analysis_disabled_(module_analysis_disabled) {
  module_database_event_source_->AddObserver(this);
}

IncompatibleApplicationsUpdater::~IncompatibleApplicationsUpdater() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  module_database_event_source_->RemoveObserver(this);
}

// static
void IncompatibleApplicationsUpdater::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kIncompatibleApplications);
}

// static
bool IncompatibleApplicationsUpdater::IsWarningEnabled() {
  return base::FeatureList::IsEnabled(
      features::kIncompatibleApplicationsWarning);
}

// static
bool IncompatibleApplicationsUpdater::HasCachedApplications() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!ModuleDatabase::IsThirdPartyBlockingPolicyEnabled() ||
      !IsWarningEnabled()) {
    return false;
  }

  bool found_valid_application = false;

  EnumerateAndTrimIncompatibleApplications(
      [&found_valid_application](
          std::unique_ptr<IncompatibleApplication> application) {
        found_valid_application = true;

        // Break the enumeration.
        return false;
      });

  return found_valid_application;
}

// static
std::vector<IncompatibleApplicationsUpdater::IncompatibleApplication>
IncompatibleApplicationsUpdater::GetCachedApplications() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(ModuleDatabase::IsThirdPartyBlockingPolicyEnabled());
  DCHECK(IsWarningEnabled());

  std::vector<IncompatibleApplication> valid_applications;

  EnumerateAndTrimIncompatibleApplications(
      [&valid_applications](
          std::unique_ptr<IncompatibleApplication> application) {
        valid_applications.push_back(std::move(*application));

        // Continue the enumeration.
        return true;
      });

  return valid_applications;
}

void IncompatibleApplicationsUpdater::OnNewModuleFound(
    const ModuleInfoKey& module_key,
    const ModuleInfoData& module_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // This is meant to create the element in the map if it doesn't exist yet.
  ModuleWarningDecision& warning_decision =
      module_warning_decisions_[module_key];

  // Only consider loaded modules.
  if ((module_data.module_properties & ModuleInfoData::kPropertyLoadedModule) ==
      0) {
    warning_decision = ModuleWarningDecision::kNotLoaded;
    return;
  }

  // Don't check modules if they were never loaded in a process where blocking
  // is enabled.
  if (!IsBlockingEnabledInProcessTypes(module_data.process_types)) {
    warning_decision = ModuleWarningDecision::kAllowedInProcessType;
    return;
  }

  // New modules should not cause a warning when the module analysis is
  // disabled.
  if (module_analysis_disabled_) {
    warning_decision = ModuleWarningDecision::kNotAnalyzed;
    return;
  }

  // First check if this module is a part of Chrome.

  // Explicitly allowlist modules whose signing cert's Subject field matches the
  // one in the current executable. No attempt is made to check the validity of
  // module signatures or of signing certs.
  if (exe_certificate_info_->type != CertificateInfo::Type::NO_CERTIFICATE &&
      exe_certificate_info_->subject ==
          module_data.inspection_result->certificate_info.subject) {
    warning_decision = ModuleWarningDecision::kAllowedSameCertificate;
    return;
  }

  // Second, check if the module is seemingly signed by Microsoft. Again, no
  // attempt is made to check the validity of the certificate.
  if (IsMicrosoftModule(
          module_data.inspection_result->certificate_info.subject)) {
    warning_decision = ModuleWarningDecision::kAllowedMicrosoft;
    return;
  }

  // allowlist modules in the same directory as the executable. This serves 2
  // purposes:
  // - In unsigned builds, this allowlists all of the DLL that are part of
  //   Chrome.
  // - It avoids an issue with the simple heuristic used to determine to which
  //   application a DLL belongs. Without this, if an injected third-party DLL
  //   is first copied into Chrome's directory, Chrome will blame itself as an
  //   incompatible application.
  base::FilePath exe_path;
  if (base::PathService::Get(base::DIR_EXE, &exe_path) &&
      exe_path.DirName().IsParent(module_key.module_path)) {
    warning_decision = ModuleWarningDecision::kAllowedSameDirectory;
    return;
  }

  // Skip modules allowlisted by the Module List component.
  if (module_list_filter_->IsAllowlisted(module_key, module_data)) {
    warning_decision = ModuleWarningDecision::kAllowedAllowlisted;
    return;
  }

  // It is preferable to mark a allowlisted shell extension as allowed because
  // it is allowlisted, not because it's a shell extension. Thus, check for the
  // module type after.
  if (module_data.module_properties & ModuleInfoData::kPropertyShellExtension) {
    warning_decision = ModuleWarningDecision::kAllowedShellExtension;
    return;
  }

  if (module_data.module_properties & ModuleInfoData::kPropertyIme) {
    warning_decision = ModuleWarningDecision::kAllowedIME;
    return;
  }

  // Now it has been determined that the module is unwanted. First check if it
  // is going to be blocked on the next Chrome launch.
  if (module_data.module_properties &
      ModuleInfoData::kPropertyAddedToBlocklist) {
    warning_decision = ModuleWarningDecision::kAddedToBlocklist;
    return;
  }

  // Then check if it can be tied to an installed application on the user's
  // computer.
  std::vector<InstalledApplications::ApplicationInfo> associated_applications;
  bool tied_to_app = installed_applications_->GetInstalledApplications(
      module_key.module_path, &associated_applications);
  UMA_HISTOGRAM_BOOLEAN("ThirdPartyModules.Uninstallable", tied_to_app);
  if (!tied_to_app) {
    warning_decision = ModuleWarningDecision::kNoTiedApplication;
    return;
  }

  warning_decision = ModuleWarningDecision::kIncompatible;

  std::unique_ptr<chrome::conflicts::BlocklistAction> blocklist_action =
      module_list_filter_->IsBlocklisted(module_key, module_data);
  if (!blocklist_action) {
    // The default behavior is to suggest to uninstall.
    blocklist_action = std::make_unique<chrome::conflicts::BlocklistAction>();
    blocklist_action->set_allow_load(true);
    blocklist_action->set_message_type(
        chrome::conflicts::BlocklistMessageType::UNINSTALL);
    blocklist_action->set_message_url(std::string());
  }

  for (auto&& associated_application : associated_applications) {
    incompatible_applications_.emplace_back(
        std::move(associated_application),
        std::make_unique<chrome::conflicts::BlocklistAction>(
            *blocklist_action));
  }
}

void IncompatibleApplicationsUpdater::OnKnownModuleLoaded(
    const ModuleInfoKey& module_key,
    const ModuleInfoData& module_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Analyze the module again.
  OnNewModuleFound(module_key, module_data);
}

void IncompatibleApplicationsUpdater::OnModuleDatabaseIdle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Update the list of incompatible applications on the UI thread. On the first
  // call to UpdateIncompatibleApplications(), the previous value must always be
  // overwritten.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&UpdateIncompatibleApplications, before_first_idle_,
                     std::move(incompatible_applications_)));
  incompatible_applications_.clear();
  before_first_idle_ = false;
}

IncompatibleApplicationsUpdater::ModuleWarningDecision
IncompatibleApplicationsUpdater::GetModuleWarningDecision(
    const ModuleInfoKey& module_key) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = module_warning_decisions_.find(module_key);
  CHECK(it != module_warning_decisions_.end(), base::NotFatalUntil::M130);
  return it->second;
}

void IncompatibleApplicationsUpdater::DisableModuleAnalysis() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  module_analysis_disabled_ = true;
}
