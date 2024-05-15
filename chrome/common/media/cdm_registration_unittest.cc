// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/media/cdm_registration.h"

#include <optional>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/logging.h"
#include "base/native_library.h"
#include "base/path_service.h"
#include "base/test/scoped_path_override.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/media/component_widevine_cdm_hint_file_linux.h"
#include "components/cdm/common/cdm_manifest.h"
#include "media/cdm/cdm_paths.h"  // nogncheck
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/widevine/cdm/widevine_cdm_common.h"

// Currently this file only checks the registration of the software-secure
// Widevine CDM.
#if !BUILDFLAG(ENABLE_WIDEVINE)
#error "This file only applies when Widevine used."
#endif

// On Windows and Mac registration is completely handled by Component Update
// (widevine_cdm_component_installer.cc).
#if !BUILDFLAG(IS_LINUX)
#error "This file only applies to desktop Linux."
#endif

namespace {

// Version numbers for the version that can be returned by Component Update.
// The bundled CDM is expected to have version 4.10..., so using values far
// outside the expected range.
const char kLowerVersion[] = "1.0.0.0";
const char kHigherVersion[] = "10.0.0.0";

// Returns the version of the bundled CDM by reading the manifest. If there is
// no bundled CDM return version "0.0.0.0".
base::Version GetBundledWidevineVersion() {
  base::FilePath cdm_base_path;
  EXPECT_TRUE(
      base::PathService::Get(chrome::DIR_BUNDLED_WIDEVINE_CDM, &cdm_base_path));

  auto manifest_path = cdm_base_path.Append(FILE_PATH_LITERAL("manifest.json"));
  if (base::PathExists(manifest_path)) {
    base::Version version;
    media::CdmCapability capability;
    if (ParseCdmManifestFromPath(manifest_path, &version, &capability)) {
      return version;
    }
  }

  // Parsing the manifest failed, assume no bundled CDM.
  return base::Version({0, 0, 0, 0});
}

// Creates a fake downloaded Widevine CDM with version `version` and updates the
// hint file to refer to it. This creates just the manifest.json file and a
// suitable library (which just needs to exist and does not need to be
// executable), and updates the hint file to refer to it. If `bundled_version`
// is specified, it is included in the hint file.
void CreateFakeComponentUpdatedWidevine(
    base::Version version,
    std::optional<base::Version> bundled_version) {
  // Typically Component Update downloads the Widevine CDM into a directory
  // named after its version.
  base::FilePath component_updated_widevine_directory;
  EXPECT_TRUE(base::PathService::Get(chrome::DIR_COMPONENT_UPDATED_WIDEVINE_CDM,
                                     &component_updated_widevine_directory));
  auto new_component_directory =
      component_updated_widevine_directory.Append(version.GetString());

  // Check if this directory already exists. Useful for the test that runs
  // through multiple scenarios ... no need to write the same directory
  // multiple times.
  if (!base::PathExists(new_component_directory)) {
    EXPECT_TRUE(base::CreateDirectory(new_component_directory));

    // Create a manifest. This is the minimum needed so that
    // ParseCdmManifestFromPath() will be happy with it.
    base::Value::Dict manifest;
    manifest.Set("version", version.GetString());
    manifest.Set("x-cdm-codecs", "vp8,vp09,av01");
    manifest.Set("x-cdm-module-versions", "4");
    manifest.Set("x-cdm-interface-versions", "10");
    manifest.Set("x-cdm-host-versions", "10");

    // Write the manifest to the manifest file.
    auto manifest_path =
        new_component_directory.Append(FILE_PATH_LITERAL("manifest.json"));
    JSONFileValueSerializer serializer(manifest_path);
    EXPECT_TRUE(serializer.Serialize(manifest));

    // Verify that the manifest is actually usable.
    base::Version manifest_version;
    media::CdmCapability capability;
    EXPECT_TRUE(ParseCdmManifestFromPath(manifest_path, &manifest_version,
                                         &capability));

    // Now create a dummy executable. It is in a nested directory, so create the
    // directory first. Contents don't matter as only its existence is checked.
    auto executable_dir =
        media::GetPlatformSpecificDirectory(new_component_directory);
    EXPECT_TRUE(base::CreateDirectory(executable_dir));
    EXPECT_GE(base::WriteFile(executable_dir.Append(base::GetNativeLibraryName(
                                  kWidevineCdmLibraryName)),
                              "random data"),
              0);
  }

  // Always update the hint file to indicate that this is the latest component
  // updated version.
  EXPECT_TRUE(
      UpdateWidevineCdmHintFile(new_component_directory, bundled_version));
}

}  // namespace

TEST(CdmRegistrationTest, ChooseBundledCdm) {
  const base::ScopedPathOverride path_override(chrome::DIR_USER_DATA);
  const base::Version bundled_version = GetBundledWidevineVersion();

  // With no Component Updated Widevine CDM (i.e. no hint file), it should
  // select the bundled CDM, if it exists.
  auto cdms = GetSoftwareSecureWidevine();
#if BUILDFLAG(BUNDLE_WIDEVINE_CDM)
  EXPECT_EQ(cdms.size(), 1u);
  EXPECT_EQ(cdms[0].version, bundled_version);
#else
  EXPECT_EQ(cdms.size(), 0u);
#endif  // BUILDFLAG(BUNDLE_WIDEVINE_CDM)
}

