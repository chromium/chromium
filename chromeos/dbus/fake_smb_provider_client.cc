// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_smb_provider_client.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/threading/thread_task_runner_handle.h"

namespace chromeos {

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

FakeSmbProviderClient::FakeSmbProviderClient() {}

FakeSmbProviderClient::~FakeSmbProviderClient() {}

void FakeSmbProviderClient::AddNetBiosPacketParsingForTesting(
    uint8_t packet_id,
    std::vector<std::string> hostnames) {
  netbios_parse_results_[packet_id] = std::move(hostnames);
}

void FakeSmbProviderClient::Init(dbus::Bus* bus) {}

void FakeSmbProviderClient::Mount(const base::FilePath& share_path,
                                  bool ntlm_enabled,
                                  const std::string& workgroup,
                                  const std::string& username,
                                  base::ScopedFD password_fd,
                                  MountCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), smbprovider::ERROR_OK, 1));
}

void FakeSmbProviderClient::Remount(const base::FilePath& share_path,
                                    int32_t mount_id,
                                    bool ntlm_enabled,
                                    const std::string& workgroup,
                                    const std::string& username,
                                    base::ScopedFD password_fd,
                                    StatusCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), smbprovider::ERROR_OK));
}

void FakeSmbProviderClient::Unmount(int32_t mount_id, StatusCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), smbprovider::ERROR_OK));
}

void FakeSmbProviderClient::ReadDirectory(int32_t mount_id,
                                          const base::FilePath& directory_path,
                                          ReadDirectoryCallback callback) {
  smbprovider::DirectoryEntryListProto entry_list;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), smbprovider::ERROR_OK, entry_list));
}

void FakeSmbProviderClient::GetMetadataEntry(int32_t mount_id,
                                             const base::FilePath& entry_path,
                                             GetMetdataEntryCallback callback) {
  smbprovider::DirectoryEntryProto entry;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), smbprovider::ERROR_OK, entry));
}

void FakeSmbProviderClient::OpenFile(int32_t mount_id,
                                     const base::FilePath& file_path,
                                     bool writeable,
                                     OpenFileCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), smbprovider::ERROR_OK, 1));
}

void FakeSmbProviderClient::CloseFile(int32_t mount_id,
                                      int32_t file_id,
                                      StatusCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), smbprovider::ERROR_OK));
}

void FakeSmbProviderClient::ReadFile(int32_t mount_id,
                                     int32_t file_id,
                                     int64_t offset,
                                     int32_t length,
                                     ReadFileCallback callback) {
  base::ScopedFD fd;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), smbprovider::ERROR_OK,
                                std::move(fd)));
}

void FakeSmbProviderClient::DeleteEntry(int32_t mount_id,
                                        const base::FilePath& entry_path,
                                        bool recursive,
                                        StatusCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), smbprovider::ERROR_OK));
}

void FakeSmbProviderClient::CreateFile(int32_t mount_id,
                                       const base::FilePath& file_path,
                                       StatusCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), smbprovider::ERROR_OK));
}

void FakeSmbProviderClient::Truncate(int32_t mount_id,
                                     const base::FilePath& file_path,
                                     int64_t length,
                                     StatusCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), smbprovider::ERROR_OK));
}

void FakeSmbProviderClient::WriteFile(int32_t mount_id,
                                      int32_t file_id,
                                      int64_t offset,
                                      int32_t length,
                                      base::ScopedFD temp_fd,
                                      StatusCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), smbprovider::ERROR_OK));
}

void FakeSmbProviderClient::CreateDirectory(
    int32_t mount_id,
    const base::FilePath& directory_path,
    bool recursive,
    StatusCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), smbprovider::ERROR_OK));
}

void FakeSmbProviderClient::MoveEntry(int32_t mount_id,
                                      const base::FilePath& source_path,
                                      const base::FilePath& target_path,
                                      StatusCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), smbprovider::ERROR_OK));
}

void FakeSmbProviderClient::CopyEntry(int32_t mount_id,
                                      const base::FilePath& source_path,
                                      const base::FilePath& target_path,
                                      StatusCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), smbprovider::ERROR_OK));
}

void FakeSmbProviderClient::GetDeleteList(int32_t mount_id,
                                          const base::FilePath& entry_path,
                                          GetDeleteListCallback callback) {
  smbprovider::DeleteListProto delete_list;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), smbprovider::ERROR_OK, delete_list));
}

void FakeSmbProviderClient::GetShares(const base::FilePath& server_url,
                                      ReadDirectoryCallback callback) {
  smbprovider::DirectoryEntryListProto entry_list;
  for (const std::string& share : shares_[server_url.value()]) {
    AddDirectoryEntryToList(&entry_list, share);
  }

  std::move(callback).Run(smbprovider::ERROR_OK, entry_list);
}

void FakeSmbProviderClient::SetupKerberos(const std::string& account_id,
                                          SetupKerberosCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true /* success */));
}

void FakeSmbProviderClient::AddToShares(const std::string& server_url,
                                        const std::string& share) {
  shares_[server_url].push_back(share);
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

void FakeSmbProviderClient::StartCopy(int32_t mount_id,
                                      const base::FilePath& source_path,
                                      const base::FilePath& target_path,
                                      StartCopyCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), smbprovider::ERROR_OK,
                                -1 /* copy_token */));
}

void FakeSmbProviderClient::ContinueCopy(int32_t mount_id,
                                         int32_t copy_token,
                                         StatusCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), smbprovider::ERROR_OK));
}

void FakeSmbProviderClient::StartReadDirectory(
    int32_t mount_id,
    const base::FilePath& directory_path,
    StartReadDirectoryCallback callback) {
  smbprovider::DirectoryEntryListProto entry_list;
  // Simulate a ReadDirectory that completes during the StartReadDirectory call.
  // read_dir_token is unset and error is set to ERROR_OK.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), smbprovider::ERROR_OK,
                                -1 /* read_dir_token */, entry_list));
}

void FakeSmbProviderClient::ContinueReadDirectory(
    int32_t mount_id,
    int32_t read_dir_token,
    ReadDirectoryCallback callback) {
  smbprovider::DirectoryEntryListProto entry_list;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), smbprovider::ERROR_OK, entry_list));
}

void FakeSmbProviderClient::ClearShares() {
  shares_.clear();
}

}  // namespace chromeos
