// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/authpolicy/fake_authpolicy_client.h"

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "components/account_id/account_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

namespace ash {
namespace {

constexpr char kCorrectMachineName[] = "machine_name";
constexpr char kCorrectUserName[] = "user@domain.com";
constexpr char kCorrectUserDomain[] = "domain.com";
constexpr char kAccountId[] = "user-account-id";
constexpr char kMachineDomain[] = "machine.domain";

}  // namespace

class FakeAuthPolicyClientTest : public ::testing::Test {
 public:
  FakeAuthPolicyClientTest() = default;

  FakeAuthPolicyClientTest(const FakeAuthPolicyClientTest&) = delete;
  FakeAuthPolicyClientTest& operator=(const FakeAuthPolicyClientTest&) = delete;

 protected:
  FakeAuthPolicyClient* authpolicy_client() {
    return FakeAuthPolicyClient::Get();
  }

  void SetUp() override {
    ::testing::Test::SetUp();
    SessionManagerClient::InitializeFakeInMemory();
    AuthPolicyClient::InitializeFake();
    authpolicy_client()->DisableOperationDelayForTesting();
  }

  void TearDown() override {
    AuthPolicyClient::Shutdown();
    SessionManagerClient::Shutdown();
  }

  void JoinAdDomain(const std::string& machine_name,
                    const std::string& username,
                    AuthPolicyClient::JoinCallback callback) {
    authpolicy::JoinDomainRequest request;
    request.set_machine_name(machine_name);
    request.set_user_principal_name(username);
    authpolicy_client()->JoinAdDomain(request, /* password_fd */ -1,
                                      std::move(callback));
  }

  void JoinAdDomainWithMachineDomain(const std::string& machine_name,
                                     const std::string& machine_domain,
                                     const std::string& username,
                                     AuthPolicyClient::JoinCallback callback) {
    authpolicy::JoinDomainRequest request;
    request.set_machine_name(machine_name);
    request.set_user_principal_name(username);
    request.set_machine_domain(machine_domain);
    authpolicy_client()->JoinAdDomain(request, /* password_fd */ -1,
                                      std::move(callback));
  }

  void AuthenticateUser(const std::string& username,
                        const std::string& account_id,
                        AuthPolicyClient::AuthCallback callback) {
    authpolicy::AuthenticateUserRequest request;
    request.set_user_principal_name(username);
    request.set_account_id(account_id);
    authpolicy_client()->AuthenticateUser(request, /* password_fd */ -1,
                                          std::move(callback));
  }

  void WaitForServiceToBeAvailable() {
    authpolicy_client()->WaitForServiceToBeAvailable(base::BindOnce(
        &FakeAuthPolicyClientTest::OnWaitForServiceToBeAvailableCalled,
        base::Unretained(this)));
  }

  void OnWaitForServiceToBeAvailableCalled(bool is_service_available) {
    EXPECT_TRUE(is_service_available);
    service_is_available_called_num_++;
  }

  void LockDevice() {
    install_attributes_.Get()->SetActiveDirectoryManaged("example.com",
                                                         "device_id");
  }

  int service_is_available_called_num_ = 0;

