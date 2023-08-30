// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/smb_shares/smb_handler.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/values.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/ash/smb_client/smb_service_factory.h"
#include "chrome/browser/profiles/profile.h"

namespace ash::smb_dialog {

namespace {

smb_client::SmbService* GetSmbService(Profile* profile) {
  smb_client::SmbService* const service =
      smb_client::SmbServiceFactory::Get(profile);
  return service;
}

base::Value::List BuildShareList(
    const std::vector<smb_client::SmbUrl>& shares) {
  base::Value::List shares_list;
  for (const auto& share : shares) {
    shares_list.Append(share.GetWindowsUNCString());
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
  web_ui()->RegisterMessageCallback(
      "smbMount",
      base::BindRepeating(&SmbHandler::HandleSmbMount, base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "startDiscovery", base::BindRepeating(&SmbHandler::HandleStartDiscovery,
                                            base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "updateCredentials",
      base::BindRepeating(&SmbHandler::HandleUpdateCredentials,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "hasAnySmbMountedBefore",
      base::BindRepeating(&SmbHandler::HandleHasAnySmbMountedBefore,
                          base::Unretained(this)));
}

void SmbHandler::SetSmbServiceForTesting(smb_client::SmbService* smb_service) {
  CHECK(smb_service);
  test_smb_service_ = smb_service;
}

smb_client::SmbService* SmbHandler::GetLocalSmbService() {
  if (test_smb_service_) {
    return test_smb_service_;
  }
  return GetSmbService(profile_);
}

void SmbHandler::HandleSmbMount(const base::Value::List& args) {
  CHECK_EQ(8U, args.size());

  std::string callback_id = args[0].GetString();
  std::string mount_url = args[1].GetString();
  std::string mount_name = args[2].GetString();
  std::string username = args[3].GetString();
  std::string password = args[4].GetString();
  bool use_kerberos = args[5].GetBool();
  bool should_open_file_manager_after_mount = args[6].GetBool();
  bool save_credentials = args[7].GetBool();

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

void SmbHandler::HandleStartDiscovery(const base::Value::List& args) {
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

void SmbHandler::HandleHasAnySmbMountedBefore(const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  std::string callback_id = args[0].GetString();
  smb_client::SmbService* const service = GetLocalSmbService();

  AllowJavascript();

  if (!service) {
    // Return the default value false so no changes would take place on the
    // Settings page.
    ResolveJavascriptCallback(base::Value(callback_id), base::Value(false));
    return;
  }

  ResolveJavascriptCallback(base::Value(callback_id),
                            base::Value(service->IsAnySmbShareConfigured()));
}

void SmbHandler::HandleGatherSharesResponse(
    const std::vector<smb_client::SmbUrl>& shares_gathered,
    bool done) {
  AllowJavascript();
  FireWebUIListener("on-shares-found", BuildShareList(shares_gathered),
                    base::Value(done));
}

void SmbHandler::HandleUpdateCredentials(const base::Value::List& args) {
  CHECK_EQ(3U, args.size());

  std::string mount_id = args[0].GetString();
  std::string username = args[1].GetString();
  std::string password = args[2].GetString();

  DCHECK(update_cred_callback_);
  std::move(update_cred_callback_).Run(username, password);
}

}  // namespace ash::smb_dialog
