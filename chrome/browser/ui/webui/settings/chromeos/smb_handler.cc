// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/smb_handler.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace chromeos {
namespace settings {

namespace {

smb_client::SmbService* GetSmbService(Profile* profile) {
  smb_client::SmbService* const service = smb_client::SmbService::Get(profile);
  return service;
}

base::Value BuildShareList(const std::vector<smb_client::SmbUrl>& shares) {
  base::Value shares_list(base::Value::Type::LIST);
  for (const auto& share : shares) {
    shares_list.GetList().push_back(base::Value(share.GetWindowsUNCString()));
  }
  return shares_list;
}

}  // namespace

SmbHandler::SmbHandler(Profile* profile)
    : profile_(profile), weak_ptr_factory_(this) {}

SmbHandler::~SmbHandler() = default;

void SmbHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "smbMount",
      base::BindRepeating(&SmbHandler::HandleSmbMount, base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "startDiscovery", base::BindRepeating(&SmbHandler::HandleStartDiscovery,
                                            base::Unretained(this)));
}

void SmbHandler::HandleSmbMount(const base::ListValue* args) {
  CHECK_EQ(5U, args->GetSize());
  std::string mount_url;
  std::string mount_name;
  std::string username;
  std::string password;
  bool use_kerberos;
  CHECK(args->GetString(0, &mount_url));
  CHECK(args->GetString(1, &mount_name));
  CHECK(args->GetString(2, &username));
  CHECK(args->GetString(3, &password));
  CHECK(args->GetBoolean(4, &use_kerberos));

  smb_client::SmbService* const service = GetSmbService(profile_);
  if (!service) {
    return;
  }

  chromeos::file_system_provider::MountOptions mo;
  mo.display_name = mount_name.empty() ? mount_url : mount_name;
  mo.writable = true;

  auto mount_response = base::BindOnce(&SmbHandler::HandleSmbMountResponse,
                                       weak_ptr_factory_.GetWeakPtr());
  auto mount_call =
      base::BindOnce(&smb_client::SmbService::Mount, base::Unretained(service),
                     mo, base::FilePath(mount_url), username, password,
                     use_kerberos, std::move(mount_response));

  if (host_discovery_done_) {
    std::move(mount_call).Run();
  } else {
    stored_mount_call_ = std::move(mount_call);
  }
}

void SmbHandler::HandleSmbMountResponse(SmbMountResult result) {
  AllowJavascript();
  FireWebUIListener("on-add-smb-share", base::Value(static_cast<int>(result)));
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
    const std::vector<smb_client::SmbUrl>& shares_gathered) {
  AllowJavascript();
  FireWebUIListener("on-shares-found", BuildShareList(shares_gathered));
}


}  // namespace settings
}  // namespace chromeos
