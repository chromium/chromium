// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/setup/platform_runtime_component.h"

#include <windows.h>

#include <memory>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/gmock_expected_support.h"
#include "base/values.h"
#include "base/win/security_descriptor.h"
#include "chrome/installer/setup/installer_state.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace installer {

namespace {

constexpr char kManifestName[] = "Chrome Platform Runtime (Inner)";
constexpr char kCrxId[] = "jidecimafobogahglicpmeajcaaaibib";

}  // namespace

TEST(PlatformRuntimeComponentTest, Create) {
  const InstallerState installer_state(InstallerState::SYSTEM_LEVEL);
  std::unique_ptr<PlatformRuntimeComponent> component =
      PlatformRuntimeComponent::Create(
          base::CommandLine(base::CommandLine::NO_PROGRAM), installer_state);
  ASSERT_TRUE(component);
  EXPECT_TRUE(
      component->ReadManifest(base::DictValue().Set("name", kManifestName)));
}

TEST(PlatformRuntimeComponentTest, ReadManifest) {
  PlatformRuntimeComponent component;
  EXPECT_TRUE(
      component.ReadManifest(base::DictValue().Set("name", kManifestName)));

  // The manifest must contain the component's name.
  EXPECT_FALSE(component.ReadManifest(base::DictValue()));
  EXPECT_FALSE(component.ReadManifest(base::DictValue().Set("name", 1)));
  EXPECT_FALSE(component.ReadManifest(
      base::DictValue().Set("name", "Chrome Dynamic Patch (Inner)")));
}

// The component is installed into a directory named for its CRX ID.
TEST(PlatformRuntimeComponentTest, DetermineDestinationRoot) {
  const base::FilePath target_path(
      FILE_PATH_LITERAL("C:\\Program Files\\Google\\Chrome\\Application"));
  InstallerState installer_state(InstallerState::SYSTEM_LEVEL);
  installer_state.set_target_path_for_testing(target_path);

  PlatformRuntimeComponent component;
  ASSERT_TRUE(
      component.ReadManifest(base::DictValue().Set("name", kManifestName)));
  EXPECT_THAT(component.DetermineDestinationRoot(installer_state, kCrxId),
              base::test::ValueIs(target_path.AppendASCII(kCrxId)));
}

// The destination root is left to inherit its ACL from the target directory.
TEST(PlatformRuntimeComponentTest, InitializeDestinationRoot) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  InstallerState installer_state(InstallerState::SYSTEM_LEVEL);
  installer_state.set_target_path_for_testing(temp_dir.GetPath());
  const base::FilePath destination_root =
      temp_dir.GetPath().AppendASCII(kCrxId);
  ASSERT_TRUE(base::CreateDirectory(destination_root));
  ASSERT_OK_AND_ASSIGN(const auto sd_before,
                       base::win::SecurityDescriptor::FromFile(
                           destination_root, DACL_SECURITY_INFORMATION));

  PlatformRuntimeComponent component;
  ASSERT_TRUE(
      component.ReadManifest(base::DictValue().Set("name", kManifestName)));
  EXPECT_TRUE(
      component.InitializeDestinationRoot(installer_state, destination_root));

  ASSERT_OK_AND_ASSIGN(const auto sd_after,
                       base::win::SecurityDescriptor::FromFile(
                           destination_root, DACL_SECURITY_INFORMATION));
  EXPECT_FALSE(sd_after.dacl_protected());
  EXPECT_EQ(sd_after.ToSddl(DACL_SECURITY_INFORMATION),
            sd_before.ToSddl(DACL_SECURITY_INFORMATION));
}

}  // namespace installer
