// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/install_attributes/install_attributes.h"

#include <memory>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/dbus/cryptohome/rpc.pb.h"
#include "chromeos/ash/components/dbus/device_management/install_attributes_util.h"
#include "chromeos/dbus/constants/dbus_paths.h"
#include "chromeos/dbus/tpm_manager/tpm_manager_client.h"
#include "components/policy/proto/install_attributes.pb.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

void CopyLockResult(base::RunLoop* loop,
                    InstallAttributes::LockResult* out,
                    InstallAttributes::LockResult result) {
  *out = result;
  loop->Quit();
}

}  // namespace

static const char kTestDomain[] = "example.com";
static const char kTestDeviceId[] = "133750519";

class InstallAttributesTest : public testing::Test {
 protected:
  InstallAttributesTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI,
                          base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(base::PathService::OverrideAndCreateIfNeeded(
        chromeos::dbus_paths::FILE_INSTALL_ATTRIBUTES, GetTempPath(), true,
        false));
    InstallAttributesClient::InitializeFake();
    chromeos::TpmManagerClient::InitializeFake();
    install_attributes_ =
        std::make_unique<InstallAttributes>(InstallAttributesClient::Get());
  }

  void TearDown() override {
    chromeos::TpmManagerClient::Shutdown();
    InstallAttributesClient::Shutdown();
  }

  base::FilePath GetTempPath() const {
    base::FilePath temp_path = base::MakeAbsoluteFilePath(temp_dir_.GetPath());
    return temp_path.Append("install_attrs_test");
  }

  void SetAttribute(
      cryptohome::SerializedInstallAttributes* install_attrs_proto,
      const std::string& name,
      const std::string& value) {
    cryptohome::SerializedInstallAttributes::Attribute* attribute;
    attribute = install_attrs_proto->add_attributes();
    attribute->set_name(name);
    attribute->set_value(value);
  }

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<InstallAttributes> install_attributes_;

  InstallAttributes::LockResult LockDeviceAndWaitForResult(
      policy::DeviceMode device_mode,
      const std::string& domain,
      const std::string& realm,
      const std::string& device_id) {
    base::RunLoop loop;
    InstallAttributes::LockResult result;
    install_attributes_->LockDevice(
        device_mode, domain, realm, device_id,
        base::BindOnce(&CopyLockResult, &loop, &result));
    loop.Run();
    return result;
  }
};

TEST_F(InstallAttributesTest, Lock) {
  EXPECT_EQ(
      InstallAttributes::LOCK_SUCCESS,
      LockDeviceAndWaitForResult(policy::DEVICE_MODE_ENTERPRISE, kTestDomain,
                                 std::string(),  // realm
                                 kTestDeviceId));
  EXPECT_EQ(chromeos::TpmManagerClient::Get()
                ->GetTestInterface()
                ->clear_stored_owner_password_count(),
            1);

  // Locking an already locked device should succeed if the parameters match.
  EXPECT_EQ(
      InstallAttributes::LOCK_SUCCESS,
      LockDeviceAndWaitForResult(policy::DEVICE_MODE_ENTERPRISE, kTestDomain,
                                 std::string(),  // realm
                                 kTestDeviceId));

  // But another domain should fail.
  EXPECT_EQ(InstallAttributes::LOCK_WRONG_DOMAIN,
            LockDeviceAndWaitForResult(policy::DEVICE_MODE_ENTERPRISE,
                                       "anotherexample.com",
                                       std::string(),  // realm
                                       kTestDeviceId));

  // A non-matching mode should fail as well.
  EXPECT_EQ(InstallAttributes::LOCK_WRONG_MODE,
            LockDeviceAndWaitForResult(policy::DEVICE_MODE_DEMO,
                                       kTestDomain,      // domain
                                       std::string(),    // realm
                                       kTestDeviceId));  // device id
}

TEST_F(InstallAttributesTest, IsEnterpriseManagedCloud) {
  install_attributes_->Init(GetTempPath());
  EXPECT_FALSE(install_attributes_->IsEnterpriseManaged());
  ASSERT_EQ(
      InstallAttributes::LOCK_SUCCESS,
      LockDeviceAndWaitForResult(policy::DEVICE_MODE_ENTERPRISE, kTestDomain,
                                 std::string(),  // realm
                                 kTestDeviceId));
  EXPECT_TRUE(install_attributes_->IsEnterpriseManaged());
  EXPECT_TRUE(install_attributes_->IsCloudManaged());
  EXPECT_FALSE(install_attributes_->IsDeviceInDemoMode());
}

