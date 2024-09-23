// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/conflicts/conflicts_data_fetcher.h"

#include <string>
#include <string_view>
#include <utility>

#include "base/strings/string_util.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/win/conflicts/module_database.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "base/win/win_util.h"
#include "chrome/browser/win/conflicts/incompatible_applications_updater.h"
#include "chrome/browser/win/conflicts/module_blocklist_cache_updater.h"
#endif

namespace {

// Converts the process_types bit field to a simple string representation where
// each process type is represented by a letter. E.g. B for browser process.
// Full process names are not used in order to save horizontal space in the
// conflicts UI.
std::string GetProcessTypesString(const ModuleInfoData& module_data) {
  uint32_t process_types = module_data.process_types;

  if (!process_types)
    return "None";

  std::string result;
  if (process_types & ProcessTypeToBit(content::PROCESS_TYPE_BROWSER))
    result.append("B");
  if (process_types & ProcessTypeToBit(content::PROCESS_TYPE_RENDERER))
    result.append("R");
  // TODO(pmonette): Add additional process types as more get supported.

  return result;
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)

// Strings used twice.
constexpr char kNotLoaded[] = "Not loaded";
constexpr char kAllowedInProcessType[] = "Allowed - Loaded in allowed process";
constexpr char kAllowedInputMethodEditor[] = "Allowed - Input method editor";
constexpr char kAllowedMatchingCertificate[] = "Allowed - Matching certificate";
constexpr char kAllowedMicrosoftModule[] = "Allowed - Microsoft module";
constexpr char kAllowedAllowlisted[] = "Allowed - Allowlisted";
constexpr char kNotAnalyzed[] =
    "Tolerated - Not analyzed (See https://crbug.com/892294)";
constexpr char kAllowedSameDirectory[] =
#if defined(OFFICIAL_BUILD)
    // In official builds, modules in the Chrome directory are blocked but they
    // won't cause a warning because the warning would blame Chrome itself.
    "Tolerated - In executable directory";
#else  // !defined(OFFICIAL_BUILD)
    // In developer builds, DLLs that are part of Chrome are not signed and thus
    // the easy way to identify them is to check that they are in the same
    // directory (or child folder) as the main exe.
    "Allowed - In executable directory (dev builds only)";
#endif

void AppendString(std::string_view input, std::string* output) {
  if (!output->empty())
    *output += ", ";
  output->append(input);
}

// Returns a string describing the current module blocking status: loaded or
// not, blocked or not, was in blocklist cache or not, bypassed blocking or not.
std::string GetBlockingStatusString(
    const ModuleBlocklistCacheUpdater::ModuleBlockingState& blocking_state) {
  std::string status;

  // Output status regarding the blocklist cache, current blocking, and
  // load status.
  if (blocking_state.was_blocked)
    status = "Blocked";
  if (!blocking_state.was_loaded)
    AppendString(kNotLoaded, &status);
  else if (blocking_state.was_in_blocklist_cache)
    AppendString("Bypassed blocking", &status);
  if (blocking_state.was_in_blocklist_cache)
    AppendString("In blocklist cache", &status);

  return status;
}

// Returns a string describing the blocking decision related to a module. This
// returns the empty string to indicate that the warning decision description
// should be used instead.
std::string GetBlockingDecisionString(
    const ModuleBlocklistCacheUpdater::ModuleBlockingState& blocking_state,
    IncompatibleApplicationsUpdater* incompatible_applications_updater) {
  using BlockingDecision = ModuleBlocklistCacheUpdater::ModuleBlockingDecision;

  // Append status regarding the logic that will be applied during the next
  // startup.
  switch (blocking_state.blocking_decision) {
    case BlockingDecision::kUnknown:
      NOTREACHED_IN_MIGRATION();
      break;
    case BlockingDecision::kNotLoaded:
      return kNotLoaded;
    case BlockingDecision::kAllowedInProcessType:
      return kAllowedInProcessType;
    case BlockingDecision::kAllowedIME:
      return kAllowedInputMethodEditor;
    case BlockingDecision::kAllowedSameCertificate:
      return kAllowedMatchingCertificate;
    case BlockingDecision::kAllowedSameDirectory:
      return kAllowedSameDirectory;
    case BlockingDecision::kAllowedMicrosoft:
      return kAllowedMicrosoftModule;
    case BlockingDecision::kAllowedAllowlisted:
      return kAllowedAllowlisted;
    case BlockingDecision::kNotAnalyzed:
      return kNotAnalyzed;
    case BlockingDecision::kTolerated:
      // This is a module explicitly allowed to load by the Module List
      // component. But it is still valid for a potential warning, and so the
      // warning status is used instead.
      if (incompatible_applications_updater)
        break;
      return "Tolerated - Will be blocked in the future";
    case BlockingDecision::kDisallowedExplicit:
      return "Disallowed - Explicitly blocklisted";
    case BlockingDecision::kDisallowedImplicit:
      return "Disallowed - Implicitly blocklisted";
  }

  // Returning an empty string indicates that the warning status should be used.
  return std::string();
}

// Returns a string describing the warning decision that was made regarding a
// module.
std::string GetModuleWarningDecisionString(
    const ModuleInfoKey& module_key,
    IncompatibleApplicationsUpdater* incompatible_applications_updater) {
  using WarningDecision =
      IncompatibleApplicationsUpdater::ModuleWarningDecision;

  WarningDecision warning_decision =
      incompatible_applications_updater->GetModuleWarningDecision(module_key);

  switch (warning_decision) {
    case WarningDecision::kNotLoaded:
      return kNotLoaded;
    case WarningDecision::kAllowedInProcessType:
      return kAllowedInProcessType;
    case WarningDecision::kAllowedIME:
      return kAllowedInputMethodEditor;
    case WarningDecision::kAllowedShellExtension:
      return "Tolerated - Shell extension";
    case WarningDecision::kAllowedSameCertificate:
      return kAllowedMatchingCertificate;
    case WarningDecision::kAllowedSameDirectory:
      return kAllowedSameDirectory;
    case WarningDecision::kAllowedMicrosoft:
      return kAllowedMicrosoftModule;
    case WarningDecision::kAllowedAllowlisted:
      return kAllowedAllowlisted;
    case WarningDecision::kNotAnalyzed:
      return kNotAnalyzed;
    case WarningDecision::kNoTiedApplication:
      return "Tolerated - Could not tie to an installed application";
    case WarningDecision::kIncompatible:
      return "Incompatible";
    case WarningDecision::kAddedToBlocklist:
    case WarningDecision::kUnknown:
      NOTREACHED_IN_MIGRATION();
      break;
  }

  return std::string();
}

std::string GetModuleStatusString(
    const ModuleInfoKey& module_key,
    IncompatibleApplicationsUpdater* incompatible_applications_updater,
    ModuleBlocklistCacheUpdater* module_blocklist_cache_updater) {
  if (!incompatible_applications_updater && !module_blocklist_cache_updater)
    return std::string();

  std::string status;

  // The blocking status is shown over the warning status.
  if (module_blocklist_cache_updater) {
    const ModuleBlocklistCacheUpdater::ModuleBlockingState& blocking_state =
        module_blocklist_cache_updater->GetModuleBlockingState(module_key);

    status = GetBlockingStatusString(blocking_state);

    std::string blocking_string = GetBlockingDecisionString(
        blocking_state, incompatible_applications_updater);
    if (!blocking_string.empty()) {
      AppendString(blocking_string, &status);
      return status;
    }

    // An empty |blocking_string| indicates that a warning decision string
    // should be used instead.
  }

  if (incompatible_applications_updater) {
    AppendString(GetModuleWarningDecisionString(
                     module_key, incompatible_applications_updater),
                 &status);
  }

  return status;
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

enum ThirdPartyFeaturesStatus {
  // The third-party features are not available in non-Google Chrome builds.
  kNonGoogleChromeBuild,
  // The ThirdPartyBlockingEnabled group policy is disabled.
  kPolicyDisabled,
  // Both the IncompatibleApplicationsWarning and the
  // ThirdPartyModulesBlocking features are disabled.
  kFeatureDisabled,
  // The Module List version received is invalid.
  kModuleListInvalid,
  // There is no Module List version available.
  kNoModuleListAvailable,
  // Only the IncompatibleApplicationsWarning feature is initialized.
  kWarningInitialized,
  // Only the ThirdPartyModulesBlocking feature is initialized.
  kBlockingInitialized,
  // Both the IncompatibleApplicationsWarning and the
  // ThirdPartyModulesBlocking feature are initialized.
  kWarningAndBlockingInitialized,
};

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
ThirdPartyFeaturesStatus GetThirdPartyFeaturesStatus(
    std::optional<ThirdPartyConflictsManager::State>
        third_party_conflicts_manager_state) {
  // The ThirdPartyConflictsManager instance exists if we have its state.
  if (third_party_conflicts_manager_state.has_value()) {
    switch (third_party_conflicts_manager_state.value()) {
      case ThirdPartyConflictsManager::State::kModuleListInvalidFailure:
        return kModuleListInvalid;
      case ThirdPartyConflictsManager::State::kNoModuleListAvailableFailure:
        return kNoModuleListAvailable;
      case ThirdPartyConflictsManager::State::kWarningInitialized:
        return kWarningInitialized;
      case ThirdPartyConflictsManager::State::kBlockingInitialized:
        return kBlockingInitialized;
      case ThirdPartyConflictsManager::State::kWarningAndBlockingInitialized:
        return kWarningAndBlockingInitialized;
      case ThirdPartyConflictsManager::State::kDestroyed:
        // Turning off the feature via group policy is the only way to have the
        // manager destroyed.
        return kPolicyDisabled;
    }
  }

  if (!ModuleDatabase::IsThirdPartyBlockingPolicyEnabled())
    return kPolicyDisabled;

  if (!IncompatibleApplicationsUpdater::IsWarningEnabled() &&
      !ModuleBlocklistCacheUpdater::IsBlockingEnabled()) {
    return kFeatureDisabled;
  }

  // The above 3 cases are the only possible reasons why the manager wouldn't
  // exist.
  NOTREACHED_IN_MIGRATION();
  return kFeatureDisabled;
}
#endif

bool IsThirdPartyFeatureEnabled(ThirdPartyFeaturesStatus status) {
  return status == kWarningInitialized || status == kBlockingInitialized ||
         status == kWarningAndBlockingInitialized;
}

std::string GetThirdPartyFeaturesStatusString(ThirdPartyFeaturesStatus status) {
  switch (status) {
    case ThirdPartyFeaturesStatus::kNonGoogleChromeBuild:
      return "The third-party features are not available in non-Google Chrome "
             "builds.";
    case ThirdPartyFeaturesStatus::kPolicyDisabled:
      return "The ThirdPartyBlockingEnabled group policy is disabled.";
    case ThirdPartyFeaturesStatus::kFeatureDisabled:
      return "Both the IncompatibleApplicationsWarning and "
             "ThirdPartyModulesBlocking features are disabled.";
    case ThirdPartyFeaturesStatus::kModuleListInvalid:
      return "Disabled - The Module List component version is invalid.";
    case ThirdPartyFeaturesStatus::kNoModuleListAvailable:
      return "Disabled - There is no Module List version available.";
    case ThirdPartyFeaturesStatus::kWarningInitialized:
      return "The IncompatibleApplicationsWarning feature is enabled, while "
             "the ThirdPartyModulesBlocking feature is disabled.";
    case ThirdPartyFeaturesStatus::kBlockingInitialized:
      return "The ThirdPartyModulesBlocking feature is enabled, while the "
             "IncompatibleApplicationsWarning feature is disabled.";
    case ThirdPartyFeaturesStatus::kWarningAndBlockingInitialized:
      return "Both the IncompatibleApplicationsWarning and "
             "ThirdPartyModulesBlocking features are enabled";
  }
}

void OnConflictsDataFetched(
    ConflictsDataFetcher::OnConflictsDataFetchedCallback
        on_conflicts_data_fetched_callback,
    base::Value::Dict results,
    ThirdPartyFeaturesStatus third_party_features_status) {
  // Third-party conflicts status.
  results.Set("thirdPartyFeatureEnabled",
              IsThirdPartyFeatureEnabled(third_party_features_status));
  results.Set("thirdPartyFeatureStatus",
              GetThirdPartyFeaturesStatusString(third_party_features_status));

  std::move(on_conflicts_data_fetched_callback).Run(std::move(results));
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
void OnModuleDataFetched(ConflictsDataFetcher::OnConflictsDataFetchedCallback
                             on_conflicts_data_fetched_callback,
                         base::Value::Dict results,
                         std::optional<ThirdPartyConflictsManager::State>
                             third_party_conflicts_manager_state) {
  OnConflictsDataFetched(
      std::move(on_conflicts_data_fetched_callback), std::move(results),
      GetThirdPartyFeaturesStatus(third_party_conflicts_manager_state));
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

}  // namespace

ConflictsDataFetcher::~ConflictsDataFetcher() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (module_list_.has_value())
    ModuleDatabase::GetInstance()->RemoveObserver(this);
}

// static
ConflictsDataFetcher::UniquePtr ConflictsDataFetcher::Create(
    OnConflictsDataFetchedCallback on_conflicts_data_fetched_callback) {
  return std::unique_ptr<ConflictsDataFetcher, base::OnTaskRunnerDeleter>(
      new ConflictsDataFetcher(std::move(on_conflicts_data_fetched_callback)),
      base::OnTaskRunnerDeleter(ModuleDatabase::GetTaskRunner()));
}

ConflictsDataFetcher::ConflictsDataFetcher(
    OnConflictsDataFetchedCallback on_conflicts_data_fetched_callback)
    : on_conflicts_data_fetched_callback_(
          std::move(on_conflicts_data_fetched_callback))
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      ,
      weak_ptr_factory_(this)
#endif
{
  DETACH_FROM_SEQUENCE(sequence_checker_);

  ModuleDatabase::GetTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ConflictsDataFetcher::InitializeOnModuleDatabaseTaskRunner,
          base::Unretained(this)));
}

