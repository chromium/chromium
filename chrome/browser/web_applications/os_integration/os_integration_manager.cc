// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"

#include <memory>
#include <utility>

#include "base/auto_reset.h"
#include "base/barrier_callback.h"
#include "base/barrier_closure.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/os_integration/file_handling_sub_manager.h"
#include "chrome/browser/web_applications/os_integration/os_integration_sub_manager.h"
#include "chrome/browser/web_applications/os_integration/protocol_handling_sub_manager.h"
#include "chrome/browser/web_applications/os_integration/run_on_os_login_sub_manager.h"
#include "chrome/browser/web_applications/os_integration/shortcut_menu_handling_sub_manager.h"
#include "chrome/browser/web_applications/os_integration/shortcut_sub_manager.h"
#include "chrome/browser/web_applications/os_integration/uninstallation_via_os_settings_sub_manager.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut.h"
#include "chrome/browser/web_applications/os_integration/web_app_uninstallation_via_os_settings_registration.h"
#include "chrome/browser/web_applications/proto/web_app_os_integration_state.pb.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/common/chrome_features.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/web_applications/app_shim_registry_mac.h"
#endif

namespace {
bool g_suppress_os_hooks_for_testing_ = false;
}  // namespace

namespace web_app {

namespace {
OsHooksErrors GetFinalErrorBitsetFromCollection(
    std::vector<OsHooksErrors> os_hooks_errors) {
  OsHooksErrors final_errors;
  for (const OsHooksErrors& error : os_hooks_errors) {
    final_errors = final_errors | error;
  }
  return final_errors;
}
}  // namespace

bool AreOsIntegrationSubManagersEnabled() {
  return base::FeatureList::IsEnabled(features::kOsIntegrationSubManagers);
}

bool AreSubManagersExecuteEnabled() {
  if (!AreOsIntegrationSubManagersEnabled())
    return false;
  return (features::kOsIntegrationSubManagersStageParam.Get() ==
          features::OsIntegrationSubManagersStage::kExecuteAndWriteConfig);
}

OsIntegrationManager::ScopedSuppressForTesting::ScopedSuppressForTesting()
    :
// Creating OS hooks on ChromeOS doesn't write files to disk, so it's
// unnecessary to suppress and it provides better crash coverage.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
      scope_(&g_suppress_os_hooks_for_testing_, true)
#else
      scope_(&g_suppress_os_hooks_for_testing_, false)
#endif
{
}

OsIntegrationManager::ScopedSuppressForTesting::~ScopedSuppressForTesting() =
    default;

// This barrier is designed to accumulate errors from calls to OS hook
// operations, and call the completion callback when all OS hook operations
// have completed. The |callback| is called when all copies of this object and
// all callbacks created using this object are destroyed.
class OsIntegrationManager::OsHooksBarrier
    : public base::RefCounted<OsHooksBarrier> {
 public:
  explicit OsHooksBarrier(OsHooksErrors errors_default,
                          InstallOsHooksCallback callback)
      : errors_(errors_default), callback_(std::move(callback)) {}

  void OnError(OsHookType::Type type) { AddResult(type, Result::kError); }

  ResultCallback CreateBarrierCallbackForType(OsHookType::Type type) {
    return base::BindOnce(&OsHooksBarrier::AddResult, this, type);
  }

 private:
  friend class base::RefCounted<OsHooksBarrier>;

  ~OsHooksBarrier() {
    CHECK(callback_);
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback_), std::move(errors_)));
  }

  void AddResult(OsHookType::Type type, Result result) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    errors_[type] = result == Result::kError ? true : false;
  }

  OsHooksErrors errors_;
  InstallOsHooksCallback callback_;
};

InstallOsHooksOptions::InstallOsHooksOptions() = default;
InstallOsHooksOptions::InstallOsHooksOptions(
    const InstallOsHooksOptions& other) = default;
InstallOsHooksOptions& InstallOsHooksOptions::operator=(
    const InstallOsHooksOptions& other) = default;

OsIntegrationManager::OsIntegrationManager(
    Profile* profile,
    std::unique_ptr<WebAppShortcutManager> shortcut_manager,
    std::unique_ptr<WebAppFileHandlerManager> file_handler_manager,
    std::unique_ptr<WebAppProtocolHandlerManager> protocol_handler_manager,
    std::unique_ptr<UrlHandlerManager> url_handler_manager)
    : profile_(profile),
      shortcut_manager_(std::move(shortcut_manager)),
      file_handler_manager_(std::move(file_handler_manager)),
      protocol_handler_manager_(std::move(protocol_handler_manager)),
      url_handler_manager_(std::move(url_handler_manager)) {}

OsIntegrationManager::~OsIntegrationManager() = default;

// static
base::RepeatingCallback<void(OsHooksErrors)>
OsIntegrationManager::GetBarrierForSynchronize(
    AnyOsHooksErrorCallback errors_callback) {
  // There are always 2 barriers, one for the normal OS Hook call and one for
  // Synchronize().
  int num_barriers = 2;

  auto barrier_callback_for_synchronize = base::BarrierCallback<OsHooksErrors>(
      num_barriers,
      base::BindOnce(
          [](AnyOsHooksErrorCallback callback,
             std::vector<OsHooksErrors> combined_errors) {
            std::move(callback).Run(
                GetFinalErrorBitsetFromCollection(combined_errors));
          },
          std::move(errors_callback)));
  return barrier_callback_for_synchronize;
}

