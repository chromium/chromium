// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/conflicts/module_database.h"

#include <tuple>
#include <utility>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/not_fatal_until.h"
#include "base/task/lazy_thread_pool_task_runner.h"
#include "base/task/sequenced_task_runner.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/win/conflicts/module_database_observer.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "base/feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/win/conflicts/incompatible_applications_updater.h"
#include "chrome/browser/win/conflicts/module_load_attempt_log_listener.h"
#include "chrome/browser/win/conflicts/third_party_conflicts_manager.h"
#include "chrome/chrome_elf/third_party_dlls/public_api.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#endif

namespace {

ModuleDatabase* g_module_database = nullptr;

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
// Callback for the pref change registrar. Is invoked when the
// ThirdPartyBlockingEnabled policy is modified. Notifies the ModuleDatabase if
// the policy was disabled.
void OnThirdPartyBlockingPolicyChanged(
    PrefChangeRegistrar* pref_change_registrar) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (ModuleDatabase::IsThirdPartyBlockingPolicyEnabled())
    return;

  // Stop listening to policy changes and notify the ModuleDatabase.
  pref_change_registrar->Remove(prefs::kThirdPartyBlockingEnabled);
  ModuleDatabase::GetTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce([]() {
        ModuleDatabase::GetInstance()->OnThirdPartyBlockingPolicyDisabled();
      }));
}

// Initializes the |pref_change_registrar| on the UI thread, where preferences
// live.
void InitPrefChangeRegistrarOnUIThread(
    PrefChangeRegistrar* pref_change_registrar) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  pref_change_registrar->Init(g_browser_process->local_state());
  // It is safe to pass the pointer to the registrar because the callback will
  // only be invoked if it is still alive.
  pref_change_registrar->Add(
      prefs::kThirdPartyBlockingEnabled,
      base::BindRepeating(&OnThirdPartyBlockingPolicyChanged,
                          pref_change_registrar));
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

}  // namespace

// static
constexpr base::TimeDelta ModuleDatabase::kIdleTimeout;

ModuleDatabase::ModuleDatabase(bool third_party_blocking_policy_enabled)
    : idle_timer_(FROM_HERE,
                  kIdleTimeout,
                  base::BindRepeating(&ModuleDatabase::OnDelayExpired,
                                      base::Unretained(this))),
      has_started_processing_(false),
      shell_extensions_enumerated_(false),
      ime_enumerated_(false),
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      pref_change_registrar_(nullptr, base::OnTaskRunnerDeleter(nullptr)),
#endif
      // ModuleDatabase owns |module_inspector_|, so it is safe to use
      // base::Unretained().
      module_inspector_(base::BindRepeating(&ModuleDatabase::OnModuleInspected,
                                            base::Unretained(this))) {
  AddObserver(&module_inspector_);
  AddObserver(&third_party_metrics_);

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  MaybeInitializeThirdPartyConflictsManager(
      third_party_blocking_policy_enabled);
#endif
}

ModuleDatabase::~ModuleDatabase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (this == g_module_database)
    g_module_database = nullptr;
}

// static
scoped_refptr<base::SequencedTaskRunner> ModuleDatabase::GetTaskRunner() {
  static base::LazyThreadPoolSequencedTaskRunner g_module_database_task_runner =
      LAZY_THREAD_POOL_SEQUENCED_TASK_RUNNER_INITIALIZER(
          base::TaskTraits(base::TaskPriority::BEST_EFFORT,
                           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN));
  return g_module_database_task_runner.Get();
}

// static
ModuleDatabase* ModuleDatabase::GetInstance() {
  DCHECK(GetTaskRunner()->RunsTasksInCurrentSequence());
  return g_module_database;
}

// static
void ModuleDatabase::SetInstance(
    std::unique_ptr<ModuleDatabase> module_database) {
  DCHECK_EQ(nullptr, g_module_database);
  // This is deliberately leaked. It can be cleaned up by manually deleting the
  // ModuleDatabase.
  g_module_database = module_database.release();
}

void ModuleDatabase::StartDrainingModuleLoadAttemptsLog() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // ModuleDatabase owns |module_load_attempt_log_listener_|, so it is safe to
  // use base::Unretained().
  module_load_attempt_log_listener_ =
      std::make_unique<ModuleLoadAttemptLogListener>(base::BindRepeating(
          &ModuleDatabase::OnModuleBlocked, base::Unretained(this)));
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

bool ModuleDatabase::IsIdle() {
  return has_started_processing_ && RegisteredModulesEnumerated() &&
         !idle_timer_.IsRunning() && module_inspector_.IsIdle();
}

void ModuleDatabase::OnShellExtensionEnumerated(const base::FilePath& path,
                                                uint32_t size_of_image,
                                                uint32_t time_date_stamp) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  idle_timer_.Reset();

  ModuleInfo* module_info = nullptr;
  FindOrCreateModuleInfo(path, size_of_image, time_date_stamp, &module_info);
  module_info->second.module_properties |=
      ModuleInfoData::kPropertyShellExtension;
}

