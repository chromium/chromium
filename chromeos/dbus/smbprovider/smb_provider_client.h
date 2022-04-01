// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_SMBPROVIDER_SMB_PROVIDER_CLIENT_H_
#define CHROMEOS_DBUS_SMBPROVIDER_SMB_PROVIDER_CLIENT_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/dbus/common/dbus_client.h"
#include "chromeos/dbus/smbprovider/directory_entry.pb.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

// SmbProviderClient is used to communicate with the org.chromium.SmbProvider
// service. All methods should be called from the origin thread (UI thread)
// which initializes the DBusThreadManager instance.
class COMPONENT_EXPORT(CHROMEOS_DBUS_SMBPROVIDER) SmbProviderClient
    : public DBusClient,
      public base::SupportsWeakPtr<SmbProviderClient> {
 public:
  using ReadDirectoryCallback = base::OnceCallback<void(
      smbprovider::ErrorType error,
      const smbprovider::DirectoryEntryListProto& entries)>;
  using StatusCallback = base::OnceCallback<void(smbprovider::ErrorType error)>;
  using SetupKerberosCallback = base::OnceCallback<void(bool success)>;
  using ParseNetBiosPacketCallback =
      base::OnceCallback<void(const std::vector<std::string>&)>;

  SmbProviderClient(const SmbProviderClient&) = delete;
  SmbProviderClient& operator=(const SmbProviderClient&) = delete;

  ~SmbProviderClient() override;

  // Factory function, creates a new instance and returns ownership.
  // For normal usage, access the singleton via DBusThreadManager::Get().
  static std::unique_ptr<SmbProviderClient> Create();

  // Calls GetShares. This gets the shares from |server_url| and calls
  // |callback| when shares are found. The DirectoryEntryListProto will contain
  // no entries if there are no shares found.
  virtual void GetShares(const base::FilePath& server_url,
                         ReadDirectoryCallback callback) = 0;

  // Calls SetupKerberos. This sets up Kerberos for the user |account_id|,
  // fetching the user's Kerberos files from AuthPolicy (if user is enrolled to
  // ChromAD) or Kerberos. If user is enrolled to ChromAD, |account_id| is
  // expected to be an object guid, otherwise it must be a principal name.
  virtual void SetupKerberos(const std::string& account_id,
                             SetupKerberosCallback callback) = 0;

  // Calls ParseNetBiosPacket. This parses the hostnames from a NetBios packet
  // |packet| and returns any hostnames described in the packet. Malformed
  // packets will return no hostnames.
  virtual void ParseNetBiosPacket(const std::vector<uint8_t>& packet,
                                  uint16_t transaction_id,
                                  ParseNetBiosPacketCallback callback) = 0;

 protected:
  // Create() should be used instead.
  SmbProviderClient();
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove when move to ash
namespace ash {
using ::chromeos::SmbProviderClient;
}  // namespace ash

#endif  // CHROMEOS_DBUS_SMBPROVIDER_SMB_PROVIDER_CLIENT_H_