void OsIntegrationManager::SetProvider(base::PassKey<WebAppProvider>,
                                       WebAppProvider& provider) {
  CHECK(!first_synchronize_called_);

  provider_ = &provider;

  base::PassKey<OsIntegrationManager> pass_key;
  file_handler_manager_->SetProvider(pass_key, provider);
  shortcut_manager_->SetProvider(pass_key, provider);
  if (protocol_handler_manager_)
    protocol_handler_manager_->SetProvider(pass_key, provider);

  sub_managers_.clear();
  sub_managers_.push_back(
      std::make_unique<ShortcutSubManager>(*profile_, provider));
  sub_managers_.push_back(
      std::make_unique<FileHandlingSubManager>(profile_->GetPath(), provider));
  sub_managers_.push_back(std::make_unique<ProtocolHandlingSubManager>(
      profile_->GetPath(), provider));
  sub_managers_.push_back(std::make_unique<ShortcutMenuHandlingSubManager>(
      profile_->GetPath(), provider));
  sub_managers_.push_back(
      std::make_unique<RunOnOsLoginSubManager>(*profile_, provider));
  sub_managers_.push_back(
      std::make_unique<UninstallationViaOsSettingsSubManager>(
          profile_->GetPath(), provider));

  set_provider_called_ = true;
}

void OsIntegrationManager::Start() {
  CHECK(provider_);
  CHECK(file_handler_manager_);

  registrar_observation_.Observe(&provider_->registrar_unsafe());
  shortcut_manager_->Start();
  file_handler_manager_->Start();
  if (protocol_handler_manager_)
    protocol_handler_manager_->Start();
}

void OsIntegrationManager::Synchronize(
    const webapps::AppId& app_id,
    base::OnceClosure callback,
    absl::optional<SynchronizeOsOptions> options) {
  first_synchronize_called_ = true;

  // This is usually called to clean up OS integration states on the OS,
  // regardless of whether there are apps existing in the app registry or not.
  if (AreSubManagersExecuteEnabled() && options.has_value() &&
      options.value().force_unregister_os_integration) {
    ForceUnregisterOsIntegrationOnSubManager(
        app_id, /*index=*/0,
        std::move(callback).Then(
            base::BindOnce(force_unregister_callback_for_testing_, app_id)));
    return;
  }

  // If the app does not exist in the DB and an unregistration is required, it
  // should have been done in the past Synchronize call.
  CHECK(provider_->registrar_unsafe().GetAppById(app_id))
      << "Can't perform OS integration without the app existing in the "
         "registrar. If the use-case requires an app to not be installed, "
         "consider setting the force_unregister_os_integration flag inside "
         "SynchronizeOsOptions";

  CHECK(set_provider_called_);

  if (!AreOsIntegrationSubManagersEnabled()) {
    std::move(callback).Run();
    return;
  }

  if (sub_managers_.empty()) {
    std::move(callback).Run();
    return;
  }

  std::unique_ptr<proto::WebAppOsIntegrationState> desired_states =
      std::make_unique<proto::WebAppOsIntegrationState>();
  proto::WebAppOsIntegrationState* desired_states_ptr = desired_states.get();

  // Note: Sometimes the execute step is a no-op based on feature flags or if os
  // integration is disabled for testing. This logic is in the
  // StartSubManagerExecutionIfRequired method.
  base::RepeatingClosure configure_barrier;
  configure_barrier = base::BarrierClosure(
      sub_managers_.size(),
      base::BindOnce(&OsIntegrationManager::StartSubManagerExecutionIfRequired,
                     weak_ptr_factory_.GetWeakPtr(), app_id, options,
                     std::move(desired_states), std::move(callback)));

  for (const auto& sub_manager : sub_managers_) {
    // This dereference is safe because the barrier closure guarantees that it
    // will not be called until `configure_barrier` is called from each sub-
    // manager.
    sub_manager->Configure(app_id, *desired_states_ptr, configure_barrier);
  }
}

void OsIntegrationManager::InstallOsHooks(
    const webapps::AppId& app_id,
    InstallOsHooksCallback callback,
    std::unique_ptr<WebAppInstallInfo> web_app_info,
    InstallOsHooksOptions options) {
  // If the "Execute" step is enabled for sub-managers, then the 'old' os
  // integration path needs to be turned off so that os integration doesn't get
  // done twice.
  if (g_suppress_os_hooks_for_testing_ || AreSubManagersExecuteEnabled()) {
    OsHooksErrors os_hooks_errors;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), os_hooks_errors));
    return;
  }
  MacAppShimOnAppInstalledForProfile(app_id);

  OsHooksErrors os_hooks_errors;
  scoped_refptr<OsHooksBarrier> barrier = base::MakeRefCounted<OsHooksBarrier>(
      os_hooks_errors, std::move(callback));

  CHECK(options.os_hooks[OsHookType::kShortcuts] ||
        !options.os_hooks[OsHookType::kShortcutsMenu])
      << "Cannot install shortcuts menu without installing shortcuts.";

  auto shortcuts_callback = base::BindOnce(
      &OsIntegrationManager::OnShortcutsCreated, weak_ptr_factory_.GetWeakPtr(),
      app_id, std::move(web_app_info), options, barrier);

#if BUILDFLAG(IS_MAC)
  // This has to happen before creating shortcuts on Mac because the shortcut
  // creation step uses the file type associations which are marked for enabling
  // by `RegisterFileHandlers()`.
  if (options.os_hooks[OsHookType::kFileHandlers]) {
    RegisterFileHandlers(app_id, barrier->CreateBarrierCallbackForType(
                                     OsHookType::kFileHandlers));
  }