void ModuleDatabase::OnShellExtensionEnumerationFinished() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!shell_extensions_enumerated_);

  shell_extensions_enumerated_ = true;

  if (RegisteredModulesEnumerated())
    OnRegisteredModulesEnumerated();
}

void ModuleDatabase::OnImeEnumerated(const base::FilePath& path,
                                     uint32_t size_of_image,
                                     uint32_t time_date_stamp) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  idle_timer_.Reset();

  ModuleInfo* module_info = nullptr;
  FindOrCreateModuleInfo(path, size_of_image, time_date_stamp, &module_info);
  module_info->second.module_properties |= ModuleInfoData::kPropertyIme;
}

void ModuleDatabase::OnImeEnumerationFinished() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!ime_enumerated_);

  ime_enumerated_ = true;

  if (RegisteredModulesEnumerated())
    OnRegisteredModulesEnumerated();
}

void ModuleDatabase::OnModuleLoad(content::ProcessType process_type,
                                  const base::FilePath& module_path,
                                  uint32_t module_size,
                                  uint32_t module_time_date_stamp) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(process_type == content::PROCESS_TYPE_BROWSER ||
         process_type == content::PROCESS_TYPE_RENDERER)
      << "The current logic in ModuleBlocklistCacheUpdater does not support "
         "other process types yet. See https://crbug.com/662084 for details.";

  ModuleInfo* module_info = nullptr;
  bool new_module = FindOrCreateModuleInfo(
      module_path, module_size, module_time_date_stamp, &module_info);

  uint32_t old_module_properties = module_info->second.module_properties;

  // Mark the module as loaded.
  module_info->second.module_properties |=
      ModuleInfoData::kPropertyLoadedModule;

  // Update the list of process types that this module has been seen in.
  module_info->second.process_types |= ProcessTypeToBit(process_type);

  // Some observers care about a known module that is just now loading. Also
  // making sure that the module is ready to be sent to observers.
  bool is_known_module_loading =
      !new_module &&
      old_module_properties != module_info->second.module_properties;
  bool ready_for_notification =
      module_info->second.inspection_result && RegisteredModulesEnumerated();
  if (is_known_module_loading && ready_for_notification) {
    for (auto& observer : observer_list_) {
      observer.OnKnownModuleLoaded(module_info->first, module_info->second);
    }
  }
}

// static
void ModuleDatabase::HandleModuleLoadEvent(content::ProcessType process_type,
                                           const base::FilePath& module_path,
                                           uint32_t module_size,
                                           uint32_t module_time_date_stamp) {
  GetTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](content::ProcessType process_type,
             const base::FilePath& module_path, uint32_t module_size,
             uint32_t module_time_date_stamp) {
            ModuleDatabase::GetInstance()->OnModuleLoad(
                process_type, module_path, module_size, module_time_date_stamp);
          },
          process_type, module_path, module_size, module_time_date_stamp));
}

void ModuleDatabase::OnModuleBlocked(const base::FilePath& module_path,
                                     uint32_t module_size,
                                     uint32_t module_time_date_stamp) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ModuleInfo* module_info = nullptr;
  FindOrCreateModuleInfo(module_path, module_size, module_time_date_stamp,
                         &module_info);

  module_info->second.module_properties |= ModuleInfoData::kPropertyBlocked;
}

void ModuleDatabase::OnModuleAddedToBlocklist(const base::FilePath& module_path,
                                              uint32_t module_size,
                                              uint32_t module_time_date_stamp) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto iter = modules_.find(
      ModuleInfoKey(module_path, module_size, module_time_date_stamp));

  // Only known modules should be added to the blocklist.
  CHECK(iter != modules_.end(), base::NotFatalUntil::M130);

  iter->second.module_properties |= ModuleInfoData::kPropertyAddedToBlocklist;
}

void ModuleDatabase::AddObserver(ModuleDatabaseObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  observer_list_.AddObserver(observer);

  // If the registered modules enumeration is not finished yet, the |observer|
  // will be notified later in OnRegisteredModulesEnumerated().
  if (!RegisteredModulesEnumerated())
    return;

  NotifyLoadedModules(observer);

  if (IsIdle())
    observer->OnModuleDatabaseIdle();
}

void ModuleDatabase::RemoveObserver(ModuleDatabaseObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  observer_list_.RemoveObserver(observer);
}

void ModuleDatabase::StartInspection() {
  module_inspector_.StartInspection();
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
// static
ModuleDatabase* ModuleDatabase::GetInstanceForTesting(
    std::unique_ptr<InstalledApplications> installed_applications) {
  CHECK(g_module_database->third_party_conflicts_manager_);
  g_module_database->third_party_conflicts_manager_
      ->SetInstalledApplicationsForTesting(  // IN-TEST
          std::move(installed_applications));
  return GetInstance();
}

// static
void ModuleDatabase::RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  // Register the pref used to disable the Incompatible Applications warning and
  // the blocking of third-party modules using group policy. Enabled by default.
  registry->RegisterBooleanPref(prefs::kThirdPartyBlockingEnabled, true);
}

