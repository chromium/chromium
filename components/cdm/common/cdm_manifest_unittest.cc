// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cdm/common/cdm_manifest.h"

#include <stdint.h>
#include <memory>
#include <string>
#include <vector>

#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_file_value_serializer.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "base/version.h"
#include "content/public/common/cdm_info.h"
#include "media/cdm/api/content_decryption_module.h"
#include "media/cdm/cdm_capability.h"
#include "media/cdm/supported_cdm_versions.h"
#include "testing/gtest/include/gtest/gtest.h"

using media::CdmCapability;

namespace {

// These names must match what is used in cdm_manifest.cc.
const char kCdmVersion[] = "version";
const char kCdmModuleVersionsName[] = "x-cdm-module-versions";
const char kCdmInterfaceVersionsName[] = "x-cdm-interface-versions";
const char kCdmHostVersionsName[] = "x-cdm-host-versions";
const char kCdmCodecsListName[] = "x-cdm-codecs";
const char kCdmPersistentLicenseSupportName[] =
    "x-cdm-persistent-license-support";
const char kCdmSupportedEncryptionSchemesName[] =
    "x-cdm-supported-encryption-schemes";

// Version checking does change over time. Deriving these values from constants
// in the code to ensure they change when the CDM interface changes.
// |kSupportedCdmInterfaceVersion| and |kSupportedCdmHostVersion| are the
// minimum versions supported. There may be versions after them that are also
// supported.
constexpr int kSupportedCdmModuleVersion = CDM_MODULE_VERSION;
constexpr int kSupportedCdmInterfaceVersion =
    media::kSupportedCdmInterfaceVersions[0].version;
static_assert(media::kSupportedCdmInterfaceVersions[0].enabled,
              "kSupportedCdmInterfaceVersion is not enabled by default.");
constexpr int kSupportedCdmHostVersion = media::kMinSupportedCdmHostVersion;

// Make a string of the values from 0 up to and including |item|.
std::string MakeStringList(int item) {
  DCHECK_GT(item, 0);
  std::vector<std::string> parts;
  for (int i = 0; i <= item; ++i) {
    parts.push_back(base::NumberToString(i));
  }
  return base::JoinString(parts, ",");
}

base::Value MakeListValue(const std::string& item) {
  base::Value list(base::Value::Type::LIST);
  list.Append(item);
  return list;
}

base::Value MakeListValue(const std::string& item1, const std::string& item2) {
  base::Value list(base::Value::Type::LIST);
  list.Append(item1);
  list.Append(item2);
  return list;
}

// Create a default manifest with valid values for all entries.
base::Value DefaultManifest() {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey(kCdmCodecsListName, "vp8,vp09,av01");
  dict.SetBoolKey(kCdmPersistentLicenseSupportName, true);
  dict.SetKey(kCdmSupportedEncryptionSchemesName,
              MakeListValue("cenc", "cbcs"));

  // The following are dependent on what the current code supports.
  EXPECT_TRUE(media::IsSupportedCdmModuleVersion(kSupportedCdmModuleVersion));
  EXPECT_TRUE(media::IsSupportedAndEnabledCdmInterfaceVersion(
      kSupportedCdmInterfaceVersion));
  EXPECT_TRUE(media::IsSupportedCdmHostVersion(kSupportedCdmHostVersion));
  dict.SetStringKey(kCdmModuleVersionsName,
                    base::NumberToString(kSupportedCdmModuleVersion));
  dict.SetStringKey(kCdmInterfaceVersionsName,
                    base::NumberToString(kSupportedCdmInterfaceVersion));
  dict.SetStringKey(kCdmHostVersionsName,
                    base::NumberToString(kSupportedCdmHostVersion));
  return dict;
}

void CheckVideoCodecs(const media::CdmCapability::VideoCodecMap& actual,
                      const std::vector<media::VideoCodec>& expected) {
  EXPECT_EQ(expected.size(), actual.size());
  for (const auto& codec : actual) {
    EXPECT_TRUE(base::Contains(expected, codec.first));

    // As the manifest only specifies codecs and not profiles, the list of
    // profiles should be empty to indicate that all profiles are supported.
    EXPECT_TRUE(codec.second.empty());
  }
}

void CheckAudioCodecs(const std::vector<media::AudioCodec>& actual,
                      const std::vector<media::AudioCodec>& expected) {
  EXPECT_EQ(actual, expected);
}

void CheckEncryptionSchemes(
    const base::flat_set<media::EncryptionScheme>& actual,
    const std::vector<media::EncryptionScheme>& expected) {
  EXPECT_EQ(actual, expected);
}

void CheckSessionTypes(const base::flat_set<media::CdmSessionType>& actual,
                       const std::vector<media::CdmSessionType>& expected) {
  EXPECT_EQ(actual, expected);
}

void WriteManifestToFile(const base::Value& manifest,
                         const base::FilePath& file_path) {
  EXPECT_FALSE(base::PathExists(file_path));
  JSONFileValueSerializer serializer(file_path);
  EXPECT_TRUE(serializer.Serialize(manifest));
  EXPECT_TRUE(base::PathExists(file_path));
}

}  // namespace