 private:
  ScopedStubInstallAttributes install_attributes_;
  base::test::SingleThreadTaskEnvironment task_environment_;
};

// Tests parsing machine name.
TEST_F(FakeAuthPolicyClientTest, JoinAdDomain_ParseMachineName) {
  authpolicy_client()->SetStarted(true);
  JoinAdDomain("correct_length1", kCorrectUserName,
               base::BindOnce(
                   [](authpolicy::ErrorType error, const std::string& domain) {
                     EXPECT_EQ(authpolicy::ERROR_NONE, error);
                     EXPECT_EQ(kCorrectUserDomain, domain);
                   }));
  JoinAdDomain("", kCorrectUserName,
               base::BindOnce(
                   [](authpolicy::ErrorType error, const std::string& domain) {
                     EXPECT_EQ(authpolicy::ERROR_INVALID_MACHINE_NAME, error);
                     EXPECT_TRUE(domain.empty());
                   }));
  JoinAdDomain("too_long_machine_name", kCorrectUserName,
               base::BindOnce(
                   [](authpolicy::ErrorType error, const std::string& domain) {
                     EXPECT_EQ(authpolicy::ERROR_MACHINE_NAME_TOO_LONG, error);
                     EXPECT_TRUE(domain.empty());
                   }));
  JoinAdDomain("invalid:name", kCorrectUserName,
               base::BindOnce(
                   [](authpolicy::ErrorType error, const std::string& domain) {
                     EXPECT_EQ(authpolicy::ERROR_INVALID_MACHINE_NAME, error);
                     EXPECT_TRUE(domain.empty());
                   }));
  base::RunLoop loop;
  JoinAdDomain(">nvalidname", kCorrectUserName,
               base::BindOnce(
                   [](base::OnceClosure closure, authpolicy::ErrorType error,
                      const std::string& domain) {
                     EXPECT_EQ(authpolicy::ERROR_INVALID_MACHINE_NAME, error);
                     EXPECT_TRUE(domain.empty());
                     std::move(closure).Run();
                   },
                   loop.QuitClosure()));
  loop.Run();
}

// Tests join to a different machine domain.
TEST_F(FakeAuthPolicyClientTest, JoinAdDomain_MachineDomain) {
  authpolicy_client()->SetStarted(true);
  JoinAdDomainWithMachineDomain(kCorrectMachineName, kMachineDomain,
                                kCorrectUserName,
                                base::BindOnce([](authpolicy::ErrorType error,
                                                  const std::string& domain) {
                                  EXPECT_EQ(authpolicy::ERROR_NONE, error);
                                  EXPECT_EQ(kMachineDomain, domain);
                                }));
  base::RunLoop loop;
  JoinAdDomainWithMachineDomain(
      kCorrectMachineName, "", kCorrectUserName,
      base::BindOnce(
          [](base::OnceClosure closure, authpolicy::ErrorType error,
             const std::string& domain) {
            EXPECT_EQ(authpolicy::ERROR_NONE, error);
            EXPECT_EQ(kCorrectUserDomain, domain);
            std::move(closure).Run();
          },
          loop.QuitClosure()));
  loop.Run();
}

// Tests parsing user name.
TEST_F(FakeAuthPolicyClientTest, JoinAdDomain_ParseUPN) {
  authpolicy_client()->SetStarted(true);
  JoinAdDomain(kCorrectMachineName, kCorrectUserName,
               base::BindOnce(
                   [](authpolicy::ErrorType error, const std::string& domain) {
                     EXPECT_EQ(authpolicy::ERROR_NONE, error);
                     EXPECT_EQ(kCorrectUserDomain, domain);
                   }));
  JoinAdDomain(kCorrectMachineName, "user",
               base::BindOnce(
                   [](authpolicy::ErrorType error, const std::string& domain) {
                     EXPECT_EQ(authpolicy::ERROR_PARSE_UPN_FAILED, error);
                     EXPECT_TRUE(domain.empty());
                   }));
  JoinAdDomain(kCorrectMachineName, "",
               base::BindOnce(
                   [](authpolicy::ErrorType error, const std::string& domain) {
                     EXPECT_EQ(authpolicy::ERROR_PARSE_UPN_FAILED, error);
                     EXPECT_TRUE(domain.empty());
                   }));
  JoinAdDomain(kCorrectMachineName, "user@",
               base::BindOnce(
                   [](authpolicy::ErrorType error, const std::string& domain) {
                     EXPECT_EQ(authpolicy::ERROR_PARSE_UPN_FAILED, error);
                     EXPECT_TRUE(domain.empty());
                   }));
  JoinAdDomain(kCorrectMachineName, "@realm",
               base::BindOnce(
                   [](authpolicy::ErrorType error, const std::string& domain) {
                     EXPECT_EQ(authpolicy::ERROR_PARSE_UPN_FAILED, error);
                     EXPECT_TRUE(domain.empty());
                   }));
  base::RunLoop loop;
  JoinAdDomain(kCorrectMachineName, "user@realm@com",
               base::BindOnce(
                   [](base::OnceClosure closure, authpolicy::ErrorType error,
                      const std::string& domain) {
                     EXPECT_EQ(authpolicy::ERROR_PARSE_UPN_FAILED, error);
                     EXPECT_TRUE(domain.empty());
                     std::move(closure).Run();
                   },
                   loop.QuitClosure()));
  loop.Run();
}

// Tests that fake server does not support legacy encryption types.
TEST_F(FakeAuthPolicyClientTest, JoinAdDomain_NotSupportedEncType) {
  authpolicy_client()->SetStarted(true);
  base::RunLoop loop;
  authpolicy::JoinDomainRequest request;
  request.set_machine_name(kCorrectMachineName);
  request.set_user_principal_name(kCorrectUserName);
  request.set_kerberos_encryption_types(
      authpolicy::KerberosEncryptionTypes::ENC_TYPES_LEGACY);
  authpolicy_client()->JoinAdDomain(
      request, /* password_fd */ -1,
      base::BindOnce(
          [](base::OnceClosure closure, authpolicy::ErrorType error,
             const std::string& domain) {
            EXPECT_EQ(authpolicy::ERROR_KDC_DOES_NOT_SUPPORT_ENCRYPTION_TYPE,
                      error);
            EXPECT_TRUE(domain.empty());
            std::move(closure).Run();
          },
          loop.QuitClosure()));
  loop.Run();
}

// Test AuthenticateUser.
TEST_F(FakeAuthPolicyClientTest, AuthenticateUser_ByAccountId) {
  authpolicy_client()->SetStarted(true);
  LockDevice();
  // Check that account_id do not change.
  AuthenticateUser(
      kCorrectUserName, kAccountId,
      base::BindOnce(
          [](authpolicy::ErrorType error,
             const authpolicy::ActiveDirectoryAccountInfo& account_info) {
            EXPECT_EQ(authpolicy::ERROR_NONE, error);
            EXPECT_EQ(kAccountId, account_info.account_id());
          }));
}

// Tests calls to not started authpolicyd fails.
TEST_F(FakeAuthPolicyClientTest, NotStartedAuthPolicyService) {
  JoinAdDomain(kCorrectMachineName, kCorrectUserName,
               base::BindOnce(
                   [](authpolicy::ErrorType error, const std::string& domain) {
                     EXPECT_EQ(authpolicy::ERROR_DBUS_FAILURE, error);
                     EXPECT_TRUE(domain.empty());
                   }));
  LockDevice();
  AuthenticateUser(
      kCorrectUserName, std::string() /* account_id */,
      base::BindOnce([](authpolicy::ErrorType error,
                        const authpolicy::ActiveDirectoryAccountInfo&) {
        EXPECT_EQ(authpolicy::ERROR_DBUS_FAILURE, error);
      }));
  authpolicy_client()->RefreshDevicePolicy(
      base::BindOnce([](authpolicy::ErrorType error) {
        EXPECT_EQ(authpolicy::ERROR_DBUS_FAILURE, error);
      }));
  base::RunLoop loop;
  authpolicy_client()->RefreshUserPolicy(
      AccountId::FromUserEmail(kCorrectUserName),
      base::BindOnce(
          [](base::OnceClosure closure, authpolicy::ErrorType error) {
            EXPECT_EQ(authpolicy::ERROR_DBUS_FAILURE, error);
            std::move(closure).Run();
          },
          loop.QuitClosure()));
  loop.Run();
}

// Tests RefreshDevicePolicy. On a not locked device it should cache policy. On
// a locked device it should send policy to session_manager.
TEST_F(FakeAuthPolicyClientTest, NotLockedDeviceCachesPolicy) {
  authpolicy_client()->SetStarted(true);
  authpolicy_client()->RefreshDevicePolicy(
      base::BindOnce([](authpolicy::ErrorType error) {
        EXPECT_EQ(authpolicy::ERROR_DEVICE_POLICY_CACHED_BUT_NOT_SENT, error);
      }));
  LockDevice();
  base::RunLoop loop;
  authpolicy_client()->RefreshDevicePolicy(base::BindOnce(
      [](base::OnceClosure closure, authpolicy::ErrorType error) {
        EXPECT_EQ(authpolicy::ERROR_NONE, error);
        std::move(closure).Run();
      },
      loop.QuitClosure()));
  loop.Run();
}

// Tests that RefreshDevicePolicy stores device policy in the session manager.
TEST_F(FakeAuthPolicyClientTest, RefreshDevicePolicyStoresPolicy) {
  authpolicy_client()->SetStarted(true);
  LockDevice();

  {
    // Call RefreshDevicePolicy.
    base::RunLoop loop;
    em::ChromeDeviceSettingsProto policy;
    policy.mutable_allow_new_users()->set_allow_new_users(true);
    authpolicy_client()->set_device_policy(policy);
    authpolicy_client()->RefreshDevicePolicy(base::BindOnce(
        [](base::OnceClosure closure, authpolicy::ErrorType error) {
          EXPECT_EQ(authpolicy::ERROR_NONE, error);
          std::move(closure).Run();
        },
        loop.QuitClosure()));
    loop.Run();
  }

  {
    // Retrieve device policy from the session manager.
    std::string response_blob;
    EXPECT_EQ(SessionManagerClient::RetrievePolicyResponseType::SUCCESS,
              SessionManagerClient::Get()->BlockingRetrieveDevicePolicy(
                  &response_blob));
    em::PolicyFetchResponse response;
    EXPECT_TRUE(response.ParseFromString(response_blob));
    EXPECT_TRUE(response.has_policy_data());

    em::PolicyData policy_data;
    EXPECT_TRUE(policy_data.ParseFromString(response.policy_data()));

    em::ChromeDeviceSettingsProto policy;
    EXPECT_TRUE(policy.ParseFromString(policy_data.policy_value()));
    EXPECT_TRUE(policy.has_allow_new_users());
    EXPECT_TRUE(policy.allow_new_users().allow_new_users());
  }
}

TEST_F(FakeAuthPolicyClientTest, WaitForServiceToBeAvailableCalled) {
  WaitForServiceToBeAvailable();
  WaitForServiceToBeAvailable();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, service_is_available_called_num_);
  authpolicy_client()->SetStarted(true);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, service_is_available_called_num_);
  WaitForServiceToBeAvailable();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(3, service_is_available_called_num_);
}

}  // namespace ash
