// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/file_handling_sub_manager.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/web_applications/os_integration/web_app_file_handler_registration.h"
#include "chrome/browser/web_applications/proto/web_app_os_integration_state.pb.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "components/services/app_service/public/cpp/file_handler.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#endif

namespace web_app {

namespace {

apps::FileHandlers ConvertFileHandlingProtoToFileHandlers(
    const proto::FileHandling file_handling_proto) {
  apps::FileHandlers file_handlers;
  for (const auto& file_handler_proto : file_handling_proto.file_handlers()) {
    apps::FileHandler file_handler;
    file_handler.action = GURL(file_handler_proto.action());
    DCHECK(file_handler.action.is_valid());
    file_handler.display_name =
        base::UTF8ToUTF16(file_handler_proto.display_name());
    for (const auto& accept_entry_proto : file_handler_proto.accept()) {
      apps::FileHandler::AcceptEntry accept_entry;
      accept_entry.mime_type = accept_entry_proto.mimetype();
      for (const auto& file_extension : accept_entry_proto.file_extensions()) {
        accept_entry.file_extensions.insert(file_extension);
      }
      file_handler.accept.push_back(std::move(accept_entry));
    }
    file_handlers.push_back(std::move(file_handler));
  }
  return file_handlers;
}

bool HasFileHandling(
    const proto::WebAppOsIntegrationState& os_integration_state) {
  return (os_integration_state.has_file_handling() &&
          os_integration_state.file_handling().file_handlers_size() > 0);
}

}  // namespace

std::set<std::string> GetFileExtensionsFromFileHandlingProto(
    const proto::FileHandling& file_handling) {
  std::set<std::string> file_extensions;
  for (const auto& file_handler : file_handling.file_handlers()) {
    for (const auto& accept_entry : file_handler.accept()) {
      for (const auto& extension : accept_entry.file_extensions()) {
        file_extensions.insert(extension);
      }
    }
  }

  return file_extensions;
}

std::set<std::string> GetMimeTypesFromFileHandlingProto(
    const proto::FileHandling& file_handling) {
  std::set<std::string> mime_types;
  for (const auto& file_handler : file_handling.file_handlers()) {
    for (const auto& accept_entry : file_handler.accept()) {
      mime_types.insert(accept_entry.mimetype());
    }
  }
  return mime_types;
}

FileHandlingSubManager::FileHandlingSubManager(
    const base::FilePath& profile_path,
    WebAppRegistrar& registrar,
    WebAppSyncBridge& sync_bridge)
    : profile_path_(profile_path),
      registrar_(registrar),
      sync_bridge_(sync_bridge) {}

FileHandlingSubManager::~FileHandlingSubManager() = default;

void FileHandlingSubManager::Configure(
    const AppId& app_id,
    proto::WebAppOsIntegrationState& desired_state,
    base::OnceClosure configure_done) {
  DCHECK(!desired_state.has_file_handling());

  if (!registrar_->IsLocallyInstalled(app_id) ||
      registrar_->GetAppFileHandlerApprovalState(app_id) ==
          ApiApprovalState::kDisallowed ||
      !ShouldRegisterFileHandlersWithOs()) {
    std::move(configure_done).Run();
    return;
  }

  proto::FileHandling* os_file_handling = desired_state.mutable_file_handling();

  // GetAppFileHandlers should never return a nullptr because of the registrar
  // checks above.
  for (const auto& file_handler : *registrar_->GetAppFileHandlers(app_id)) {
    proto::FileHandling::FileHandler* file_handler_proto =
        os_file_handling->add_file_handlers();
    DCHECK(file_handler.action.is_valid());
    file_handler_proto->set_action(file_handler.action.spec());
    file_handler_proto->set_display_name(
        base::UTF16ToUTF8(file_handler.display_name));

    for (const auto& accept_entry : file_handler.accept) {
      auto* accept_entry_proto = file_handler_proto->add_accept();
      accept_entry_proto->set_mimetype(accept_entry.mime_type);

      for (const auto& file_extension : accept_entry.file_extensions) {
        accept_entry_proto->add_file_extensions(file_extension);
      }
    }
  }

#if BUILDFLAG(IS_MAC)
  if (AreSubManagersExecuteEnabled()) {
    // Save file handlers data on `AppShimRegistry` to be used during
    // `ShortcutSubManager::Execute`.
    AppShimRegistry::Get()->SaveFileHandlersForAppAndProfile(
        app_id, profile_path_,
        GetFileExtensionsFromFileHandlingProto(desired_state.file_handling()),
        GetMimeTypesFromFileHandlingProto(desired_state.file_handling()));
  }
#endif

  std::move(configure_done).Run();
}

void FileHandlingSubManager::Execute(
    const AppId& app_id,
    const absl::optional<SynchronizeOsOptions>& synchronize_options,
    const proto::WebAppOsIntegrationState& desired_state,
    const proto::WebAppOsIntegrationState& current_state,
    base::OnceClosure callback) {
  if (!HasFileHandling(desired_state) && !HasFileHandling(current_state)) {
    std::move(callback).Run();
    return;
  }

  if (HasFileHandling(desired_state) && HasFileHandling(current_state) &&
      desired_state.file_handling().SerializeAsString() ==
          current_state.file_handling().SerializeAsString()) {
    std::move(callback).Run();
    return;
  }

  // All changes are generalized by first unregistering any existing file
  // handlers and then registering any desired file handlers.
  Unregister(app_id, desired_state, current_state,
             base::BindOnce(&FileHandlingSubManager::Register,
                            weak_ptr_factory_.GetWeakPtr(), app_id,
                            desired_state, std::move(callback)));
}

// TODO(b/279068663): Implement if needed.
void FileHandlingSubManager::ForceUnregister(const AppId& app_id,
                                             base::OnceClosure callback) {
  std::move(callback).Run();
}

void FileHandlingSubManager::Unregister(
    const AppId& app_id,
    const proto::WebAppOsIntegrationState& desired_state,
    const proto::WebAppOsIntegrationState& current_state,
    base::OnceClosure callback) {
  if (!HasFileHandling(current_state)) {
    std::move(callback).Run();
    return;
  }

  ResultCallback metrics_callback =
      base::BindOnce([](Result result) {
        base::UmaHistogramBoolean("WebApp.FileHandlersUnregistration.Result",
                                  (result == Result::kOk));
      }).Then(std::move(callback));

  // TODO(https://crbug.com/1295044): remove after fully deprecate old
  // `InstallOsHooks/UninstallOsHooks` paths.
  if (!HasFileHandling(desired_state)) {
    ScopedRegistryUpdate update(&sync_bridge_.get());
    update->UpdateApp(app_id)->SetFileHandlerOsIntegrationState(
        OsIntegrationState::kDisabled);
  }

  UnregisterFileHandlersWithOs(app_id, profile_path_,
                               std::move(metrics_callback));
}

void FileHandlingSubManager::Register(
    const AppId& app_id,
    const proto::WebAppOsIntegrationState& desired_state,
    base::OnceClosure callback) {
  if (!HasFileHandling(desired_state)) {
    std::move(callback).Run();
    return;
  }

  ResultCallback metrics_callback =
      base::BindOnce([](Result result) {
        base::UmaHistogramBoolean("WebApp.FileHandlersRegistration.Result",
                                  (result == Result::kOk));
      }).Then(std::move(callback));

  // TODO(https://crbug.com/1295044): remove after fully deprecate old
  // `InstallOsHooks/UninstallOsHooks` paths.
  {
    ScopedRegistryUpdate update(&sync_bridge_.get());
    update->UpdateApp(app_id)->SetFileHandlerOsIntegrationState(
        OsIntegrationState::kEnabled);
  }

  RegisterFileHandlersWithOs(
      app_id, registrar_->GetAppShortName(app_id), profile_path_,
      ConvertFileHandlingProtoToFileHandlers(desired_state.file_handling()),
      std::move(metrics_callback));
}

}  // namespace web_app
