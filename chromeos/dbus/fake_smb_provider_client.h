// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_SMB_PROVIDER_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_SMB_PROVIDER_CLIENT_H_

#include <map>
#include <string>
#include <vector>

#include "chromeos/dbus/smb_provider_client.h"

namespace chromeos {

// A fake implementation of SmbProviderClient.
class COMPONENT_EXPORT(CHROMEOS_DBUS) FakeSmbProviderClient
    : public SmbProviderClient {
 public:
  FakeSmbProviderClient();
  explicit FakeSmbProviderClient(bool should_run_synchronously);
  ~FakeSmbProviderClient() override;

  // Adds an entry in the |netbios_parse_results_| map for <packetid,
  // hostnames>.
  void AddNetBiosPacketParsingForTesting(uint8_t packet_id,
                                         std::vector<std::string> hostnames);

  // DBusClient override.
  void Init(dbus::Bus* bus) override;

  // SmbProviderClient override.
  void Mount(const base::FilePath& share_path,
             const MountOptions& options,
             base::ScopedFD password_fd,
             MountCallback callback) override;

  void Unmount(int32_t mount_id,
               bool remove_password,
               StatusCallback callback) override;
  void ReadDirectory(int32_t mount_id,
                     const base::FilePath& directory_path,
                     ReadDirectoryCallback callback) override;
  void GetMetadataEntry(int32_t mount_id,
                        const base::FilePath& entry_path,
                        GetMetdataEntryCallback callback) override;
  void OpenFile(int32_t mount_id,
                const base::FilePath& file_path,
                bool writeable,
                OpenFileCallback callback) override;
  void CloseFile(int32_t mount_id,
                 int32_t file_id,
                 StatusCallback callback) override;
  void ReadFile(int32_t mount_id,
                int32_t file_id,
                int64_t offset,
                int32_t length,
                ReadFileCallback callback) override;

  void DeleteEntry(int32_t mount_id,
                   const base::FilePath& entry_path,
                   bool recursive,
                   StatusCallback callback) override;

  void CreateFile(int32_t mount_id,
                  const base::FilePath& file_path,
                  StatusCallback callback) override;

  void Truncate(int32_t mount_id,
                const base::FilePath& file_path,
                int64_t length,
                StatusCallback callback) override;

  void WriteFile(int32_t mount_id,
                 int32_t file_id,
                 int64_t offset,
                 int32_t length,
                 base::ScopedFD temp_fd,
                 StatusCallback callback) override;

  void CreateDirectory(int32_t mount_id,
                       const base::FilePath& directory_path,
                       bool recursive,
                       StatusCallback callback) override;

  void MoveEntry(int32_t mount_id,
                 const base::FilePath& source_path,
                 const base::FilePath& target_path,
                 StatusCallback callback) override;

  void CopyEntry(int32_t mount_id,
                 const base::FilePath& source_path,
                 const base::FilePath& target_path,
                 StatusCallback callback) override;

  void GetDeleteList(int32_t mount_id,
                     const base::FilePath& entry_path,
                     GetDeleteListCallback callback) override;

  void GetShares(const base::FilePath& server_url,
                 ReadDirectoryCallback callback) override;

  void SetupKerberos(const std::string& account_id,
                     SetupKerberosCallback callback) override;

  void ParseNetBiosPacket(const std::vector<uint8_t>& packet,
                          uint16_t transaction_id,
                          ParseNetBiosPacketCallback callback) override;

  void StartCopy(int32_t mount_id,
                 const base::FilePath& source_path,
                 const base::FilePath& target_path,
                 StartCopyCallback callback) override;

  void ContinueCopy(int32_t mount_id,
                    int32_t copy_token,
                    StatusCallback callback) override;

  void StartReadDirectory(int32_t mount_id,
                          const base::FilePath& directory_path,
                          StartReadDirectoryCallback callback) override;

  void ContinueReadDirectory(int32_t mount_id,
                             int32_t read_dir_token,
                             ReadDirectoryCallback callback) override;

  void UpdateMountCredentials(int32_t mount_id,
                              std::string workgroup,
                              std::string username,
                              base::ScopedFD password_fd,
                              StatusCallback callback) override;

  void UpdateSharePath(int32_t mount_id,
                       const std::string& share_path,
                       StatusCallback callback) override;

  // Adds |share| to the list of shares for |server_url| in |shares_|.
  void AddToShares(const std::string& server_url, const std::string& share);

  // Adds a failure to get shares for |server_url|.
  void AddGetSharesFailure(const std::string& server_url,
                           smbprovider::ErrorType error);

  // Clears |shares_|.
  void ClearShares();

  // Runs |stored_callback_|.
  void RunStoredReadDirCallback();

 private:
  // Result of a GetShares() call.
  struct ShareResult {
    ShareResult();
    ~ShareResult();

    smbprovider::ErrorType error = smbprovider::ErrorType::ERROR_OK;
    std::vector<std::string> shares;
  };

  // Controls whether |stored_readdir_callback_| should run synchronously.
  bool should_run_synchronously_ = true;

  base::OnceClosure stored_readdir_callback_;

  std::map<uint8_t, std::vector<std::string>> netbios_parse_results_;

  // Mapping of a server url to its shares.
  std::map<std::string, ShareResult> shares_;

  DISALLOW_COPY_AND_ASSIGN(FakeSmbProviderClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_SMB_PROVIDER_CLIENT_H_
