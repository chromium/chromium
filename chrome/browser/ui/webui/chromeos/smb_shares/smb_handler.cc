// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/smb_shares/smb_handler.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/ash/smb_client/smb_service_factory.h"
#include "chrome/browser/profiles/profile.h"

namespace chromeos {
namespace smb_dialog {

namespace {

smb_client::SmbService* GetSmbService(Profile* profile) {
  smb_client::SmbService* const service =
      smb_client::SmbServiceFactory::Get(profile);
  return service;
}

base::Value BuildShareList(const std::vector<smb_client::SmbUrl>& shares) {
  base::Value shares_list(base::Value::Type::LIST);
  for (const auto& share : shares) {
    shares_list.Append(base::Value(share.GetWindowsUNCString()));
  }
  return shares_list;
}

}  // namespace

SmbHandler::SmbHandler(Profile* profile,
                       UpdateCredentialsCallback update_cred_callback)
    : profile_(profile),
      update_cred_callback_(std::move(update_cred_callback)) {}

SmbHandler::~SmbHandler() = default;

void SmbHandler::RegisterMessages() {
  web_ui()->RegisterDeprecatedMessageCallback(
      "smbMount",
      base::BindRepeating(&SmbHandler::HandleSmbMount, base::Unretained(this)));

  web_ui()->RegisterDeprecatedMessageCallback(
      "startDiscovery", base::BindRepeating(&SmbHandler::HandleStartDiscovery,
                                            base::Unretained(this)));

  web_ui()->RegisterDeprecatedMessageCallback(
      "updateCredentials",
      base::BindRepeating(&SmbHandler::HandleUpdateCredentials,
                          base::Unretained(this)));
}

void SmbHandler::HandleSmbMount(const base::ListValue* args) {
  CHECK_EQ(8U, args->GetListDeprecated().size());

  std::string callback_id = args->GetListDeprecated()[0].GetString();
  std::string mount_url = args->GetListDeprecated()[1].GetString();
  std::string mount_name = args->GetListDeprecated()[2].GetString();
  std::string username = args->GetListDeprecated()[3].GetString();
  std::string password = args->GetListDeprecated()[4].GetString();
  bool use_kerberos = args->GetListDeprecated()[5].GetBool();
  bool should_open_file_manager_after_mount =
      args->GetListDeprecated()[6].GetBool();
  bool save_credentials = args->GetListDeprecated()[7].GetBool();

  smb_client::SmbService* const service = GetSmbService(profile_);
  if (!service) {
    return;
  }

  std::string display_name = mount_name.empty() ? mount_url : mount_name;

  auto mount_response =
      base::BindOnce(&SmbHandler::HandleSmbMountResponse,
                     weak_ptr_factory_.GetWeakPtr(), callback_id);
  auto mount_call = base::BindOnce(
      &smb_client::SmbService::Mount, base::Unretained(service), display_name,
      base::FilePath(mount_url), username, password, use_kerberos,
      should_open_file_manager_after_mount, save_credentials,
      std::move(mount_response));

  if (host_discovery_done_) {
    std::move(mount_call).Run();
  } else {
    stored_mount_call_ = std::move(mount_call);
  }
}

void SmbHandler::HandleSmbMountResponse(const std::string& callback_id,
                                        smb_client::SmbMountResult result) {
  AllowJavascript();
  ResolveJavascriptCallback(base::Value(callback_id),
                            base::Value(static_cast<int>(result)));
}

void SmbHandler::HandleStartDiscovery(const base::ListValue* args) {
  smb_client::SmbService* const service = GetSmbService(profile_);
  if (!service) {
    return;
  }

  service->GatherSharesInNetwork(
      base::BindOnce(&SmbHandler::HandleDiscoveryDone,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(&SmbHandler::HandleGatherSharesResponse,
                          weak_ptr_factory_.GetWeakPtr()));
}

void SmbHandler::HandleDiscoveryDone() {
  host_discovery_done_ = true;
  if (!stored_mount_call_.is_null()) {
    std::move(stored_mount_call_).Run();
  }
}

void SmbHandler::HandleGatherSharesResponse(
    const std::vector<smb_client::SmbUrl>& shares_gathered,
    bool done) {
  AllowJavascript();
  FireWebUIListener("on-shares-found", BuildShareList(shares_gathered),
                    base::Value(done));
}

void SmbHandler::HandleUpdateCredentials(const base::ListValue* args) {
  CHECK_EQ(3U, args->GetListDeprecated().size());

  std::string mount_id = args->GetListDeprecated()[0].GetString();
  std::string username = args->GetListDeprecated()[1].GetString();
  std::string password = args->GetListDeprecated()[2].GetString();

  DCHECK(update_cred_callback_);
  std::move(update_cred_callback_).Run(username, password);
}

}  // namespace smb_dialog
}  // namespace chromeos
