// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/smb_shares/smb_handler.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/profiles/profile.h"

namespace chromeos {
namespace smb_dialog {

namespace {

smb_client::SmbService* GetSmbService(Profile* profile) {
  smb_client::SmbService* const service = smb_client::SmbService::Get(profile);
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

SmbHandler::SmbHandler(Profile* profile) : profile_(profile) {}

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
}

void SmbHandler::HandleSmbMount(const base::ListValue* args) {
  CHECK_EQ(8U, args->GetSize());
  std::string callback_id;
  CHECK(args->GetString(0, &callback_id));

  std::string mount_url;
  std::string mount_name;
  std::string username;
  std::string password;
  bool use_kerberos;
  bool should_open_file_manager_after_mount;
  bool save_credentials;
  CHECK(args->GetString(1, &mount_url));
  CHECK(args->GetString(2, &mount_name));
  CHECK(args->GetString(3, &username));
  CHECK(args->GetString(4, &password));
  CHECK(args->GetBoolean(5, &use_kerberos));
  CHECK(args->GetBoolean(6, &should_open_file_manager_after_mount));
  CHECK(args->GetBoolean(7, &save_credentials));

  smb_client::SmbService* const service = GetSmbService(profile_);
  if (!service) {
    return;
  }

  chromeos::file_system_provider::MountOptions mo;
  mo.display_name = mount_name.empty() ? mount_url : mount_name;
  mo.writable = true;

  auto mount_response =
      base::BindOnce(&SmbHandler::HandleSmbMountResponse,
                     weak_ptr_factory_.GetWeakPtr(), callback_id);
  auto mount_call =
      base::BindOnce(&smb_client::SmbService::Mount, base::Unretained(service),
                     mo, base::FilePath(mount_url), username, password,
                     use_kerberos, should_open_file_manager_after_mount,
                     save_credentials, std::move(mount_response));

  if (host_discovery_done_) {
    std::move(mount_call).Run();
  } else {
    stored_mount_call_ = std::move(mount_call);
  }
}

void SmbHandler::HandleSmbMountResponse(const std::string& callback_id,
                                        SmbMountResult result) {
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
  CHECK_EQ(3U, args->GetSize());

  int32_t mount_id;
  std::string username;
  std::string password;

  CHECK(args->GetInteger(0, &mount_id));
  CHECK(args->GetString(1, &username));
  CHECK(args->GetString(2, &password));

  smb_client::SmbService* const service = GetSmbService(profile_);
  if (!service) {
    return;
  }

  service->UpdateCredentials(mount_id, username, password);
}

}  // namespace smb_dialog
}  // namespace chromeos