// static
bool ModuleDatabase::IsThirdPartyBlockingPolicyEnabled() {
  const PrefService::Preference* third_party_blocking_enabled_pref =
      g_browser_process->local_state()->FindPreference(
          prefs::kThirdPartyBlockingEnabled);
  return !third_party_blocking_enabled_pref->IsManaged() ||
         third_party_blocking_enabled_pref->GetValue()->GetBool();
}

// static
void ModuleDatabase::DisableThirdPartyBlocking() {
  // Immediately disable the hook. DisableHook() can be called concurrently.
  DisableHook();

  // Notify the ModuleDatabase instance.
  GetTaskRunner()->PostTask(FROM_HERE, base::BindOnce([]() {
                              GetInstance()->OnThirdPartyBlockingDisabled();
                            }));
}

void ModuleDatabase::OnThirdPartyBlockingPolicyDisabled() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(third_party_conflicts_manager_);

  ThirdPartyConflictsManager::ShutdownAndDestroy(
      std::move(third_party_conflicts_manager_));
  // The registrar is no longer observing the local state prefs, so there's no
  // point in keeping it around.
  pref_change_registrar_ = nullptr;
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

bool ModuleDatabase::FindOrCreateModuleInfo(
    const base::FilePath& module_path,
    uint32_t module_size,
    uint32_t module_time_date_stamp,
    ModuleDatabase::ModuleInfo** module_info) {
  auto result = modules_.emplace(
      std::piecewise_construct,
      std::forward_as_tuple(module_path, module_size, module_time_date_stamp),
      std::forward_as_tuple());

  // New modules must be inspected.
  bool new_module = result.second;
  if (new_module) {
    has_started_processing_ = true;
    idle_timer_.Reset();

    module_inspector_.AddModule(result.first->first);
  }

  *module_info = &(*result.first);
  return new_module;
}

bool ModuleDatabase::RegisteredModulesEnumerated() {
  return shell_extensions_enumerated_ && ime_enumerated_;
}

void ModuleDatabase::OnRegisteredModulesEnumerated() {
  for (auto& observer : observer_list_)
    NotifyLoadedModules(&observer);

  if (IsIdle())
    EnterIdleState();
}

void ModuleDatabase::OnModuleInspected(
    const ModuleInfoKey& module_key,
    ModuleInspectionResult inspection_result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = modules_.find(module_key);
  if (it == modules_.end())
    return;

  it->second.inspection_result = std::move(inspection_result);

  if (RegisteredModulesEnumerated())
    for (auto& observer : observer_list_)
      observer.OnNewModuleFound(it->first, it->second);

  // Notify the observers if this was the last outstanding module inspection and
  // the delay has already expired.
  if (IsIdle())
    EnterIdleState();
}

void ModuleDatabase::OnDelayExpired() {
  // Notify the observers if there are no outstanding module inspections.
  if (IsIdle())
    EnterIdleState();
}

void ModuleDatabase::EnterIdleState() {
  for (auto& observer : observer_list_)
    observer.OnModuleDatabaseIdle();
}

void ModuleDatabase::NotifyLoadedModules(ModuleDatabaseObserver* observer) {
  for (const auto& module : modules_) {
    if (module.second.inspection_result)
      observer->OnNewModuleFound(module.first, module.second);
  }
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
void ModuleDatabase::OnThirdPartyBlockingDisabled() {
  third_party_metrics_.SetHookDisabled();

  if (third_party_conflicts_manager_)
    third_party_conflicts_manager_->DisableModuleAnalysis();
}

void ModuleDatabase::MaybeInitializeThirdPartyConflictsManager(
    bool third_party_blocking_policy_enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!third_party_blocking_policy_enabled)
    return;

  if (IncompatibleApplicationsUpdater::IsWarningEnabled() ||
      ModuleBlocklistCacheUpdater::IsBlockingEnabled()) {
    StartInspection();

    third_party_conflicts_manager_ =
        std::make_unique<ThirdPartyConflictsManager>(this);

    // If Chrome detects that the group policy for third-party blocking gets
    // disabled at run-time, the |third_party_conflicts_manager_| instance must
    // be destroyed. Since prefs can only be read on the UI thread, the
    // registrar is initialized there.
    auto ui_task_runner = content::GetUIThreadTaskRunner({});
    pref_change_registrar_ =
        std::unique_ptr<PrefChangeRegistrar, base::OnTaskRunnerDeleter>(
            new PrefChangeRegistrar(),
            base::OnTaskRunnerDeleter(ui_task_runner));
    ui_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(&InitPrefChangeRegistrarOnUIThread,
                       base::Unretained(pref_change_registrar_.get())));
  }
}
#endif
