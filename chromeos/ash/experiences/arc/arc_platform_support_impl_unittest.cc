// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/experiences/arc/arc_platform_support_impl.h"

#include <memory>

#include "base/test/test_future.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/fake_cros_settings_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

class ArcPlatformSupportImplTest : public testing::Test {
 protected:
  void SetUp() override {
    cros_settings_ = std::make_unique<ash::CrosSettings>();
    arc_platform_support_ = std::make_unique<ArcPlatformSupportImpl>();
  }

  void TearDown() override {
    arc_platform_support_.reset();
    cros_settings_.reset();
  }

  void SetEnterpriseManaged() {
    test_install_attributes_.Get()->SetCloudManaged("test-domain",
                                                    "FAKE_DEVICE_ID");
  }

  void SetDlcPolicy(bool enabled) {
    auto provider =
        std::make_unique<ash::FakeCrosSettingsProvider>(base::DoNothing());
    provider->Set(ash::kDeviceFlexArcPreloadEnabled, enabled);
    cros_settings_->AddSettingsProvider(std::move(provider));
  }

  std::unique_ptr<ash::CrosSettings> cros_settings_;
  ash::ScopedStubInstallAttributes test_install_attributes_;
  std::unique_ptr<ArcPlatformSupportImpl> arc_platform_support_;
};

// Test case: Device is not enterprise managed.
TEST_F(ArcPlatformSupportImplTest, IsDlcEnabled_NotManaged) {
  arc_platform_support_->CheckDlcRequirement();

  ASSERT_NE(nullptr, ArcPlatformSupport::Get());
  EXPECT_FALSE(ArcPlatformSupport::Get()->IsDlcEnabled());
}

// Test case: Managed, but the DLC policy is set to false.
TEST_F(ArcPlatformSupportImplTest, IsDlcEnabled_Managed_PolicyDisabled) {
  SetEnterpriseManaged();
  SetDlcPolicy(false);

  arc_platform_support_->CheckDlcRequirement();

  ASSERT_NE(nullptr, ArcPlatformSupport::Get());
  EXPECT_FALSE(ArcPlatformSupport::Get()->IsDlcEnabled());
}

// Test case: Managed, and the DLC policy is set to true.
TEST_F(ArcPlatformSupportImplTest, IsDlcEnabled_Managed_PolicyEnabled) {
  SetEnterpriseManaged();
  SetDlcPolicy(true);

  arc_platform_support_->CheckDlcRequirement();

  ASSERT_NE(nullptr, ArcPlatformSupport::Get());
  EXPECT_TRUE(ArcPlatformSupport::Get()->IsDlcEnabled());
}

}  // namespace arc