#endif

  // TODO(ortuno): Make adding a shortcut to the applications menu independent
  // from adding a shortcut to desktop.
  if (options.os_hooks[OsHookType::kShortcuts]) {
    CreateShortcuts(app_id, options.add_to_desktop, options.reason,
                    std::move(shortcuts_callback));
  } else {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(shortcuts_callback),
                                  /*shortcuts_created=*/false));
  }
}

void OsIntegrationManager::UninstallAllOsHooks(
    const webapps::AppId& app_id,
    UninstallOsHooksCallback callback) {
  OsHooksOptions os_hooks;
  os_hooks.set();
  UninstallOsHooks(app_id, os_hooks, std::move(callback));
}

void OsIntegrationManager::UninstallOsHooks(const webapps::AppId& app_id,
                                            const OsHooksOptions& os_hooks,
                                            UninstallOsHooksCallback callback) {
  // If the "Execute" step is enabled for sub-managers, then the 'old' os
  // integration path needs to be turned off so that os integration doesn't get
  // done twice.
  if (g_suppress_os_hooks_for_testing_ || AreSubManagersExecuteEnabled()) {
    OsHooksErrors os_hooks_errors;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), os_hooks_errors));
    return;
  }

  OsHooksErrors os_hooks_errors;
  scoped_refptr<OsHooksBarrier> barrier = base::MakeRefCounted<OsHooksBarrier>(
      os_hooks_errors, std::move(callback));

  if (os_hooks[OsHookType::kShortcutsMenu]) {
    bool success = UnregisterShortcutsMenu(
        app_id,
        barrier->CreateBarrierCallbackForType(OsHookType::kShortcutsMenu));
    if (!success)
      barrier->OnError(OsHookType::kShortcutsMenu);
  }

  if (os_hooks[OsHookType::kRunOnOsLogin] &&
      base::FeatureList::IsEnabled(features::kDesktopPWAsRunOnOsLogin)) {
    UnregisterRunOnOsLogin(app_id, barrier->CreateBarrierCallbackForType(
                                       OsHookType::kRunOnOsLogin));
  }

  if (os_hooks[OsHookType::kShortcuts]) {
    std::unique_ptr<ShortcutInfo> shortcut_info = BuildShortcutInfo(app_id);
    base::FilePath shortcut_data_dir =
        internals::GetShortcutDataDir(*shortcut_info);

    DeleteShortcuts(
        app_id, shortcut_data_dir, std::move(shortcut_info),
        barrier->CreateBarrierCallbackForType(OsHookType::kShortcuts));
  }
  // unregistration and record errors during unregistration.
  if (os_hooks[OsHookType::kFileHandlers]) {
    UnregisterFileHandlers(app_id, barrier->CreateBarrierCallbackForType(
                                       OsHookType::kFileHandlers));
  }

  if (os_hooks[OsHookType::kProtocolHandlers]) {
    UnregisterProtocolHandlers(app_id, barrier->CreateBarrierCallbackForType(
                                           OsHookType::kProtocolHandlers));
  }

  if (os_hooks[OsHookType::kUrlHandlers])
    UnregisterUrlHandlers(app_id);

  // There is a chance uninstallation point was created with feature flag
  // enabled so we need to clean it up regardless of feature flag state.
  if (os_hooks[OsHookType::kUninstallationViaOsSettings])
    UnregisterWebAppOsUninstallation(app_id);
}

void OsIntegrationManager::UpdateOsHooks(
    const webapps::AppId& app_id,
    base::StringPiece old_name,
    FileHandlerUpdateAction file_handlers_need_os_update,
    const WebAppInstallInfo& web_app_info,
    UpdateOsHooksCallback callback) {
  // If the "Execute" step is enabled for sub-managers, then the 'old' os
  // integration path needs to be turned off so that os integration doesn't get
  // done twice.
  if (g_suppress_os_hooks_for_testing_ || AreSubManagersExecuteEnabled()) {
    OsHooksErrors os_hooks_errors;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), os_hooks_errors));
    return;
  }

  OsHooksErrors os_hooks_errors;
  scoped_refptr<OsHooksBarrier> barrier = base::MakeRefCounted<OsHooksBarrier>(
      os_hooks_errors, std::move(callback));

  UpdateFileHandlers(app_id, file_handlers_need_os_update,
                     base::BindOnce(barrier->CreateBarrierCallbackForType(
                         OsHookType::kFileHandlers)));
  UpdateShortcuts(app_id, old_name,
                  base::BindOnce(barrier->CreateBarrierCallbackForType(
                      OsHookType::kShortcuts)));
  UpdateShortcutsMenu(app_id, web_app_info,
                      base::BindOnce(barrier->CreateBarrierCallbackForType(
                          OsHookType::kShortcutsMenu)));
  UpdateUrlHandlers(
      app_id,
      base::BindOnce(
          [](ResultCallback callback, bool success) {
            std::move(callback).Run(success ? Result::kOk : Result::kError);
          },
          barrier->CreateBarrierCallbackForType(OsHookType::kUrlHandlers)));
  UpdateProtocolHandlers(app_id, /*force_shortcut_updates_if_needed=*/false,
                         base::BindOnce(barrier->CreateBarrierCallbackForType(
                                            OsHookType::kProtocolHandlers),
                                        Result::kOk));
}

void OsIntegrationManager::GetAppExistingShortCutLocation(
    ShortcutLocationCallback callback,
    std::unique_ptr<ShortcutInfo> shortcut_info) {
  CHECK(shortcut_manager_);
  shortcut_manager_->GetAppExistingShortCutLocation(std::move(callback),
                                                    std::move(shortcut_info));
}