TEST(CdmManifestTest, IsCompatibleWithChrome) {
  base::Value manifest(DefaultManifest());
  EXPECT_TRUE(IsCdmManifestCompatibleWithChrome(manifest));
}

TEST(CdmManifestTest, InCompatibleModuleVersion) {
  const int kUnsupportedModuleVersion = 0;
  EXPECT_FALSE(media::IsSupportedCdmModuleVersion(kUnsupportedModuleVersion));

  auto manifest = DefaultManifest();
  manifest.SetStringKey(kCdmModuleVersionsName,
                        base::NumberToString(kUnsupportedModuleVersion));
  EXPECT_FALSE(IsCdmManifestCompatibleWithChrome(std::move(manifest)));
}

TEST(CdmManifestTest, InCompatibleInterfaceVersion) {
  const int kUnsupportedInterfaceVersion = kSupportedCdmInterfaceVersion - 1;
  EXPECT_FALSE(media::IsSupportedAndEnabledCdmInterfaceVersion(
      kUnsupportedInterfaceVersion));

  auto manifest = DefaultManifest();
  manifest.SetStringKey(kCdmInterfaceVersionsName,
                        base::NumberToString(kUnsupportedInterfaceVersion));
  EXPECT_FALSE(IsCdmManifestCompatibleWithChrome(std::move(manifest)));
}

TEST(CdmManifestTest, InCompatibleHostVersion) {
  const int kUnsupportedHostVersion = kSupportedCdmHostVersion - 1;
  EXPECT_FALSE(media::IsSupportedCdmHostVersion(kUnsupportedHostVersion));

  auto manifest = DefaultManifest();
  manifest.SetStringKey(kCdmHostVersionsName,
                        base::NumberToString(kUnsupportedHostVersion));
  EXPECT_FALSE(IsCdmManifestCompatibleWithChrome(std::move(manifest)));
}

TEST(CdmManifestTest, IsCompatibleWithMultipleValues) {
  auto manifest = DefaultManifest();
  manifest.SetStringKey(kCdmModuleVersionsName,
                        MakeStringList(kSupportedCdmModuleVersion));
  manifest.SetStringKey(kCdmInterfaceVersionsName,
                        MakeStringList(kSupportedCdmInterfaceVersion));
  manifest.SetStringKey(kCdmHostVersionsName,
                        MakeStringList(kSupportedCdmHostVersion));
  EXPECT_TRUE(IsCdmManifestCompatibleWithChrome(std::move(manifest)));
}

TEST(CdmManifestTest, ValidManifest) {
  auto manifest = DefaultManifest();
  CdmCapability capability;
  EXPECT_TRUE(ParseCdmManifest(manifest, &capability));
  CheckVideoCodecs(capability.video_codecs,
                   {media::VideoCodec::kVP8, media::VideoCodec::kVP9,
                    media::VideoCodec::kAV1});
  CheckAudioCodecs(capability.audio_codecs, {
    media::AudioCodec::kOpus, media::AudioCodec::kVorbis,
        media::AudioCodec::kFLAC,
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
        media::AudioCodec::kAAC,
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)
  });
  CheckEncryptionSchemes(
      capability.encryption_schemes,
      {media::EncryptionScheme::kCenc, media::EncryptionScheme::kCbcs});
  CheckSessionTypes(capability.session_types,
                    {media::CdmSessionType::kTemporary,
                     media::CdmSessionType::kPersistentLicense});
}