TEST_F(InstallAttributesTest, IsEnterpriseManagedDemoMode) {
  install_attributes_->Init(GetTempPath());
  EXPECT_FALSE(install_attributes_->IsEnterpriseManaged());
  ASSERT_EQ(InstallAttributes::LOCK_SUCCESS,
            LockDeviceAndWaitForResult(policy::DEVICE_MODE_DEMO, kTestDomain,
                                       std::string(),  // realm
                                       kTestDeviceId));
  EXPECT_TRUE(install_attributes_->IsEnterpriseManaged());
  EXPECT_TRUE(install_attributes_->IsCloudManaged());
  EXPECT_TRUE(install_attributes_->IsDeviceInDemoMode());
}

TEST_F(InstallAttributesTest, GettersCloud) {
  install_attributes_->Init(GetTempPath());
  EXPECT_EQ(policy::DEVICE_MODE_PENDING, install_attributes_->GetMode());
  EXPECT_EQ(std::string(), install_attributes_->GetDomain());
  EXPECT_EQ(std::string(), install_attributes_->GetRealm());
  EXPECT_EQ(std::string(), install_attributes_->GetDeviceId());
  ASSERT_EQ(
      InstallAttributes::LOCK_SUCCESS,
      LockDeviceAndWaitForResult(policy::DEVICE_MODE_ENTERPRISE, kTestDomain,
                                 std::string(),  // realm
                                 kTestDeviceId));
  EXPECT_EQ(policy::DEVICE_MODE_ENTERPRISE, install_attributes_->GetMode());
  EXPECT_EQ(kTestDomain, install_attributes_->GetDomain());
  EXPECT_EQ(std::string(), install_attributes_->GetRealm());
  EXPECT_EQ(kTestDeviceId, install_attributes_->GetDeviceId());
  EXPECT_FALSE(install_attributes_->IsDeviceInDemoMode());
}

TEST_F(InstallAttributesTest, GettersDemoMode) {
  install_attributes_->Init(GetTempPath());
  EXPECT_EQ(policy::DEVICE_MODE_PENDING, install_attributes_->GetMode());
  EXPECT_EQ(std::string(), install_attributes_->GetDomain());
  EXPECT_EQ(std::string(), install_attributes_->GetRealm());
  EXPECT_EQ(std::string(), install_attributes_->GetDeviceId());
  ASSERT_EQ(InstallAttributes::LOCK_SUCCESS,
            LockDeviceAndWaitForResult(policy::DEVICE_MODE_DEMO, kTestDomain,
                                       std::string(),  // realm
                                       kTestDeviceId));
  EXPECT_EQ(policy::DEVICE_MODE_DEMO, install_attributes_->GetMode());
  EXPECT_EQ(kTestDomain, install_attributes_->GetDomain());
  EXPECT_EQ(std::string(), install_attributes_->GetRealm());
  EXPECT_EQ(kTestDeviceId, install_attributes_->GetDeviceId());
  EXPECT_TRUE(install_attributes_->IsDeviceInDemoMode());
}

TEST_F(InstallAttributesTest, ConsumerDevice) {
  install_attributes_->Init(GetTempPath());
  EXPECT_EQ(policy::DEVICE_MODE_PENDING, install_attributes_->GetMode());
  // Lock the attributes empty.
  ASSERT_TRUE(install_attributes_util::InstallAttributesFinalize());
  base::RunLoop loop;
  install_attributes_->ReadImmutableAttributes(loop.QuitClosure());
  loop.Run();

  ASSERT_FALSE(install_attributes_util::InstallAttributesIsFirstInstall());
  EXPECT_EQ(policy::DEVICE_MODE_CONSUMER, install_attributes_->GetMode());
  EXPECT_EQ(std::string(), install_attributes_->GetDomain());
  EXPECT_EQ(std::string(), install_attributes_->GetRealm());
  EXPECT_EQ(std::string(), install_attributes_->GetDeviceId());
  EXPECT_FALSE(install_attributes_->IsDeviceInDemoMode());
}

TEST_F(InstallAttributesTest, Init) {
  cryptohome::SerializedInstallAttributes install_attrs_proto;
  SetAttribute(&install_attrs_proto, InstallAttributes::kAttrEnterpriseOwned,
               "true");
  const std::string blob(install_attrs_proto.SerializeAsString());
  ASSERT_TRUE(base::WriteFile(GetTempPath(), blob));
  install_attributes_->Init(GetTempPath());
  EXPECT_EQ(policy::DEVICE_MODE_ENTERPRISE, install_attributes_->GetMode());
  EXPECT_EQ(std::string(), install_attributes_->GetDomain());
  EXPECT_EQ(std::string(), install_attributes_->GetRealm());
  EXPECT_EQ(std::string(), install_attributes_->GetDeviceId());
  EXPECT_FALSE(install_attributes_->IsDeviceInDemoMode());
}