void OsIntegrationManager::GetShortcutInfoForApp(
    const webapps::AppId& app_id,
    WebAppShortcutManager::GetShortcutInfoCallback callback) {
  CHECK(shortcut_manager_);
  return shortcut_manager_->GetShortcutInfoForApp(app_id, std::move(callback));
}

bool OsIntegrationManager::IsFileHandlingAPIAvailable(
    const webapps::AppId& app_id) {
  return true;
}

const apps::FileHandlers* OsIntegrationManager::GetEnabledFileHandlers(
    const webapps::AppId& app_id) const {
  CHECK(file_handler_manager_);
  return file_handler_manager_->GetEnabledFileHandlers(app_id);
}

absl::optional<GURL> OsIntegrationManager::TranslateProtocolUrl(
    const webapps::AppId& app_id,
    const GURL& protocol_url) {
  if (!protocol_handler_manager_)
    return absl::optional<GURL>();

  return protocol_handler_manager_->TranslateProtocolUrl(app_id, protocol_url);
}

std::vector<custom_handlers::ProtocolHandler>
OsIntegrationManager::GetAppProtocolHandlers(const webapps::AppId& app_id) {
  if (!protocol_handler_manager_)
    return std::vector<custom_handlers::ProtocolHandler>();

  return protocol_handler_manager_->GetAppProtocolHandlers(app_id);
}

std::vector<custom_handlers::ProtocolHandler>
OsIntegrationManager::GetAllowedHandlersForProtocol(
    const std::string& protocol) {
  if (!protocol_handler_manager_)
    return std::vector<custom_handlers::ProtocolHandler>();

  return protocol_handler_manager_->GetAllowedHandlersForProtocol(protocol);
}

std::vector<custom_handlers::ProtocolHandler>
OsIntegrationManager::GetDisallowedHandlersForProtocol(
    const std::string& protocol) {
  if (!protocol_handler_manager_)
    return std::vector<custom_handlers::ProtocolHandler>();

  return protocol_handler_manager_->GetDisallowedHandlersForProtocol(protocol);
}

WebAppShortcutManager& OsIntegrationManager::shortcut_manager_for_testing() {
  CHECK(shortcut_manager_);
  return *shortcut_manager_;
}

UrlHandlerManager& OsIntegrationManager::url_handler_manager_for_testing() {
  CHECK(url_handler_manager_);
  return *url_handler_manager_;
}

WebAppProtocolHandlerManager&
OsIntegrationManager::protocol_handler_manager_for_testing() {
  CHECK(protocol_handler_manager_);
  return *protocol_handler_manager_;
}

FakeOsIntegrationManager* OsIntegrationManager::AsTestOsIntegrationManager() {
  return nullptr;
}

void OsIntegrationManager::CreateShortcuts(const webapps::AppId& app_id,
                                           bool add_to_desktop,
                                           ShortcutCreationReason reason,
                                           CreateShortcutsCallback callback) {
  if (shortcut_manager_->CanCreateShortcuts()) {
    shortcut_manager_->CreateShortcuts(app_id, add_to_desktop, reason,
                                       std::move(callback));
  } else {
    std::move(callback).Run(false);
  }
}

void OsIntegrationManager::RegisterFileHandlers(const webapps::AppId& app_id,
                                                ResultCallback callback) {
  CHECK(file_handler_manager_);
  ResultCallback metrics_callback =
      base::BindOnce([](Result result) {
        base::UmaHistogramBoolean("WebApp.FileHandlersRegistration.Result",
                                  (result == Result::kOk));
        return result;
      }).Then(std::move(callback));

  file_handler_manager_->EnableAndRegisterOsFileHandlers(
      app_id, std::move(metrics_callback));
}

void OsIntegrationManager::RegisterProtocolHandlers(
    const webapps::AppId& app_id,
    ResultCallback callback) {
  if (!protocol_handler_manager_) {
    std::move(callback).Run(Result::kOk);
    return;
  }

  protocol_handler_manager_->RegisterOsProtocolHandlers(app_id,
                                                        std::move(callback));
}

void OsIntegrationManager::RegisterUrlHandlers(const webapps::AppId& app_id,
                                               ResultCallback callback) {
  if (!url_handler_manager_) {
    std::move(callback).Run(Result::kOk);
    return;
  }

  url_handler_manager_->RegisterUrlHandlers(app_id, std::move(callback));
}

void OsIntegrationManager::RegisterShortcutsMenu(
    const webapps::AppId& app_id,
    const std::vector<WebAppShortcutsMenuItemInfo>& shortcuts_menu_item_infos,
    const ShortcutsMenuIconBitmaps& shortcuts_menu_icon_bitmaps,
    ResultCallback callback) {
  if (!ShouldRegisterShortcutsMenuWithOs()) {
    std::move(callback).Run(Result::kOk);
    return;
  }

  // Exit early if shortcuts_menu_item_infos are not populated.
  if (shortcuts_menu_item_infos.size() < 1) {
    std::move(callback).Run(Result::kOk);
    return;
  }

  ResultCallback metrics_callback =
      base::BindOnce([](Result result) {
        base::UmaHistogramBoolean("WebApp.ShortcutsMenuRegistration.Result",
                                  (result == Result::kOk));
        return result;
      }).Then(std::move(callback));

  CHECK(shortcut_manager_);
  shortcut_manager_->RegisterShortcutsMenuWithOs(
      app_id, shortcuts_menu_item_infos, shortcuts_menu_icon_bitmaps,
      std::move(metrics_callback));
}

