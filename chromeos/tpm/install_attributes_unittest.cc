// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/tpm/install_attributes.h"

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chromeos/dbus/constants/dbus_paths.h"
#include "chromeos/dbus/cryptohome/cryptohome_client.h"
#include "chromeos/dbus/cryptohome/rpc.pb.h"
#include "chromeos/dbus/cryptohome/tpm_util.h"
#include "components/policy/proto/install_attributes.pb.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

void CopyLockResult(base::RunLoop* loop,
                    InstallAttributes::LockResult* out,
                    InstallAttributes::LockResult result) {
  *out = result;
  loop->Quit();
}

}  // namespace

static const char kTestDomain[] = "example.com";
static const char kTestRealm[] = "realm.example.com";
static const char kTestDeviceId[] = "133750519";
static const char kTestUserDeprecated[] = "test@example.com";

class InstallAttributesTest : public testing::Test {
 protected:
  InstallAttributesTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI) {}

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(base::PathService::OverrideAndCreateIfNeeded(
        dbus_paths::FILE_INSTALL_ATTRIBUTES, GetTempPath(), true, false));
    CryptohomeClient::InitializeFake();
    install_attributes_ =
        std::make_unique<InstallAttributes>(CryptohomeClient::Get());
  }

  void TearDown() override { CryptohomeClient::Shutdown(); }

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
  {
    base::HistogramTester histogram_tester;
    EXPECT_EQ(
        InstallAttributes::LOCK_SUCCESS,
        LockDeviceAndWaitForResult(policy::DEVICE_MODE_ENTERPRISE, kTestDomain,
                                   std::string(),  // realm
                                   kTestDeviceId));
    histogram_tester.ExpectUniqueSample(
        "Enterprise.ExistingInstallAttributesLock", 0, 1);
  }

  {
    // Locking an already locked device should succeed if the parameters match.
    base::HistogramTester histogram_tester;
    EXPECT_EQ(
        InstallAttributes::LOCK_SUCCESS,
        LockDeviceAndWaitForResult(policy::DEVICE_MODE_ENTERPRISE, kTestDomain,
                                   std::string(),  // realm
                                   kTestDeviceId));
    histogram_tester.ExpectUniqueSample(
        "Enterprise.ExistingInstallAttributesLock", 1, 1);
  }

  {
    // But another domain should fail.
    base::HistogramTester histogram_tester;
    EXPECT_EQ(InstallAttributes::LOCK_WRONG_DOMAIN,
              LockDeviceAndWaitForResult(policy::DEVICE_MODE_ENTERPRISE,
                                         "anotherexample.com",
                                         std::string(),  // realm
                                         kTestDeviceId));
    histogram_tester.ExpectTotalCount(
        "Enterprise.ExistingInstallAttributesLock", 0);
  }

  {
    // A non-matching mode should fail as well.
    base::HistogramTester histogram_tester;
    EXPECT_EQ(InstallAttributes::LOCK_WRONG_MODE,
              LockDeviceAndWaitForResult(
                  policy::DEVICE_MODE_CONSUMER_KIOSK_AUTOLAUNCH,
                  std::string(),    // domain
                  std::string(),    // realm
                  std::string()));  // device id
    histogram_tester.ExpectTotalCount(
        "Enterprise.ExistingInstallAttributesLock", 0);
  }
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
  EXPECT_FALSE(install_attributes_->IsActiveDirectoryManaged());
}

TEST_F(InstallAttributesTest, IsEnterpriseManagedRealm) {
  install_attributes_->Init(GetTempPath());
  EXPECT_FALSE(install_attributes_->IsEnterpriseManaged());
  ASSERT_EQ(InstallAttributes::LOCK_SUCCESS,
            LockDeviceAndWaitForResult(policy::DEVICE_MODE_ENTERPRISE_AD,
                                       std::string(),  // domain
                                       kTestRealm, kTestDeviceId));
  EXPECT_TRUE(install_attributes_->IsEnterpriseManaged());
  EXPECT_FALSE(install_attributes_->IsCloudManaged());
  EXPECT_TRUE(install_attributes_->IsActiveDirectoryManaged());
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
  EXPECT_FALSE(install_attributes_->IsActiveDirectoryManaged());
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
}