TEST(CdmManifestTest, EmptyManifest) {
  base::Value manifest(base::Value::Type::DICTIONARY);
  CdmCapability capability;
  EXPECT_TRUE(ParseCdmManifest(manifest, &capability));
  CheckVideoCodecs(capability.video_codecs, {});
  CheckAudioCodecs(capability.audio_codecs, {
    media::AudioCodec::kOpus, media::AudioCodec::kVorbis,
        media::AudioCodec::kFLAC,
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
        media::AudioCodec::kAAC,
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)
  });
  CheckEncryptionSchemes(capability.encryption_schemes,
                         {media::EncryptionScheme::kCenc});
  CheckSessionTypes(capability.session_types,
                    {media::CdmSessionType::kTemporary});
}

TEST(CdmManifestTest, ManifestCodecs) {
  auto manifest = DefaultManifest();

  // Try each valid value individually.
  {
    CdmCapability capability;
    manifest.SetStringKey(kCdmCodecsListName, "vp8");
    EXPECT_TRUE(ParseCdmManifest(manifest, &capability));
    CheckVideoCodecs(capability.video_codecs, {media::VideoCodec::kVP8});
  }
  {
    CdmCapability capability;
    manifest.SetStringKey(kCdmCodecsListName, "vp09");
    EXPECT_TRUE(ParseCdmManifest(manifest, &capability));
    CheckVideoCodecs(capability.video_codecs, {media::VideoCodec::kVP9});
  }
  {
    CdmCapability capability;
    manifest.SetStringKey(kCdmCodecsListName, "av01");
    EXPECT_TRUE(ParseCdmManifest(manifest, &capability));
    CheckVideoCodecs(capability.video_codecs, {media::VideoCodec::kAV1});
  }
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  {
    CdmCapability capability;
    manifest.SetStringKey(kCdmCodecsListName, "avc1");
    EXPECT_TRUE(ParseCdmManifest(manifest, &capability));
    CheckVideoCodecs(capability.video_codecs, {media::VideoCodec::kH264});
  }
#endif
  {
    // Try list of everything (except proprietary codecs).
    CdmCapability capability;
    manifest.SetStringKey(kCdmCodecsListName, "vp8,vp09,av01");
    EXPECT_TRUE(ParseCdmManifest(manifest, &capability));
    CheckVideoCodecs(capability.video_codecs,
                     {media::VideoCodec::kVP8, media::VideoCodec::kVP9,
                      media::VideoCodec::kAV1});
  }
  {
    // Empty codecs list result in empty list.
    CdmCapability capability;
    manifest.SetStringKey(kCdmCodecsListName, "");
    EXPECT_TRUE(ParseCdmManifest(manifest, &capability));
    CheckVideoCodecs(capability.video_codecs, {});
  }
  {
    // Note that invalid codec values are simply skipped.
    CdmCapability capability;
    manifest.SetStringKey(kCdmCodecsListName, "invalid,av01");
    EXPECT_TRUE(ParseCdmManifest(manifest, &capability));
    CheckVideoCodecs(capability.video_codecs, {media::VideoCodec::kAV1});
  }
  {
    // Legacy: "vp9.0" was used to support VP9 profile 0 (no profile 2 support).
    // Now this has been deprecated and "vp9.0" becomes an invalid codec value.
    CdmCapability capability;
    manifest.SetStringKey(kCdmCodecsListName, "vp9.0");
    EXPECT_TRUE(ParseCdmManifest(manifest, &capability));
    CheckVideoCodecs(capability.video_codecs, {});
  }
  {
    // Wrong types are an error.
    CdmCapability capability;
    manifest.SetBoolKey(kCdmCodecsListName, true);
    EXPECT_FALSE(ParseCdmManifest(manifest, &capability));
  }
  {
    // Missing entry is OK, but list is empty.
    CdmCapability capability;
    EXPECT_TRUE(manifest.RemoveKey(kCdmCodecsListName));
    EXPECT_TRUE(ParseCdmManifest(manifest, &capability));
    CheckVideoCodecs(capability.video_codecs, {});
  }
}