void OsIntegrationManager::ReadAllShortcutsMenuIconsAndRegisterShortcutsMenu(
    const webapps::AppId& app_id,
    ResultCallback callback) {
  if (!ShouldRegisterShortcutsMenuWithOs()) {
    std::move(callback).Run(Result::kOk);
    return;
  }

  std::vector<WebAppShortcutsMenuItemInfo> shortcuts_menu_item_infos =
      provider_->registrar_unsafe().GetAppShortcutsMenuItemInfos(app_id);

  // Exit early if shortcuts_menu_item_infos are not populated.
  if (shortcuts_menu_item_infos.size() < 1) {
    std::move(callback).Run(Result::kOk);
    return;
  }

  ResultCallback metrics_callback =
      base::BindOnce([](Result result) {
        base::UmaHistogramBoolean("WebApp.ShortcutsMenuRegistration.Result",
                                  (result == Result::kOk));
        return result;
      }).Then(std::move(callback));

  shortcut_manager_->ReadAllShortcutsMenuIconsAndRegisterShortcutsMenu(
      app_id, shortcuts_menu_item_infos, std::move(metrics_callback));
}

void OsIntegrationManager::RegisterRunOnOsLogin(const webapps::AppId& app_id,
                                                ResultCallback callback) {
  ResultCallback metrics_callback =
      base::BindOnce([](Result result) {
        base::UmaHistogramBoolean("WebApp.RunOnOsLogin.Registration.Result",
                                  (result == Result::kOk));
        return result;
      }).Then(std::move(callback));

  GetShortcutInfoForApp(
      app_id,
      base::BindOnce(
          &OsIntegrationManager::OnShortcutInfoRetrievedRegisterRunOnOsLogin,
          weak_ptr_factory_.GetWeakPtr(), std::move(metrics_callback)));
}

void OsIntegrationManager::MacAppShimOnAppInstalledForProfile(
    const webapps::AppId& app_id) {
#if BUILDFLAG(IS_MAC)
  AppShimRegistry::Get()->OnAppInstalledForProfile(app_id, profile_->GetPath());
#endif
}

void OsIntegrationManager::AddAppToQuickLaunchBar(
    const webapps::AppId& app_id) {
  CHECK(provider_);
  if (provider_->ui_manager().CanAddAppToQuickLaunchBar()) {
    provider_->ui_manager().AddAppToQuickLaunchBar(app_id);
  }
}

void OsIntegrationManager::RegisterWebAppOsUninstallation(
    const webapps::AppId& app_id,
    const std::string& name) {
  if (ShouldRegisterUninstallationViaOsSettingsWithOs()) {
    RegisterUninstallationViaOsSettingsWithOs(app_id, name,
                                              profile_->GetPath());
  }
}

bool OsIntegrationManager::UnregisterShortcutsMenu(const webapps::AppId& app_id,
                                                   ResultCallback callback) {
  if (!ShouldRegisterShortcutsMenuWithOs()) {
    std::move(callback).Run(Result::kOk);
    return true;
  }

  ResultCallback metrics_callback =
      base::BindOnce([](Result result) {
        base::UmaHistogramBoolean("WebApp.ShortcutsMenuUnregistered.Result",
                                  (result == Result::kOk));
        return result;
      }).Then(std::move(callback));

  return UnregisterShortcutsMenuWithOs(app_id, profile_->GetPath(),
                                       std::move(metrics_callback));
}

void OsIntegrationManager::UnregisterRunOnOsLogin(const webapps::AppId& app_id,
                                                  ResultCallback callback) {
  ResultCallback metrics_callback =
      base::BindOnce([](Result result) {
        base::UmaHistogramBoolean("WebApp.RunOnOsLogin.Unregistration.Result",
                                  (result == Result::kOk));
        return result;
      }).Then(std::move(callback));

  ScheduleUnregisterRunOnOsLogin(
      *provider_, app_id, profile_->GetPath(),
      base::UTF8ToUTF16(provider_->registrar_unsafe().GetAppShortName(app_id)),
      std::move(metrics_callback));
}

void OsIntegrationManager::DeleteShortcuts(
    const webapps::AppId& app_id,
    const base::FilePath& shortcuts_data_dir,
    std::unique_ptr<ShortcutInfo> shortcut_info,
    ResultCallback callback) {
  if (shortcut_manager_->CanCreateShortcuts()) {
    auto shortcuts_callback = base::BindOnce(
        &OsIntegrationManager::OnShortcutsDeleted,
        weak_ptr_factory_.GetWeakPtr(), app_id, std::move(callback));

    shortcut_manager_->DeleteShortcuts(app_id, shortcuts_data_dir,
                                       std::move(shortcut_info),
                                       std::move(shortcuts_callback));
  } else {
    std::move(callback).Run(Result::kOk);
  }
}

void OsIntegrationManager::UnregisterFileHandlers(const webapps::AppId& app_id,
                                                  ResultCallback callback) {
  CHECK(file_handler_manager_);
  ResultCallback metrics_callback =
      base::BindOnce([](Result result) {
        base::UmaHistogramBoolean("WebApp.FileHandlersUnregistration.Result",
                                  (result == Result::kOk));
        return result;
      }).Then(std::move(callback));
  file_handler_manager_->DisableAndUnregisterOsFileHandlers(
      app_id, std::move(metrics_callback));
}

void OsIntegrationManager::UnregisterProtocolHandlers(
    const webapps::AppId& app_id,
    ResultCallback callback) {
  if (!protocol_handler_manager_) {
    std::move(callback).Run(Result::kOk);
    return;
  }

  protocol_handler_manager_->UnregisterOsProtocolHandlers(app_id,
                                                          std::move(callback));
}

