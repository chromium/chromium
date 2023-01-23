// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/file_handling_sub_manager.h"

#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/web_applications/proto/web_app_os_integration_state.pb.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_registrar.h"

namespace web_app {

FileHandlingSubManager::FileHandlingSubManager(WebAppRegistrar& registrar)
    : registrar_(registrar) {}

FileHandlingSubManager::~FileHandlingSubManager() = default;

void FileHandlingSubManager::Configure(
    const AppId& app_id,
    proto::WebAppOsIntegrationState& desired_state,
    base::OnceClosure configure_done) {
  DCHECK(!desired_state.has_file_handling());

  if (!registrar_->IsLocallyInstalled(app_id) ||
      registrar_->GetAppFileHandlerApprovalState(app_id) ==
          ApiApprovalState::kDisallowed) {
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

  std::move(configure_done).Run();
}

void FileHandlingSubManager::Start() {}

void FileHandlingSubManager::Shutdown() {}

void FileHandlingSubManager::Execute(
    const AppId& app_id,
    const absl::optional<SynchronizeOsOptions>& synchronize_options,
    const proto::WebAppOsIntegrationState& desired_state,
    const proto::WebAppOsIntegrationState& current_state,
    base::OnceClosure callback) {
  // Not implemented yet.
  std::move(callback).Run();
}

}  // namespace web_app