TEST(CdmRegistrationTest, ChooseComponentUpdatedCdm) {
  const base::ScopedPathOverride path_override(chrome::DIR_USER_DATA);
  const base::Version bundled_version = GetBundledWidevineVersion();

  // Component Update version needs to be higher than the bundled CDM to be
  // chosen. Note that if there is no support for bundled CDMs, then
  // `bundled_version` will be 0.0.0.0.
  const base::Version component_updated_version(kHigherVersion);
  EXPECT_GT(component_updated_version, bundled_version);

  // Now create a downloaded Widevine CDM with a higher version.
  CreateFakeComponentUpdatedWidevine(component_updated_version, std::nullopt);

  auto cdms = GetSoftwareSecureWidevine();
#if BUILDFLAG(ENABLE_WIDEVINE_CDM_COMPONENT)
  // Component Updated CDM has the higher version so it should be chosen.
  EXPECT_EQ(cdms.size(), 1u);
  EXPECT_EQ(cdms[0].version, component_updated_version);
#elif BUILDFLAG(BUNDLE_WIDEVINE_CDM)
  // No Component Update support but a bundled CDM, so it should be chosen.
  EXPECT_EQ(cdms.size(), 1u);
  EXPECT_EQ(cdms[0].version, bundled_version);
#else
  // No CDM available.
  EXPECT_EQ(cdms.size(), 0u);
#endif  // BUILDFLAG(ENABLE_WIDEVINE_CDM_COMPONENT)
}

TEST(CdmRegistrationTest, ChooseDowngradedCdm) {
  const base::ScopedPathOverride path_override(chrome::DIR_USER_DATA);
  const base::Version bundled_version = GetBundledWidevineVersion();

  // For this test the Component Updated CDM is a lower version than the bundled
  // CDM, but Component Update has selected it over the bundled CDM.
  const base::Version component_updated_version(kLowerVersion);
#if BUILDFLAG(BUNDLE_WIDEVINE_CDM)
  // Can only check if there is a bundled CDM (as if there is none,
  // `bundled_version` is 0.0.0.0).
  EXPECT_LT(component_updated_version, bundled_version);
#endif

  // Now create a downloaded Widevine CDM with a lower version that replaces the
  // current bundled CDM.
  CreateFakeComponentUpdatedWidevine(component_updated_version,
                                     bundled_version);

  auto cdms = GetSoftwareSecureWidevine();
#if BUILDFLAG(ENABLE_WIDEVINE_CDM_COMPONENT)
  // Even though the Component Updated CDM has the lower version, it should be
  // chosen. Doesn't matter if there is a bundled CDM or not.
  EXPECT_EQ(cdms.size(), 1u);
  EXPECT_EQ(cdms[0].version, component_updated_version);
#elif BUILDFLAG(BUNDLE_WIDEVINE_CDM)
  // No Component Update support but a bundled CDM, so it should be chosen.
  EXPECT_EQ(cdms.size(), 1u);
  EXPECT_EQ(cdms[0].version, bundled_version);
#else
  // No CDM available.
  EXPECT_EQ(cdms.size(), 0u);
#endif  // BUILDFLAG(ENABLE_WIDEVINE_CDM_COMPONENT)
}

TEST(CdmRegistrationTest, ChooseCorrectCdm) {
  const base::ScopedPathOverride path_override(chrome::DIR_USER_DATA);

  // This test will iterate through the following test cases where the numbers
  // represent different versions (and 1 < 2, etc.). As the bundled CDM is
  // fixed, the value for `bundled` must always be 2.
  const struct test_case {
    int16_t hinted;
    int16_t last_bundled;
    int16_t bundled;
    int16_t selected;
  } cases[] = {
      // Normal bundled cases
      {2, 2, 2, 2},  // all versions the same
      {1, 1, 2, 2},  // bundled version is higher
      // Normal component update cases
      {3, 2, 2, 3},  // component updated is higher
      {2, 1, 2, 2},  // component updated is same
      // Downgrade cases
      {2, 3, 2, 2},  // bundled a lower CDM version, should never happen
      {1, 2, 2, 1},  // downgrade
      {0, 1, 2, 2},  // bundled is now greater than last bundled
      {1, 0, 2, 2},  // bundled is now greater than last bundled
  };
  const std::vector<base::Version> versions = {
      base::Version(kLowerVersion), base::Version("2.0.0.0"),
      GetBundledWidevineVersion(), base::Version(kHigherVersion)};

  for (const auto& c : cases) {
    EXPECT_EQ(c.bundled, 2);  // Can't change bundled version.
    CreateFakeComponentUpdatedWidevine(versions[c.hinted],
                                       versions[c.last_bundled]);

    auto cdms = GetSoftwareSecureWidevine();
#if BUILDFLAG(BUNDLE_WIDEVINE_CDM) && BUILDFLAG(ENABLE_WIDEVINE_CDM_COMPONENT)
    EXPECT_EQ(cdms.size(), 1u);
    EXPECT_EQ(cdms[0].version, versions[c.selected]);
#elif BUILDFLAG(BUNDLE_WIDEVINE_CDM)
    // Only support for bundled CDM, so it will always be returned.
    EXPECT_EQ(cdms.size(), 1u);
    EXPECT_EQ(cdms[0].version, versions[c.bundled]);
#elif BUILDFLAG(ENABLE_WIDEVINE_CDM_COMPONENT)
    // Only support for component updated CDM, so it will always be returned.
    EXPECT_EQ(cdms.size(), 1u);
    EXPECT_EQ(cdms[0].version, versions[c.hinted]);
#else
    EXPECT_EQ(cdms.size(), 0u);
#endif
  }
}