void OsIntegrationManager::UnregisterUrlHandlers(const webapps::AppId& app_id) {
  if (!url_handler_manager_)
    return;

  url_handler_manager_->UnregisterUrlHandlers(app_id);
}

void OsIntegrationManager::UnregisterWebAppOsUninstallation(
    const webapps::AppId& app_id) {
  if (ShouldRegisterUninstallationViaOsSettingsWithOs()) {
    UnregisterUninstallationViaOsSettingsWithOs(app_id, profile_->GetPath());
  }
}

void OsIntegrationManager::UpdateShortcuts(const webapps::AppId& app_id,
                                           base::StringPiece old_name,
                                           ResultCallback callback) {
  // If the "Execute" step is enabled for sub-managers, then the 'old' os
  // integration path needs to be turned off so that os integration doesn't get
  // done twice.
  if (AreSubManagersExecuteEnabled()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), Result::kOk));
    return;
  }
  CHECK(shortcut_manager_);
  if (!shortcut_manager_->CanCreateShortcuts()) {
    std::move(callback).Run(Result::kOk);
    return;
  }

  ResultCallback metrics_callback =
      base::BindOnce([](Result result) {
        base::UmaHistogramBoolean("WebApp.Shortcuts.Update.Result",
                                  (result == Result::kOk));
        return result;
      }).Then(std::move(callback));

  shortcut_manager_->UpdateShortcuts(app_id, old_name,
                                     std::move(metrics_callback));
}

void OsIntegrationManager::UpdateShortcutsMenu(
    const webapps::AppId& app_id,
    const WebAppInstallInfo& web_app_info,
    ResultCallback callback) {
  if (web_app_info.shortcuts_menu_item_infos.empty()) {
    UnregisterShortcutsMenu(app_id, std::move(callback));
  } else {
    RegisterShortcutsMenu(app_id, web_app_info.shortcuts_menu_item_infos,
                          web_app_info.shortcuts_menu_icon_bitmaps,
                          std::move(callback));
  }
}

void OsIntegrationManager::UpdateUrlHandlers(
    const webapps::AppId& app_id,
    base::OnceCallback<void(bool success)> callback) {
  // If the "Execute" step is enabled for sub-managers, then the 'old' os
  // integration path needs to be turned off so that os integration doesn't get
  // done twice.
  if (AreSubManagersExecuteEnabled()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), true));
    return;
  }
  if (!url_handler_manager_)
    return;

  url_handler_manager_->UpdateUrlHandlers(app_id, std::move(callback));
}

void OsIntegrationManager::UpdateFileHandlers(
    const webapps::AppId& app_id,
    FileHandlerUpdateAction file_handlers_need_os_update,
    ResultCallback finished_callback) {
  // If the "Execute" step is enabled for sub-managers, then the 'old' os
  // integration path needs to be turned off so that os integration doesn't get
  // done twice.
  if (AreSubManagersExecuteEnabled()) {
    // Due to the way UpdateFileHandlerCommand is currently written, this needs
    // to be synchronously called on Mac.
#if BUILDFLAG(IS_MAC)
    std::move(finished_callback).Run(Result::kOk);
    return;
#else
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(finished_callback), Result::kOk));
    return;
#endif
  }
  if (file_handlers_need_os_update == FileHandlerUpdateAction::kNoUpdate) {
    std::move(finished_callback).Run(Result::kOk);
    return;
  }

  ResultCallback callback_after_removal;
  if (file_handlers_need_os_update == FileHandlerUpdateAction::kUpdate) {
    callback_after_removal = base::BindOnce(
        [](base::WeakPtr<OsIntegrationManager> os_integration_manager,
           const webapps::AppId& app_id, ResultCallback finished_callback,
           Result result) {
          if (!os_integration_manager) {
            std::move(finished_callback).Run(Result::kError);
            return;
          }
          os_integration_manager->RegisterFileHandlers(
              app_id, std::move(finished_callback));
        },
        weak_ptr_factory_.GetWeakPtr(), app_id, std::move(finished_callback));
  } else {
    CHECK_EQ(file_handlers_need_os_update, FileHandlerUpdateAction::kRemove);
    callback_after_removal = std::move(finished_callback);
  }

  // Update file handlers via complete uninstallation, then potential
  // reinstallation.
  UnregisterFileHandlers(app_id, std::move(callback_after_removal));
}

void OsIntegrationManager::UpdateProtocolHandlers(
    const webapps::AppId& app_id,
    bool force_shortcut_updates_if_needed,
    base::OnceClosure callback) {
  // If the "Execute" step is enabled for sub-managers, then the 'old' os
  // integration path needs to be turned off so that os integration doesn't get
  // done twice.
  if (AreSubManagersExecuteEnabled()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(callback));
    return;
  }

  if (!protocol_handler_manager_) {
    std::move(callback).Run();
    return;
  }

  auto shortcuts_callback = base::BindOnce(
      &OsIntegrationManager::OnShortcutsUpdatedForProtocolHandlers,
      weak_ptr_factory_.GetWeakPtr(), app_id, std::move(callback));

#if !BUILDFLAG(IS_WIN)
  // Windows handles protocol registration through the registry. For other
  // OS's we also need to regenerate the shortcut file before we call into
  // the OS. Since `UpdateProtocolHandlers` function is also called in
  // `UpdateOSHooks`, which also recreates the shortcuts, only do it if
  // required.
  if (force_shortcut_updates_if_needed) {
    UpdateShortcuts(app_id, "",
                    base::IgnoreArgs<Result>(std::move(shortcuts_callback)));
    return;
  }