TEST_F(InstallAttributesTest, GettersAD) {
  install_attributes_->Init(GetTempPath());
  EXPECT_EQ(policy::DEVICE_MODE_PENDING, install_attributes_->GetMode());
  EXPECT_EQ(std::string(), install_attributes_->GetDomain());
  EXPECT_EQ(std::string(), install_attributes_->GetRealm());
  EXPECT_EQ(std::string(), install_attributes_->GetDeviceId());
  ASSERT_EQ(InstallAttributes::LOCK_SUCCESS,
            LockDeviceAndWaitForResult(policy::DEVICE_MODE_ENTERPRISE_AD,
                                       std::string(),  // domain
                                       kTestRealm, kTestDeviceId));
  EXPECT_EQ(policy::DEVICE_MODE_ENTERPRISE_AD, install_attributes_->GetMode());
  EXPECT_EQ(std::string(), install_attributes_->GetDomain());
  EXPECT_EQ(kTestRealm, install_attributes_->GetRealm());
  EXPECT_EQ(kTestDeviceId, install_attributes_->GetDeviceId());
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
}

TEST_F(InstallAttributesTest, ConsumerDevice) {
  install_attributes_->Init(GetTempPath());
  EXPECT_EQ(policy::DEVICE_MODE_PENDING, install_attributes_->GetMode());
  // Lock the attributes empty.
  ASSERT_TRUE(tpm_util::InstallAttributesFinalize());
  base::RunLoop loop;
  install_attributes_->ReadImmutableAttributes(loop.QuitClosure());
  loop.Run();

  ASSERT_FALSE(tpm_util::InstallAttributesIsFirstInstall());
  EXPECT_EQ(policy::DEVICE_MODE_CONSUMER, install_attributes_->GetMode());
  EXPECT_EQ(std::string(), install_attributes_->GetDomain());
  EXPECT_EQ(std::string(), install_attributes_->GetRealm());
  EXPECT_EQ(std::string(), install_attributes_->GetDeviceId());
}

TEST_F(InstallAttributesTest, ConsumerKioskDevice) {
  install_attributes_->Init(GetTempPath());
  EXPECT_EQ(policy::DEVICE_MODE_PENDING, install_attributes_->GetMode());
  // Lock the attributes for consumer kiosk.
  ASSERT_EQ(
      InstallAttributes::LOCK_SUCCESS,
      LockDeviceAndWaitForResult(policy::DEVICE_MODE_CONSUMER_KIOSK_AUTOLAUNCH,
                                 std::string(), std::string(), std::string()));

  ASSERT_FALSE(tpm_util::InstallAttributesIsFirstInstall());
  EXPECT_EQ(policy::DEVICE_MODE_CONSUMER_KIOSK_AUTOLAUNCH,
            install_attributes_->GetMode());
  EXPECT_EQ(std::string(), install_attributes_->GetDomain());
  EXPECT_EQ(std::string(), install_attributes_->GetRealm());
  EXPECT_EQ(std::string(), install_attributes_->GetDeviceId());
  ASSERT_TRUE(install_attributes_->IsConsumerKioskDeviceWithAutoLaunch());
}

TEST_F(InstallAttributesTest, DeviceLockedFromOlderVersion) {
  install_attributes_->Init(GetTempPath());
  EXPECT_EQ(policy::DEVICE_MODE_PENDING, install_attributes_->GetMode());
  // Lock the attributes as if it was done from older Chrome version.
  ASSERT_TRUE(tpm_util::InstallAttributesSet(
      InstallAttributes::kAttrEnterpriseOwned, "true"));
  ASSERT_TRUE(tpm_util::InstallAttributesSet(
      InstallAttributes::kAttrEnterpriseUser, kTestUserDeprecated));
  ASSERT_TRUE(tpm_util::InstallAttributesFinalize());
  base::RunLoop loop;
  install_attributes_->ReadImmutableAttributes(loop.QuitClosure());
  loop.Run();

  ASSERT_FALSE(tpm_util::InstallAttributesIsFirstInstall());
  EXPECT_EQ(policy::DEVICE_MODE_ENTERPRISE, install_attributes_->GetMode());
  EXPECT_EQ(kTestDomain, install_attributes_->GetDomain());
  EXPECT_EQ(std::string(), install_attributes_->GetRealm());
  EXPECT_EQ(std::string(), install_attributes_->GetDeviceId());
}

