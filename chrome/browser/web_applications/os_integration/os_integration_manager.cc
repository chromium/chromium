// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"

#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include "base/atomic_ref_count.h"
#include "base/auto_reset.h"
#include "base/barrier_callback.h"
#include "base/barrier_closure.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
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
#include "chrome/browser/web_applications/os_integration/os_integration_test_override.h"
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

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/web_applications/app_shim_registry_mac.h"
#endif

namespace web_app {

namespace {
base::AtomicRefCount& GetSuppressCount() {
  static base::AtomicRefCount g_ref_count;
  return g_ref_count;
}
}  // namespace

OsIntegrationManager::ScopedSuppressForTesting::ScopedSuppressForTesting() {
// Creating OS hooks on ChromeOS doesn't write files to disk, so it's
// unnecessary to suppress and it provides better crash coverage.
#if !BUILDFLAG(IS_CHROMEOS)
  GetSuppressCount().Increment();
#endif
}

OsIntegrationManager::ScopedSuppressForTesting::~ScopedSuppressForTesting() {
#if !BUILDFLAG(IS_CHROMEOS)
  CHECK(!GetSuppressCount().IsZero());
  GetSuppressCount().Decrement();
#endif
}

bool OsIntegrationManager::AreOsHooksSuppressedForTesting() {
  return !GetSuppressCount().IsZero();
}

OsIntegrationManager::OsIntegrationManager(
    Profile* profile,
    std::unique_ptr<WebAppShortcutManager> shortcut_manager,
    std::unique_ptr<WebAppFileHandlerManager> file_handler_manager,
    std::unique_ptr<WebAppProtocolHandlerManager> protocol_handler_manager)
    : profile_(profile),
      shortcut_manager_(std::move(shortcut_manager)),
      file_handler_manager_(std::move(file_handler_manager)),
      protocol_handler_manager_(std::move(protocol_handler_manager)) {}

OsIntegrationManager::~OsIntegrationManager() = default;

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
    std::optional<SynchronizeOsOptions> options) {
  first_synchronize_called_ = true;

  // This is usually called to clean up OS integration states on the OS,
  // regardless of whether there are apps existing in the app registry or not.
  if (options.has_value() && options.value().force_unregister_os_integration) {
    CHECK_OS_INTEGRATION_ALLOWED();
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

std::optional<GURL> OsIntegrationManager::TranslateProtocolUrl(
    const webapps::AppId& app_id,
    const GURL& protocol_url) {
  if (!protocol_handler_manager_)
    return std::optional<GURL>();

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

WebAppProtocolHandlerManager&
OsIntegrationManager::protocol_handler_manager_for_testing() {
  CHECK(protocol_handler_manager_);
  return *protocol_handler_manager_;
}

FakeOsIntegrationManager* OsIntegrationManager::AsTestOsIntegrationManager() {
  return nullptr;
}

void OsIntegrationManager::OnWebAppProfileWillBeDeleted(
    const webapps::AppId& app_id) {
  // This is used to keep the profile from being deleted while doing a
  // ForceUnregister when profile deletion is started.
  auto profile_keep_alive = std::make_unique<ScopedProfileKeepAlive>(
      profile_, ProfileKeepAliveOrigin::kOsIntegrationForceUnregistration);
  ForceUnregisterOsIntegrationOnSubManager(
      app_id, 0,
      base::BindOnce(&OsIntegrationManager::SubManagersUnregistered,
                     weak_ptr_factory_.GetWeakPtr(), app_id,
                     std::move(profile_keep_alive)));
}

void OsIntegrationManager::OnAppRegistrarDestroyed() {
  registrar_observation_.Reset();
}

void OsIntegrationManager::SetForceUnregisterCalledForTesting(
    base::RepeatingCallback<void(const webapps::AppId&)> on_force_unregister) {
  force_unregister_callback_for_testing_ = on_force_unregister;
}

void OsIntegrationManager::StartSubManagerExecutionIfRequired(
    const webapps::AppId& app_id,
    std::optional<SynchronizeOsOptions> options,
    std::unique_ptr<proto::WebAppOsIntegrationState> desired_states,
    base::OnceClosure on_all_execution_done) {
  // The "execute" step is skipped in the following cases:
  // 1. The app is no longer in the registrar. The whole synchronize process is
  //    stopped here.
  // 2. The `g_suppress_os_hooks_for_testing_` flag is set.

  const WebApp* web_app = provider_->registrar_unsafe().GetAppById(app_id);
  if (!web_app) {
    std::move(on_all_execution_done).Run();
    return;
  }

  proto::WebAppOsIntegrationState* desired_states_ptr = desired_states.get();
  auto write_state_to_db = base::BindOnce(
      &OsIntegrationManager::WriteStateToDB, weak_ptr_factory_.GetWeakPtr(),
      app_id, std::move(desired_states), std::move(on_all_execution_done));

  if (AreOsHooksSuppressedForTesting()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(write_state_to_db));
    return;
  }

  CHECK_OS_INTEGRATION_ALLOWED();

  ExecuteNextSubmanager(app_id, options, desired_states_ptr,
                        web_app->current_os_integration_states(), /*index=*/0,
                        std::move(write_state_to_db));
}

void OsIntegrationManager::ExecuteNextSubmanager(
    const webapps::AppId& app_id,
    std::optional<SynchronizeOsOptions> options,
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
  // Exit early if the app is already uninstalled. We still need to write the
  // desired_states to the web_app DB during the uninstallation process since
  // that helps make decisions on whether the uninstallation went successfully
  // or not inside the RemoveWebAppJob.
  const WebApp* existing_app = provider_->registrar_unsafe().GetAppById(app_id);
  if (!existing_app) {
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

void OsIntegrationManager::SubManagersUnregistered(
    const webapps::AppId& app_id,
    std::unique_ptr<ScopedProfileKeepAlive> keep_alive) {
  force_unregister_callback_for_testing_.Run(app_id);
  keep_alive.reset();
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

}  // namespace web_app