TEST(CdmManifestTest, ManifestEncryptionSchemes) {
  auto manifest = DefaultManifest();

  // Try each valid value individually.
  {
    CdmCapability capability;
    manifest.SetKey(kCdmSupportedEncryptionSchemesName, MakeListValue("cenc"));
    EXPECT_TRUE(ParseCdmManifest(manifest, &capability));
    CheckEncryptionSchemes(capability.encryption_schemes,
                           {media::EncryptionScheme::kCenc});
  }
  {
    CdmCapability capability;
    manifest.SetKey(kCdmSupportedEncryptionSchemesName, MakeListValue("cbcs"));
    EXPECT_TRUE(ParseCdmManifest(manifest, &capability));
    CheckEncryptionSchemes(capability.encryption_schemes,
                           {media::EncryptionScheme::kCbcs});
  }
  {
    // Try multiple valid entries.
    CdmCapability capability;
    manifest.SetKey(kCdmSupportedEncryptionSchemesName,
                    MakeListValue("cenc", "cbcs"));
    EXPECT_TRUE(ParseCdmManifest(manifest, &capability));
    CheckEncryptionSchemes(
        capability.encryption_schemes,
        {media::EncryptionScheme::kCenc, media::EncryptionScheme::kCbcs});
  }
  {
    // Invalid encryption schemes are ignored. However, if value specified then
    // there must be at least 1 valid value.
    CdmCapability capability;
    manifest.SetKey(kCdmSupportedEncryptionSchemesName,
                    MakeListValue("invalid"));
    EXPECT_FALSE(ParseCdmManifest(manifest, &capability));
  }
  {
    CdmCapability capability;
    manifest.SetKey(kCdmSupportedEncryptionSchemesName,
                    MakeListValue("invalid", "cenc"));
    EXPECT_TRUE(ParseCdmManifest(manifest, &capability));
    CheckEncryptionSchemes(capability.encryption_schemes,
                           {media::EncryptionScheme::kCenc});
  }
  {
    // Wrong types are an error.
    CdmCapability capability;
    manifest.SetBoolKey(kCdmSupportedEncryptionSchemesName, true);
    EXPECT_FALSE(ParseCdmManifest(manifest, &capability));
  }
  {
    // Missing values default to "cenc".
    CdmCapability capability;
    EXPECT_TRUE(manifest.RemoveKey(kCdmSupportedEncryptionSchemesName));
    EXPECT_TRUE(ParseCdmManifest(manifest, &capability));
    CheckEncryptionSchemes(capability.encryption_schemes,
                           {media::EncryptionScheme::kCenc});
  }
}

TEST(CdmManifestTest, ManifestSessionTypes) {
  auto manifest = DefaultManifest();

  {
    // Try false (persistent license not supported).
    CdmCapability capability;
    manifest.SetBoolKey(kCdmPersistentLicenseSupportName, false);
    EXPECT_TRUE(ParseCdmManifest(manifest, &capability));
    CheckSessionTypes(capability.session_types,
                      {media::CdmSessionType::kTemporary});
  }
  {
    // Try true (persistent license is supported).
    CdmCapability capability;
    manifest.SetBoolKey(kCdmPersistentLicenseSupportName, true);
    EXPECT_TRUE(ParseCdmManifest(manifest, &capability));
    CheckSessionTypes(capability.session_types,
                      {media::CdmSessionType::kTemporary,
                       media::CdmSessionType::kPersistentLicense});
  }
  {
    // Wrong types are an error.
    CdmCapability capability;
    manifest.SetStringKey(kCdmPersistentLicenseSupportName, "true");
    EXPECT_FALSE(ParseCdmManifest(manifest, &capability));
  }
  {
    // Missing values default to "temporary".
    CdmCapability capability;
    EXPECT_TRUE(manifest.RemoveKey(kCdmPersistentLicenseSupportName));
    EXPECT_TRUE(ParseCdmManifest(manifest, &capability));
    CheckSessionTypes(capability.session_types,
                      {media::CdmSessionType::kTemporary});
  }
}