TEST_F(InstallAttributesTest, Init) {
  cryptohome::SerializedInstallAttributes install_attrs_proto;
  SetAttribute(&install_attrs_proto, InstallAttributes::kAttrEnterpriseOwned,
               "true");
  SetAttribute(&install_attrs_proto, InstallAttributes::kAttrEnterpriseUser,
               kTestUserDeprecated);
  const std::string blob(install_attrs_proto.SerializeAsString());
  ASSERT_EQ(static_cast<int>(blob.size()),
            base::WriteFile(GetTempPath(), blob.c_str(), blob.size()));
  install_attributes_->Init(GetTempPath());
  EXPECT_EQ(policy::DEVICE_MODE_ENTERPRISE, install_attributes_->GetMode());
  EXPECT_EQ(kTestDomain, install_attributes_->GetDomain());
  EXPECT_EQ(std::string(), install_attributes_->GetRealm());
  EXPECT_EQ(std::string(), install_attributes_->GetDeviceId());
}

TEST_F(InstallAttributesTest, InitForConsumerKiosk) {
  cryptohome::SerializedInstallAttributes install_attrs_proto;
  SetAttribute(&install_attrs_proto,
               InstallAttributes::kAttrConsumerKioskEnabled, "true");
  const std::string blob(install_attrs_proto.SerializeAsString());
  ASSERT_EQ(static_cast<int>(blob.size()),
            base::WriteFile(GetTempPath(), blob.c_str(), blob.size()));
  install_attributes_->Init(GetTempPath());
  EXPECT_EQ(policy::DEVICE_MODE_CONSUMER_KIOSK_AUTOLAUNCH,
            install_attributes_->GetMode());
  EXPECT_EQ(std::string(), install_attributes_->GetDomain());
  EXPECT_EQ(std::string(), install_attributes_->GetRealm());
  EXPECT_EQ(std::string(), install_attributes_->GetDeviceId());
}

TEST_F(InstallAttributesTest, VerifyFakeInstallAttributesCache) {
  // This test verifies that FakeCryptohomeClient::InstallAttributesFinalize
  // writes a cache that InstallAttributes::Init accepts.

  // Verify that no attributes are initially set.
  install_attributes_->Init(GetTempPath());
  EXPECT_EQ(policy::DEVICE_MODE_PENDING, install_attributes_->GetMode());

  // Write test values.
  ASSERT_TRUE(tpm_util::InstallAttributesSet(
      InstallAttributes::kAttrEnterpriseOwned, "true"));
  ASSERT_TRUE(tpm_util::InstallAttributesSet(
      InstallAttributes::kAttrEnterpriseUser, kTestUserDeprecated));
  ASSERT_TRUE(tpm_util::InstallAttributesFinalize());

  // Verify that InstallAttributes correctly decodes the stub cache file.
  install_attributes_->Init(GetTempPath());
  EXPECT_EQ(policy::DEVICE_MODE_ENTERPRISE, install_attributes_->GetMode());
  EXPECT_EQ(kTestDomain, install_attributes_->GetDomain());
  EXPECT_EQ(std::string(), install_attributes_->GetRealm());
  EXPECT_EQ(std::string(), install_attributes_->GetDeviceId());
}

TEST_F(InstallAttributesTest, CheckSetBlockDevmodeInTpm) {
  bool succeeded = false;
  install_attributes_->SetBlockDevmodeInTpm(
      true,
      base::BindOnce(
          [](bool* succeeded, base::Optional<cryptohome::BaseReply> reply) {
            *succeeded = reply.has_value();
          },
          &succeeded));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(succeeded);
}

}  // namespace chromeos