void ConflictsDataFetcher::InitializeOnModuleDatabaseTaskRunner() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // If the ThirdPartyConflictsManager instance exists, wait until it is fully
  // initialized before retrieving the list of modules.
  auto* third_party_conflicts_manager =
      ModuleDatabase::GetInstance()->third_party_conflicts_manager();
  if (third_party_conflicts_manager) {
    third_party_conflicts_manager->ForceInitialization(base::BindRepeating(
        &ConflictsDataFetcher::OnManagerInitializationComplete,
        weak_ptr_factory_.GetWeakPtr()));
    return;
  }
#endif

  GetListOfModules();
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
void ConflictsDataFetcher::OnManagerInitializationComplete(
    ThirdPartyConflictsManager::State state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  third_party_conflicts_manager_state_ = state;

  GetListOfModules();
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

void ConflictsDataFetcher::GetListOfModules() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // The request is handled asynchronously, filling up the |module_list_|,
  // and will callback via OnModuleDatabaseIdle() on completion.
  module_list_ = base::Value::List();

  auto* module_database = ModuleDatabase::GetInstance();
  module_database->StartInspection();
  module_database->AddObserver(this);
}

void ConflictsDataFetcher::OnNewModuleFound(const ModuleInfoKey& module_key,
                                            const ModuleInfoData& module_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(module_list_);

  base::Value::Dict data;

  data.Set("third_party_module_status", std::string());
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (ModuleDatabase::GetInstance()->third_party_conflicts_manager()) {
    auto* incompatible_applications_updater =
        ModuleDatabase::GetInstance()
            ->third_party_conflicts_manager()
            ->incompatible_applications_updater();
    auto* module_blocklist_cache_updater =
        ModuleDatabase::GetInstance()
            ->third_party_conflicts_manager()
            ->module_blocklist_cache_updater();

    data.Set(
        "third_party_module_status",
        GetModuleStatusString(module_key, incompatible_applications_updater,
                              module_blocklist_cache_updater));
  }
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

  std::string type_string;
  if (module_data.module_properties & ModuleInfoData::kPropertyShellExtension)
    type_string = "Shell extension";
  data.Set("type_description", type_string);

  const auto& inspection_result = *module_data.inspection_result;
  data.Set("location", inspection_result.location);
  data.Set("name", inspection_result.basename);
  data.Set("product_name", inspection_result.product_name);
  data.Set("description", inspection_result.description);
  data.Set("version", inspection_result.version);
  data.Set("digital_signer", inspection_result.certificate_info.subject);
  data.Set("code_id", GenerateCodeId(module_key));
  data.Set("process_types", GetProcessTypesString(module_data));

  module_list_->Append(std::move(data));
}

void ConflictsDataFetcher::OnModuleDatabaseIdle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(module_list_);

  ModuleDatabase::GetInstance()->RemoveObserver(this);

  base::Value::Dict results;
  results.Set("moduleCount", static_cast<int>(module_list_->size()));
  results.Set("moduleList", std::move(*module_list_));
  module_list_ = std::nullopt;

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // The state of third-party features must be determined on the UI thread.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          OnModuleDataFetched, std::move(on_conflicts_data_fetched_callback_),
          std::move(results), std::move(third_party_conflicts_manager_state_)));
#else
  // The third-party features are always disabled on Chromium builds.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(OnConflictsDataFetched,
                                std::move(on_conflicts_data_fetched_callback_),
                                std::move(results), kNonGoogleChromeBuild));
#endif
}