TEST(CdmManifestTest, FileManifest) {
  const char kVersion[] = "1.2.3.4";

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  auto manifest_path = temp_dir.GetPath().AppendASCII("manifest.json");

  // Manifests read from a file also need a version.
  auto manifest = DefaultManifest();
  manifest.SetStringKey(kCdmVersion, kVersion);
  WriteManifestToFile(manifest, manifest_path);

  base::Version version;
  CdmCapability capability;
  EXPECT_TRUE(ParseCdmManifestFromPath(manifest_path, &version, &capability));
  EXPECT_TRUE(version.IsValid());
  EXPECT_EQ(version.GetString(), kVersion);
  CheckVideoCodecs(capability.video_codecs,
                   {media::VideoCodec::kVP8, media::VideoCodec::kVP9,
                    media::VideoCodec::kAV1});
  CheckEncryptionSchemes(
      capability.encryption_schemes,
      {media::EncryptionScheme::kCenc, media::EncryptionScheme::kCbcs});
  CheckSessionTypes(capability.session_types,
                    {media::CdmSessionType::kTemporary,
                     media::CdmSessionType::kPersistentLicense});
}

TEST(CdmManifestTest, FileManifestNoVersion) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  auto manifest_path = temp_dir.GetPath().AppendASCII("manifest.json");

  auto manifest = DefaultManifest();
  WriteManifestToFile(manifest, manifest_path);

  base::Version version;
  CdmCapability capability;
  EXPECT_FALSE(ParseCdmManifestFromPath(manifest_path, &version, &capability));
}

TEST(CdmManifestTest, FileManifestBadVersion) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  auto manifest_path = temp_dir.GetPath().AppendASCII("manifest.json");

  auto manifest = DefaultManifest();
  manifest.SetStringKey(kCdmVersion, "bad version");
  WriteManifestToFile(manifest, manifest_path);

  base::Version version;
  CdmCapability capability;
  EXPECT_FALSE(ParseCdmManifestFromPath(manifest_path, &version, &capability));
}

TEST(CdmManifestTest, FileManifestDoesNotExist) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  auto manifest_path = temp_dir.GetPath().AppendASCII("manifest.json");

  base::Version version;
  CdmCapability capability;
  EXPECT_FALSE(ParseCdmManifestFromPath(manifest_path, &version, &capability));
}

TEST(CdmManifestTest, FileManifestEmpty) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  auto manifest_path = temp_dir.GetPath().AppendASCII("manifest.json");

  base::Value manifest(base::Value::Type::DICTIONARY);
  WriteManifestToFile(manifest, manifest_path);

  base::Version version;
  CdmCapability capability;
  EXPECT_FALSE(ParseCdmManifestFromPath(manifest_path, &version, &capability));
}

TEST(CdmManifestTest, FileManifestLite) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  auto manifest_path = temp_dir.GetPath().AppendASCII("manifest.json");

  // Only a version plus fields to satisfy compatibility are required in the
  // manifest to parse correctly.
  base::Value manifest(base::Value::Type::DICTIONARY);
  manifest.SetStringKey(kCdmVersion, "1.2.3.4");
  manifest.SetStringKey(kCdmModuleVersionsName,
                        base::NumberToString(kSupportedCdmModuleVersion));
  manifest.SetStringKey(kCdmInterfaceVersionsName,
                        base::NumberToString(kSupportedCdmInterfaceVersion));
  manifest.SetStringKey(kCdmHostVersionsName,
                        base::NumberToString(kSupportedCdmHostVersion));
  WriteManifestToFile(manifest, manifest_path);

  base::Version version;
  CdmCapability capability;
  EXPECT_TRUE(ParseCdmManifestFromPath(manifest_path, &version, &capability));
  CheckVideoCodecs(capability.video_codecs, {});
  CheckEncryptionSchemes(capability.encryption_schemes,
                         {media::EncryptionScheme::kCenc});
  CheckSessionTypes(capability.session_types,
                    {media::CdmSessionType::kTemporary});
}

TEST(CdmManifestTest, FileManifestNotDictionary) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  auto manifest_path = temp_dir.GetPath().AppendASCII("manifest.json");

  base::Value manifest("not a dictionary");
  WriteManifestToFile(manifest, manifest_path);

  base::Version version;
  CdmCapability capability;
  EXPECT_FALSE(ParseCdmManifestFromPath(manifest_path, &version, &capability));
}
