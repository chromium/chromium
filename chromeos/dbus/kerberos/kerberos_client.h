// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_KERBEROS_KERBEROS_CLIENT_H_
#define CHROMEOS_DBUS_KERBEROS_KERBEROS_CLIENT_H_

#include "base/callback.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "chromeos/dbus/kerberos/kerberos_service.pb.h"
#include "dbus/object_proxy.h"

namespace dbus {
class Bus;
}

namespace chromeos {

// KerberosClient is used to communicate with the org.chromium.Kerberos
// service. All method should be called from the origin thread (UI thread) which
// initializes the DBusThreadManager instance.
class COMPONENT_EXPORT(KERBEROS) KerberosClient {
 public:
  using AddAccountCallback =
      base::OnceCallback<void(const kerberos::AddAccountResponse& response)>;
  using RemoveAccountCallback =
      base::OnceCallback<void(const kerberos::RemoveAccountResponse& response)>;
  using ClearAccountsCallback =
      base::OnceCallback<void(const kerberos::ClearAccountsResponse& response)>;
  using ListAccountsCallback =
      base::OnceCallback<void(const kerberos::ListAccountsResponse& response)>;
  using SetConfigCallback =
      base::OnceCallback<void(const kerberos::SetConfigResponse& response)>;
  using ValidateConfigCallback = base::OnceCallback<void(
      const kerberos::ValidateConfigResponse& response)>;
  using AcquireKerberosTgtCallback = base::OnceCallback<void(
      const kerberos::AcquireKerberosTgtResponse& response)>;
  using GetKerberosFilesCallback = base::OnceCallback<void(
      const kerberos::GetKerberosFilesResponse& response)>;
  using KerberosFilesChangedCallback =
      base::RepeatingCallback<void(const std::string& principal_name)>;
  using KerberosTicketExpiringCallback =
      base::RepeatingCallback<void(const std::string& principal_name)>;

  // Interface with testing functionality. Accessed through GetTestInterface(),
  // only implemented in the fake implementation.
  class TestInterface {
   public:
    // Sets the artificial delay for asynchronous function calls.
    // Should be set to 0 for tests.
    virtual void SetTaskDelay(base::TimeDelta delay) = 0;

    // Starts recording which functions are called.
    virtual void StartRecordingFunctionCalls() = 0;

    // Stops recording which functions are called and returns calls as a
    // comma separated list, e.g. "AddAccount,ListAccounts".
    virtual std::string StopRecordingAndGetRecordedFunctionCalls() = 0;

    // Returns the number of accounts currently saved.
    virtual std::size_t GetNumberOfAccounts() const = 0;

   protected:
    virtual ~TestInterface() {}
  };

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance if not already created.
  static void InitializeFake();

  // Destroys the global instance.
  static void Shutdown();

  // Returns the global instance which may be null if not initialized.
  static KerberosClient* Get();

  // Kerberos daemon D-Bus method calls. See org.chromium.Kerberos.xml and
  // kerberos_service.proto in Chromium OS code for the documentation of the
  // methods and request/response messages.
  virtual void AddAccount(const kerberos::AddAccountRequest& request,
                          AddAccountCallback callback) = 0;

  virtual void RemoveAccount(const kerberos::RemoveAccountRequest& request,
                             RemoveAccountCallback callback) = 0;

  virtual void ClearAccounts(const kerberos::ClearAccountsRequest& request,
                             ClearAccountsCallback callback) = 0;

  virtual void ListAccounts(const kerberos::ListAccountsRequest& request,
                            ListAccountsCallback callback) = 0;

  virtual void SetConfig(const kerberos::SetConfigRequest& request,
                         SetConfigCallback callback) = 0;

  virtual void ValidateConfig(const kerberos::ValidateConfigRequest& request,
                              ValidateConfigCallback callback) = 0;

  virtual void AcquireKerberosTgt(
      const kerberos::AcquireKerberosTgtRequest& request,
      int password_fd,
      AcquireKerberosTgtCallback callback) = 0;

  virtual void GetKerberosFiles(
      const kerberos::GetKerberosFilesRequest& request,
      GetKerberosFilesCallback callback) = 0;

  virtual void ConnectToKerberosFileChangedSignal(
      KerberosFilesChangedCallback callback) = 0;

  virtual void ConnectToKerberosTicketExpiringSignal(
      KerberosTicketExpiringCallback callback) = 0;

  // Returns an interface for testing (fake only), or returns nullptr.
  virtual TestInterface* GetTestInterface() = 0;

 protected:
  // Initialize/Shutdown should be used instead.
  KerberosClient();
  virtual ~KerberosClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(KerberosClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_KERBEROS_KERBEROS_CLIENT_H_