#endif

  std::move(shortcuts_callback).Run();
}

void OsIntegrationManager::OnShortcutsUpdatedForProtocolHandlers(
    const webapps::AppId& app_id,
    base::OnceClosure update_finished_callback) {
  // Update protocol handlers via complete uninstallation, then reinstallation.
  ResultCallback unregister_callback = base::BindOnce(
      [](base::WeakPtr<OsIntegrationManager> os_integration_manager,
         const webapps::AppId& app_id,
         base::OnceClosure update_finished_callback, Result result) {
        // Re-register protocol handlers regardless of `result`.
        // TODO(https://crbug.com/1250728): Report a UMA metric when
        // unregistering fails, either here, or at the point of failure. This
        // might also mean we can remove `result`.
        if (!os_integration_manager) {
          std::move(update_finished_callback).Run();
          return;
        }

        os_integration_manager->RegisterProtocolHandlers(
            app_id,
            base::BindOnce(
                [](base::OnceClosure update_finished_callback, Result result) {
                  // TODO(https://crbug.com/1250728): Report
                  // |result| in an UMA metric.
                  std::move(update_finished_callback).Run();
                },
                std::move(update_finished_callback)));
      },
      weak_ptr_factory_.GetWeakPtr(), app_id,
      std::move(update_finished_callback));

  UnregisterProtocolHandlers(app_id, std::move(unregister_callback));
}

void OsIntegrationManager::SubManagersUnregistered(
    const webapps::AppId& app_id,
    std::unique_ptr<ScopedProfileKeepAlive> keep_alive) {
  force_unregister_callback_for_testing_.Run(app_id);
  keep_alive.reset();
}

void OsIntegrationManager::OnWebAppProfileWillBeDeleted(
    const webapps::AppId& app_id) {
  if (AreSubManagersExecuteEnabled()) {
    // This is used to keep the profile from being deleted while doing a
    // ForceUnregister when profile deletion is started.
    auto profile_keep_alive = std::make_unique<ScopedProfileKeepAlive>(
        profile_, ProfileKeepAliveOrigin::kOsIntegrationForceUnregistration);
    ForceUnregisterOsIntegrationOnSubManager(
        app_id, 0,
        base::BindOnce(&OsIntegrationManager::SubManagersUnregistered,
                       weak_ptr_factory_.GetWeakPtr(), app_id,
                       std::move(profile_keep_alive)));
  } else {
    UninstallAllOsHooks(app_id,
                        base::IgnoreArgs<OsHooksErrors>(base::BindOnce(
                            force_unregister_callback_for_testing_, app_id)));
  }
}

void OsIntegrationManager::OnAppRegistrarDestroyed() {
  registrar_observation_.Reset();
}

void OsIntegrationManager::SetForceUnregisterCalledForTesting(
    base::RepeatingCallback<void(const webapps::AppId&)> on_force_unregister) {
  force_unregister_callback_for_testing_ = on_force_unregister;
}

std::unique_ptr<ShortcutInfo> OsIntegrationManager::BuildShortcutInfo(
    const webapps::AppId& app_id) {
  CHECK(shortcut_manager_);
  return shortcut_manager_->BuildShortcutInfo(app_id);
}

void OsIntegrationManager::StartSubManagerExecutionIfRequired(
    const webapps::AppId& app_id,
    absl::optional<SynchronizeOsOptions> options,
    std::unique_ptr<proto::WebAppOsIntegrationState> desired_states,
    base::OnceClosure on_all_execution_done) {
  // This can never be a use-case where we execute OS integration registration/
  // unregistration but do not update the WebAppOsIntegrationState proto in the
  // web_app DB.
  CHECK(AreOsIntegrationSubManagersEnabled());

  // The "execute" step is skipped in the following cases:
  // 1. The app is no longer in the registrar. The whole synchronize process is
  //    stopped here.
  // 2. The `g_suppress_os_hooks_for_testing_` flag is set.
  // 3. Execution has been disabled by the feature parameters (see
  //    `AreSubManagersExecuteEnabled()`).

  const WebApp* web_app = provider_->registrar_unsafe().GetAppById(app_id);
  if (!web_app) {
    std::move(on_all_execution_done).Run();
    return;
  }

  proto::WebAppOsIntegrationState* desired_states_ptr = desired_states.get();
  auto write_state_to_db = base::BindOnce(
      &OsIntegrationManager::WriteStateToDB, weak_ptr_factory_.GetWeakPtr(),
      app_id, std::move(desired_states), std::move(on_all_execution_done));

  if (g_suppress_os_hooks_for_testing_ || !AreSubManagersExecuteEnabled()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(write_state_to_db));
    return;
  }

  ExecuteNextSubmanager(app_id, options, desired_states_ptr,
                        web_app->current_os_integration_states(), /*index=*/0,
                        std::move(write_state_to_db));
}

void OsIntegrationManager::ExecuteNextSubmanager(
    const webapps::AppId& app_id,
    absl::optional<SynchronizeOsOptions> options,
    proto::WebAppOsIntegrationState* desired_state,
    const proto::WebAppOsIntegrationState current_state,
    size_t index,
    base::OnceClosure on_all_execution_done_db_write) {
  CHECK(index < sub_managers_.size());
  base::OnceClosure next_callback = base::OnceClosure();
  if (index == sub_managers_.size() - 1) {
    next_callback = std::move(on_all_execution_done_db_write);
  } else {
    next_callback = base::BindOnce(
        &OsIntegrationManager::ExecuteNextSubmanager,
        weak_ptr_factory_.GetWeakPtr(), app_id, options, desired_state,
        current_state, index + 1, std::move(on_all_execution_done_db_write));
  }
  sub_managers_[index]->Execute(app_id, options, *desired_state, current_state,
                                std::move(next_callback));
}