TEST_F(InstallAttributesTest, InitForEnterpriseDemo) {
  cryptohome::SerializedInstallAttributes install_attrs_proto;
  SetAttribute(&install_attrs_proto, InstallAttributes::kAttrEnterpriseOwned,
               "true");
  SetAttribute(&install_attrs_proto, InstallAttributes::kAttrEnterpriseDomain,
               policy::kDemoModeDomain);
  const std::string blob(install_attrs_proto.SerializeAsString());
  ASSERT_TRUE(base::WriteFile(GetTempPath(), blob));
  install_attributes_->Init(GetTempPath());
  EXPECT_EQ(policy::DEVICE_MODE_ENTERPRISE, install_attributes_->GetMode());
  EXPECT_EQ(policy::kDemoModeDomain, install_attributes_->GetDomain());
  EXPECT_EQ(std::string(), install_attributes_->GetRealm());
  EXPECT_EQ(std::string(), install_attributes_->GetDeviceId());
  EXPECT_TRUE(install_attributes_->IsDeviceInDemoMode());
}

TEST_F(InstallAttributesTest, VerifyFakeInstallAttributesCache) {
  // This test verifies that
  // install_attributes_util::InstallAttributesFinalize() writes a cache that
  // InstallAttributes::Init accepts.

  // Verify that no attributes are initially set.
  install_attributes_->Init(GetTempPath());
  EXPECT_EQ(policy::DEVICE_MODE_PENDING, install_attributes_->GetMode());

  // Write test values.
  ASSERT_TRUE(install_attributes_util::InstallAttributesSet(
      InstallAttributes::kAttrEnterpriseOwned, "true"));
  ASSERT_TRUE(install_attributes_util::InstallAttributesFinalize());

  // Verify that InstallAttributes correctly decodes the stub cache file.
  install_attributes_->Init(GetTempPath());
  EXPECT_EQ(policy::DEVICE_MODE_ENTERPRISE, install_attributes_->GetMode());
  EXPECT_EQ(std::string(), install_attributes_->GetDomain());
  EXPECT_EQ(std::string(), install_attributes_->GetRealm());
  EXPECT_EQ(std::string(), install_attributes_->GetDeviceId());
}

TEST_F(InstallAttributesTest, CheckSetBlockDevmodeInTpm) {
  std::optional<::device_management::SetFirmwareManagementParametersReply>
      reply;
  install_attributes_->SetBlockDevmodeInTpm(
      true,
      base::BindOnce(
          [](std::optional<
                 ::device_management::SetFirmwareManagementParametersReply>*
                 reply_ptr,
             std::optional<
                 ::device_management::SetFirmwareManagementParametersReply>
                 reply) { *reply_ptr = reply; },
          &reply));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(reply.has_value());
  EXPECT_EQ(reply->error(), ::device_management::DeviceManagementErrorCode::
                                DEVICE_MANAGEMENT_ERROR_NOT_SET);
}

TEST_F(InstallAttributesTest, ConsistencyCheckTriggeredWithTpmPassword) {
  chromeos::TpmManagerClient::Get()
      ->GetTestInterface()
      ->mutable_nonsensitive_status_reply()
      ->set_is_owner_password_present(true);
  base::HistogramTester histogram_tester;
  install_attributes_->Init(GetTempPath());
  base::RunLoop().RunUntilIdle();

  // The expectation is "not locked, not cloud managed, and owner password not
  // wiped", which is mapped to "0".
  histogram_tester.ExpectUniqueSample("Enterprise.AttributesTPMConsistency", 0,
                                      1);
}

TEST_F(InstallAttributesTest, ConsistencyCheckTriggeredTpmPasswordWiped) {
  chromeos::TpmManagerClient::Get()
      ->GetTestInterface()
      ->mutable_nonsensitive_status_reply()
      ->set_is_owner_password_present(false);
  base::HistogramTester histogram_tester;
  install_attributes_->Init(GetTempPath());
  base::RunLoop().RunUntilIdle();

  // The expectation is "not locked, not cloud managed, and owner password
  // wiped", which is mapped to "4".
  histogram_tester.ExpectUniqueSample("Enterprise.AttributesTPMConsistency", 4,
                                      1);
}

TEST_F(InstallAttributesTest, ConsistencyCheckNotTriggeredDBusError) {
  chromeos::TpmManagerClient::Get()
      ->GetTestInterface()
      ->mutable_nonsensitive_status_reply()
      ->set_status(::tpm_manager::STATUS_DBUS_ERROR);
  base::HistogramTester histogram_tester;
  install_attributes_->Init(GetTempPath());
  // Fast-forward the timeline to virtually an infinite value in reality to make
  // sure retries get exhausted.
  task_environment_.FastForwardBy(base::Seconds(99999));

  // The expectation is "8" when TPM is not reachable.
  histogram_tester.ExpectUniqueSample("Enterprise.AttributesTPMConsistency", 8,
                                      1);
}

}  // namespace ash
