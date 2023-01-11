// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_AUTHPOLICY_AUTHPOLICY_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_AUTHPOLICY_AUTHPOLICY_CLIENT_H_

#include <string>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "chromeos/ash/components/dbus/authpolicy/active_directory_info.pb.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

class AccountId;

namespace ash {

// AuthPolicyClient is used to communicate with the org.chromium.AuthPolicy
// sevice. All method should be called from the origin thread (UI thread) which
// initializes the DBusThreadManager instance.
class COMPONENT_EXPORT(AUTHPOLICY) AuthPolicyClient {
 public:
  using AuthCallback = base::OnceCallback<void(
      authpolicy::ErrorType error,
      const authpolicy::ActiveDirectoryAccountInfo& account_info)>;
  using GetUserStatusCallback = base::OnceCallback<void(
      authpolicy::ErrorType error,
      const authpolicy::ActiveDirectoryUserStatus& user_status)>;
  using GetUserKerberosFilesCallback =
      base::OnceCallback<void(authpolicy::ErrorType error,
                              const authpolicy::KerberosFiles& kerberos_files)>;
  using JoinCallback =
      base::OnceCallback<void(authpolicy::ErrorType error,
                              const std::string& machine_domain)>;
  using RefreshPolicyCallback =
      base::OnceCallback<void(authpolicy::ErrorType error)>;

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance if not already created.
  static void InitializeFake();

  // Destroys the global instance which must have been initialized.
  static void Shutdown();

  // Returns the global instance if initialized. May return null.
  static AuthPolicyClient* Get();

  AuthPolicyClient(const AuthPolicyClient&) = delete;
  AuthPolicyClient& operator=(const AuthPolicyClient&) = delete;

  // Calls JoinADDomain to join a machine/device to an Active Directory domain.
  // Password is read from the |password_fd|. |callback| is called after getting
  // (or failing to get) D-BUS response.
  virtual void JoinAdDomain(const authpolicy::JoinDomainRequest& request,
                            int password_fd,
                            JoinCallback callback) = 0;

  // Calls AuthenticateUser to authenticate a user against Active Directory.
  // Password is read from the |password_fd|. |callback| is called after getting
  // (or failing to get) D-BUS response.
  virtual void AuthenticateUser(
      const authpolicy::AuthenticateUserRequest& request,
      int password_fd,
      AuthCallback callback) = 0;

  // Calls GetUserStatus. If Active Directory server is online it fetches
  // ActiveDirectoryUserStatus for the user specified by |request|.
  // |callback| is called after getting (or failing to get) D-Bus response.
  virtual void GetUserStatus(const authpolicy::GetUserStatusRequest& request,
                             GetUserStatusCallback callback) = 0;

  // Calls GetUserKerberosFiles. If authpolicyd has Kerberos files for the user
  // specified by |object_guid| it sends them in response: credentials cache and
  // krb5 config files.
  virtual void GetUserKerberosFiles(const std::string& object_guid,
                                    GetUserKerberosFilesCallback callback) = 0;

  // Calls RefreshDevicePolicy - handle policy for the device.
  // Fetch GPO files from Active directory server, parse it, encode it into
  // protobuf and send to SessionManager. Callback is called after that.
  virtual void RefreshDevicePolicy(RefreshPolicyCallback callback) = 0;

  // Calls RefreshUserPolicy - handle policy for the user specified by
  // |account_id|. Similar to RefreshDevicePolicy.
  virtual void RefreshUserPolicy(const AccountId& account_id,
                                 RefreshPolicyCallback callback) = 0;

  // Connects callbacks to D-Bus signal |signal_name| sent by authpolicyd.
  virtual void ConnectToSignal(
      const std::string& signal_name,
      dbus::ObjectProxy::SignalCallback signal_callback,
      dbus::ObjectProxy::OnConnectedCallback on_connected_callback) = 0;

  virtual void WaitForServiceToBeAvailable(
      dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback) = 0;

 protected:
  // Initialize/Shutdown should be used instead.
  AuthPolicyClient();
  virtual ~AuthPolicyClient();
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_AUTHPOLICY_AUTHPOLICY_CLIENT_H_
