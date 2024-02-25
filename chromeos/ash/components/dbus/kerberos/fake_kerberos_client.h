// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_KERBEROS_FAKE_KERBEROS_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_KERBEROS_FAKE_KERBEROS_CLIENT_H_

#include <memory>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include "chromeos/ash/components/dbus/kerberos/kerberos_client.h"
#include "chromeos/ash/components/dbus/kerberos/kerberos_service.pb.h"
#include "dbus/object_proxy.h"

namespace ash {

class COMPONENT_EXPORT(CHROMEOS_DBUS) FakeKerberosClient
    : public KerberosClient,
      public KerberosClient::TestInterface {
 public:
  FakeKerberosClient();

  FakeKerberosClient(const FakeKerberosClient&) = delete;
  FakeKerberosClient& operator=(const FakeKerberosClient&) = delete;

  ~FakeKerberosClient() override;

  // KerberosClient:
  void AddAccount(const kerberos::AddAccountRequest& request,
                  AddAccountCallback callback) override;
  void RemoveAccount(const kerberos::RemoveAccountRequest& request,
                     RemoveAccountCallback callback) override;
  void ClearAccounts(const kerberos::ClearAccountsRequest& request,
                     ClearAccountsCallback callback) override;
  void ListAccounts(const kerberos::ListAccountsRequest& request,
                    ListAccountsCallback callback) override;
  void SetConfig(const kerberos::SetConfigRequest& request,
                 SetConfigCallback callback) override;
  void ValidateConfig(const kerberos::ValidateConfigRequest& request,
                      ValidateConfigCallback callback) override;
  void AcquireKerberosTgt(const kerberos::AcquireKerberosTgtRequest& request,
                          int password_fd,
                          AcquireKerberosTgtCallback callback) override;
  void GetKerberosFiles(const kerberos::GetKerberosFilesRequest& request,
                        GetKerberosFilesCallback callback) override;
  base::CallbackListSubscription SubscribeToKerberosFileChangedSignal(
      KerberosFilesChangedCallback callback) override;
  base::CallbackListSubscription SubscribeToKerberosTicketExpiringSignal(
      KerberosTicketExpiringCallback callback) override;
  KerberosClient::TestInterface* GetTestInterface() override;

  // KerberosClient::TestInterface:
  void SetTaskDelay(base::TimeDelta delay) override;
  void StartRecordingFunctionCalls() override;
  std::string StopRecordingAndGetRecordedFunctionCalls() override;
  std::size_t GetNumberOfAccounts() const override;
  void SetSimulatedNumberOfNetworkFailures(int number_of_failures) override;

 private:
  using RepeatedAccountField =
      google::protobuf::RepeatedPtrField<kerberos::Account>;

  struct AccountData {
    // User principal (user@EXAMPLE.COM) that identifies this account.
    std::string principal_name;

    // Kerberos configuration file.
    std::string krb5conf;

    // True if AcquireKerberosTgt succeeded.
    bool has_tgt = false;

    // True if the account was added by policy.
    bool is_managed = false;

    // True if login password was used during last AcquireKerberosTgt() call.
    bool use_login_password = false;

    // Remembered password, if any.
    std::string password;

    explicit AccountData(const std::string& principal_name);
    AccountData(const AccountData& other);
    AccountData& operator=(const AccountData& other);

    // Only compares principal_name. For finding and erasing in vectors.
    bool operator==(const AccountData& other) const;
    bool operator!=(const AccountData& other) const;
  };

  enum class WhatToRemove { kNothing, kPassword, kAccount };

  // Determines what data to remove, depending on |mode| and |data|.
  static WhatToRemove DetermineWhatToRemove(kerberos::ClearMode mode,
                                            const AccountData& data);

  // Returns the AccountData for |principal_name| if available or nullptr
  // otherwise.
  AccountData* GetAccountData(const std::string& principal_name);

  // Appends |function_name| to |recorded_function_calls_| if the latter is set.
  void MaybeRecordFunctionCallForTesting(const char* function_name);

  // Maps the list of account data into the given proto repeated field.
  void MapAccountData(RepeatedAccountField* accounts);

  // List of account data.
  std::vector<AccountData> accounts_;

  // For recording which methods have been called (for testing).
  std::optional<std::string> recorded_function_calls_;

  // Fake delay for any asynchronous operation.
  base::TimeDelta task_delay_ = base::Milliseconds(100);

  // The simulated number of network failures on |AcquireKerberosTgt()| (for
  // testing).
  int simulated_number_of_network_failures_ = 0;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_KERBEROS_FAKE_KERBEROS_CLIENT_H_
