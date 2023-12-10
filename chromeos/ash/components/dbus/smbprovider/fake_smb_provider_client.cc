// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/smbprovider/fake_smb_provider_client.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/single_thread_task_runner.h"

namespace ash {

namespace {

// Helper method to create a DirectoryEntryProto with |entry_name| and add it to
// |entry_list| as a directory.
void AddDirectoryEntryToList(smbprovider::DirectoryEntryListProto* entry_list,
                             const std::string& entry_name) {
  DCHECK(entry_list);

  smbprovider::DirectoryEntryProto* entry = entry_list->add_entries();
  entry->set_is_directory(true);
  entry->set_name(entry_name);
  entry->set_size(0);
  entry->set_last_modified_time(0);
}
}  // namespace

FakeSmbProviderClient::ShareResult::ShareResult() = default;

FakeSmbProviderClient::ShareResult::~ShareResult() = default;

FakeSmbProviderClient::FakeSmbProviderClient() {}

FakeSmbProviderClient::FakeSmbProviderClient(bool should_run_synchronously)
    : should_run_synchronously_(should_run_synchronously) {}

FakeSmbProviderClient::~FakeSmbProviderClient() {}

void FakeSmbProviderClient::AddNetBiosPacketParsingForTesting(
    uint8_t packet_id,
    std::vector<std::string> hostnames) {
  netbios_parse_results_[packet_id] = std::move(hostnames);
}

void FakeSmbProviderClient::Init(dbus::Bus* bus) {}

void FakeSmbProviderClient::GetShares(const base::FilePath& server_url,
                                      ReadDirectoryCallback callback) {
  smbprovider::DirectoryEntryListProto entry_list;

  smbprovider::ErrorType error = smbprovider::ErrorType::ERROR_OK;
  auto it = shares_.find(server_url.value());
  if (it != shares_.end()) {
    error = it->second.error;
    for (const std::string& share : it->second.shares) {
      AddDirectoryEntryToList(&entry_list, share);
    }
  }

  if (should_run_synchronously_) {
    std::move(callback).Run(error, entry_list);
  } else {
    stored_readdir_callback_ =
        base::BindOnce(std::move(callback), error, entry_list);
  }
}

void FakeSmbProviderClient::SetupKerberos(const std::string& account_id,
                                          SetupKerberosCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true /* success */));
}

void FakeSmbProviderClient::AddToShares(const std::string& server_url,
                                        const std::string& share) {
  shares_[server_url].shares.push_back(share);
}

void FakeSmbProviderClient::AddGetSharesFailure(const std::string& server_url,
                                                smbprovider::ErrorType error) {
  shares_[server_url].error = error;
}

void FakeSmbProviderClient::ParseNetBiosPacket(
    const std::vector<uint8_t>& packet,
    uint16_t transaction_id,
    ParseNetBiosPacketCallback callback) {
  std::vector<std::string> result;

  // For testing, we map a 1 byte packet to a vector<std::string> to simulate
  // parsing a list of hostnames from a packet.
  if (packet.size() == 1 && netbios_parse_results_.count(packet[0])) {
    result = netbios_parse_results_[packet[0]];
  }

  std::move(callback).Run(result);
}

base::WeakPtr<SmbProviderClient> FakeSmbProviderClient::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void FakeSmbProviderClient::ClearShares() {
  shares_.clear();
}

void FakeSmbProviderClient::RunStoredReadDirCallback() {
  std::move(stored_readdir_callback_).Run();
}

}  // namespace ash