void OsIntegrationManager::WriteStateToDB(
    const webapps::AppId& app_id,
    std::unique_ptr<proto::WebAppOsIntegrationState> desired_states,
    base::OnceClosure callback) {
  // Exit early if the app is scheduled to be uninstalled or is already
  // uninstalled.
  const WebApp* existing_app = provider_->registrar_unsafe().GetAppById(app_id);
  if (!existing_app || existing_app->is_uninstalling()) {
    std::move(callback).Run();
    return;
  }

  {
    ScopedRegistryUpdate update = provider_->sync_bridge_unsafe().BeginUpdate();
    WebApp* web_app = update->UpdateApp(app_id);
    CHECK(web_app);
    web_app->SetCurrentOsIntegrationStates(*desired_states.get());
  }

  std::move(callback).Run();
}

void OsIntegrationManager::ForceUnregisterOsIntegrationOnSubManager(
    const webapps::AppId& app_id,
    size_t index,
    base::OnceClosure final_callback) {
  CHECK(index < sub_managers_.size());
  base::OnceClosure next_callback = base::OnceClosure();
  if (index == sub_managers_.size() - 1) {
    next_callback = std::move(final_callback);
  } else {
    next_callback = base::BindOnce(
        &OsIntegrationManager::ForceUnregisterOsIntegrationOnSubManager,
        weak_ptr_factory_.GetWeakPtr(), app_id, index + 1,
        std::move(final_callback));
  }
  sub_managers_[index]->ForceUnregister(app_id, std::move(next_callback));
}

void OsIntegrationManager::OnShortcutsCreated(
    const webapps::AppId& app_id,
    std::unique_ptr<WebAppInstallInfo> web_app_info,
    InstallOsHooksOptions options,
    scoped_refptr<OsHooksBarrier> barrier,
    bool shortcuts_created) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(barrier);

  if (provider_ && !provider_->registrar_unsafe().GetAppById(app_id)) {
    return;
  }

  bool shortcut_creation_failure =
      !shortcuts_created && options.os_hooks[OsHookType::kShortcuts];
  if (shortcut_creation_failure)
    barrier->OnError(OsHookType::kShortcuts);

#if !BUILDFLAG(IS_MAC)
  // This step happens before shortcut creation on Mac.
  if (options.os_hooks[OsHookType::kFileHandlers]) {
    RegisterFileHandlers(app_id, barrier->CreateBarrierCallbackForType(
                                     OsHookType::kFileHandlers));
  }
#endif

  if (options.os_hooks[OsHookType::kProtocolHandlers]) {
    RegisterProtocolHandlers(app_id, barrier->CreateBarrierCallbackForType(
                                         OsHookType::kProtocolHandlers));
  }

  if (options.os_hooks[OsHookType::kUrlHandlers]) {
    RegisterUrlHandlers(app_id, barrier->CreateBarrierCallbackForType(
                                    OsHookType::kUrlHandlers));
  }

  if (options.os_hooks[OsHookType::kShortcuts] &&
      options.add_to_quick_launch_bar) {
    AddAppToQuickLaunchBar(app_id);
  }
  if (shortcuts_created && options.os_hooks[OsHookType::kShortcutsMenu]) {
    if (web_app_info) {
      RegisterShortcutsMenu(
          app_id, web_app_info->shortcuts_menu_item_infos,
          web_app_info->shortcuts_menu_icon_bitmaps,
          barrier->CreateBarrierCallbackForType(OsHookType::kShortcutsMenu));
    } else {
      ReadAllShortcutsMenuIconsAndRegisterShortcutsMenu(
          app_id,
          barrier->CreateBarrierCallbackForType(OsHookType::kShortcutsMenu));
    }
  }

  if (options.os_hooks[OsHookType::kRunOnOsLogin] &&
      base::FeatureList::IsEnabled(features::kDesktopPWAsRunOnOsLogin)) {
    // TODO(crbug.com/1091964): Implement Run on OS Login mode selection.
    // Currently it is set to be the default: RunOnOsLoginMode::kWindowed
    RegisterRunOnOsLogin(app_id, barrier->CreateBarrierCallbackForType(
                                     OsHookType::kRunOnOsLogin));
  }

  if (options.os_hooks[OsHookType::kUninstallationViaOsSettings]) {
    RegisterWebAppOsUninstallation(
        app_id,
        provider_ ? provider_->registrar_unsafe().GetAppShortName(app_id) : "");
  }
}

void OsIntegrationManager::OnShortcutsDeleted(const webapps::AppId& app_id,
                                              ResultCallback callback,
                                              Result result) {
#if BUILDFLAG(IS_MAC)
  bool delete_multi_profile_shortcuts =
      AppShimRegistry::Get()->OnAppUninstalledForProfile(app_id,
                                                         profile_->GetPath());
  if (delete_multi_profile_shortcuts) {
    internals::ScheduleDeleteMultiProfileShortcutsForApp(app_id,
                                                         std::move(callback));
  }
#else
  std::move(callback).Run(result);
#endif
}

void OsIntegrationManager::OnShortcutInfoRetrievedRegisterRunOnOsLogin(
    ResultCallback callback,
    std::unique_ptr<ShortcutInfo> info) {
  ScheduleRegisterRunOnOsLogin(&provider_->sync_bridge_unsafe(),
                               std::move(info), std::move(callback));
}

}  // namespace web_app
