// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_SMBPROVIDER_FAKE_SMB_PROVIDER_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_SMBPROVIDER_FAKE_SMB_PROVIDER_CLIENT_H_

#include <map>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/dbus/smbprovider/smb_provider_client.h"

namespace ash {

// A fake implementation of SmbProviderClient.
class COMPONENT_EXPORT(ASH_DBUS_SMBPROVIDER) FakeSmbProviderClient
    : public SmbProviderClient {
 public:
  FakeSmbProviderClient();
  explicit FakeSmbProviderClient(bool should_run_synchronously);

  FakeSmbProviderClient(const FakeSmbProviderClient&) = delete;
  FakeSmbProviderClient& operator=(const FakeSmbProviderClient&) = delete;

  ~FakeSmbProviderClient() override;

  // Adds an entry in the |netbios_parse_results_| map for <packetid,
  // hostnames>.
  void AddNetBiosPacketParsingForTesting(uint8_t packet_id,
                                         std::vector<std::string> hostnames);

  // chromeos::DBusClient override.
  void Init(dbus::Bus* bus) override;

  // SmbProviderClient override.
  void GetShares(const base::FilePath& server_url,
                 ReadDirectoryCallback callback) override;

  void SetupKerberos(const std::string& account_id,
                     SetupKerberosCallback callback) override;

  void ParseNetBiosPacket(const std::vector<uint8_t>& packet,
                          uint16_t transaction_id,
                          ParseNetBiosPacketCallback callback) override;

  base::WeakPtr<SmbProviderClient> AsWeakPtr() override;

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

  base::WeakPtrFactory<FakeSmbProviderClient> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_SMBPROVIDER_FAKE_SMB_PROVIDER_CLIENT_H_
