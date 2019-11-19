// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/media/cdm_manifest.h"

#include <stdint.h>
#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_file_value_serializer.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "base/version.h"
#include "content/public/common/cdm_info.h"
#include "extensions/common/manifest_constants.h"
#include "media/cdm/api/content_decryption_module.h"
#include "media/cdm/supported_cdm_versions.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::CdmCapability;

namespace {

// These names must match what is used in cdm_manifest.cc.
const char kCdmModuleVersionsName[] = "x-cdm-module-versions";
const char kCdmInterfaceVersionsName[] = "x-cdm-interface-versions";
const char kCdmHostVersionsName[] = "x-cdm-host-versions";
const char kCdmCodecsListName[] = "x-cdm-codecs";
const char kCdmPersistentLicenseSupportName[] =
    "x-cdm-persistent-license-support";
const char kCdmSupportedEncryptionSchemesName[] =
    "x-cdm-supported-encryption-schemes";
const char kCdmSupportedCdmProxyProtocolsName[] =
    "x-cdm-supported-cdm-proxy-protocols";

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
  list.GetList().push_back(base::Value(item));
  return list;
}

base::Value MakeListValue(const std::string& item1, const std::string& item2) {
  base::Value list(base::Value::Type::LIST);
  list.GetList().push_back(base::Value(item1));
  list.GetList().push_back(base::Value(item2));
  return list;
}

// Create a default manifest with valid values for all entries.
base::Value DefaultManifest() {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey(kCdmCodecsListName, "vp8,vp9.0,av01");
  dict.SetBoolKey(kCdmPersistentLicenseSupportName, true);
  dict.SetKey(kCdmSupportedEncryptionSchemesName,
              MakeListValue("cenc", "cbcs"));
  dict.SetKey(kCdmSupportedCdmProxyProtocolsName, MakeListValue("intel"));

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

void CheckCodecs(const std::vector<media::VideoCodec>& actual,
                 const std::vector<media::VideoCodec>& expected) {
  EXPECT_EQ(expected.size(), actual.size());
  for (const auto& codec : expected) {
    EXPECT_TRUE(base::Contains(actual, codec));
  }
}

void CheckEncryptionSchemes(
    const base::flat_set<media::EncryptionScheme>& actual,
    const std::vector<media::EncryptionScheme>& expected) {
  EXPECT_EQ(expected.size(), actual.size());
  for (const auto& encryption_scheme : expected) {
    EXPECT_TRUE(base::Contains(actual, encryption_scheme));
  }
}

void CheckSessionTypes(const base::flat_set<media::CdmSessionType>& actual,
                       const std::vector<media::CdmSessionType>& expected) {
  EXPECT_EQ(expected.size(), actual.size());
  for (const auto& session_type : expected) {
    EXPECT_TRUE(base::Contains(actual, session_type));
  }
}

void CheckProxyProtocols(
    const base::flat_set<media::CdmProxy::Protocol>& actual,
    const std::vector<media::CdmProxy::Protocol>& expected) {
  EXPECT_EQ(expected.size(), actual.size());
  for (const auto& proxy_protocol : expected) {
    EXPECT_TRUE(base::Contains(actual, proxy_protocol));
  }
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
  CheckCodecs(capability.video_codecs,
              {media::VideoCodec::kCodecVP8, media::VideoCodec::kCodecVP9,
               media::VideoCodec::kCodecAV1});
  CheckEncryptionSchemes(
      capability.encryption_schemes,
      {media::EncryptionScheme::kCenc, media::EncryptionScheme::kCbcs});
  CheckSessionTypes(capability.session_types,
                    {media::CdmSessionType::kTemporary,
                     media::CdmSessionType::kPersistentLicense});
  CheckProxyProtocols(capability.cdm_proxy_protocols,
                      {media::CdmProxy::Protocol::kIntel});
}

TEST(CdmManifestTest, EmptyManifest) {
  base::Value manifest(base::Value::Type::DICTIONARY);
  CdmCapability capability;
  EXPECT_TRUE(ParseCdmManifest(manifest, &capability));
  CheckCodecs(capability.video_codecs, {});
  CheckEncryptionSchemes(capability.encryption_schemes,
                         {media::EncryptionScheme::kCenc});
  CheckSessionTypes(capability.session_types,
                    {media::CdmSessionType::kTemporary});
  CheckProxyProtocols(capability.cdm_proxy_protocols, {});
}

TEST(CdmManifestTest, ManifestCodecs) {
  auto manifest = DefaultManifest();

  // Try each valid value individually.
  {
    CdmCapability capability;
    manifest.SetStringKey(kCdmCodecsListName, "vp8");
    EXPECT_TRUE(ParseCdmManifest(manifest, &capability));
    CheckCodecs(capability.video_codecs, {media::VideoCodec::kCodecVP8});
  }
  {
    CdmCapability capability;
    manifest.SetStringKey(kCdmCodecsListName, "vp9.0");
    EXPECT_TRUE(ParseCdmManifest(manifest, &capability));
    CheckCodecs(capability.video_codecs, {media::VideoCodec::kCodecVP9});
    EXPECT_FALSE(capability.supports_vp9_profile2);
  }
  {
    CdmCapability capability;
    manifest.SetStringKey(kCdmCodecsListName, "vp09");
    EXPECT_TRUE(ParseCdmManifest(manifest, &capability));
    CheckCodecs(capability.video_codecs, {media::VideoCodec::kCodecVP9});
    EXPECT_TRUE(capability.supports_vp9_profile2);
  }
  {
    CdmCapability capability;
    manifest.SetStringKey(kCdmCodecsListName, "av01");
    EXPECT_TRUE(ParseCdmManifest(manifest, &capability));
    CheckCodecs(capability.video_codecs, {media::VideoCodec::kCodecAV1});
  }
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  {
    CdmCapability capability;
    manifest.SetStringKey(kCdmCodecsListName, "avc1");
    EXPECT_TRUE(ParseCdmManifest(manifest, &capability));
    CheckCodecs(capability.video_codecs, {media::VideoCodec::kCodecH264});
  }
#endif
  {
    // Try list of everything (except proprietary codecs).
    CdmCapability capability;
    manifest.SetStringKey(kCdmCodecsListName, "vp8,vp9.0,vp09,av01");
    EXPECT_TRUE(ParseCdmManifest(manifest, &capability));
    // Note that kCodecVP9 is returned twice in the list.
    CheckCodecs(capability.video_codecs,
                {media::VideoCodec::kCodecVP8, media::VideoCodec::kCodecVP9,
                 media::VideoCodec::kCodecVP9, media::VideoCodec::kCodecAV1});
    EXPECT_TRUE(capability.supports_vp9_profile2);
  }
  {
    // Note that invalid codec values are simply skipped.
    CdmCapability capability;
    manifest.SetStringKey(kCdmCodecsListName, "invalid,av01");
    EXPECT_TRUE(ParseCdmManifest(manifest, &capability));
    CheckCodecs(capability.video_codecs, {media::VideoCodec::kCodecAV1});
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
    CheckCodecs(capability.video_codecs, {});
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

TEST(CdmManifestTest, ManifestProxyProtocols) {
  auto manifest = DefaultManifest();

  {
    // Try only supported value.
    CdmCapability capability;
    manifest.SetKey(kCdmSupportedCdmProxyProtocolsName, MakeListValue("intel"));
    EXPECT_TRUE(ParseCdmManifest(manifest, &capability));
    CheckProxyProtocols(capability.cdm_proxy_protocols,
                        {media::CdmProxy::Protocol::kIntel});
  }
  {
    // Unrecognized values are ignored.
    CdmCapability capability;
    manifest.SetKey(kCdmSupportedCdmProxyProtocolsName,
                    MakeListValue("unknown", "intel"));
    EXPECT_TRUE(ParseCdmManifest(manifest, &capability));
    CheckProxyProtocols(capability.cdm_proxy_protocols,
                        {media::CdmProxy::Protocol::kIntel});
  }
  {
    // Wrong types are an error.
    CdmCapability capability;
    manifest.SetStringKey(kCdmSupportedCdmProxyProtocolsName, "intel");
    EXPECT_FALSE(ParseCdmManifest(manifest, &capability));
  }
  {
    // Missing values are OK.
    CdmCapability capability;
    EXPECT_TRUE(manifest.RemoveKey(kCdmSupportedCdmProxyProtocolsName));
    EXPECT_TRUE(ParseCdmManifest(manifest, &capability));
    CheckProxyProtocols(capability.cdm_proxy_protocols, {});
  }
}

TEST(CdmManifestTest, FileManifest) {
  const char kVersion[] = "1.2.3.4";

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  auto manifest_path = temp_dir.GetPath().AppendASCII("manifest.json");

  // Manifests read from a file also need a version.
  auto manifest = DefaultManifest();
  manifest.SetStringKey(extensions::manifest_keys::kVersion, kVersion);
  WriteManifestToFile(manifest, manifest_path);

  base::Version version;
  CdmCapability capability;
  EXPECT_TRUE(ParseCdmManifestFromPath(manifest_path, &version, &capability));
  EXPECT_TRUE(version.IsValid());
  EXPECT_EQ(version.GetString(), kVersion);
  CheckCodecs(capability.video_codecs,
              {media::VideoCodec::kCodecVP8, media::VideoCodec::kCodecVP9,
               media::VideoCodec::kCodecAV1});
  CheckEncryptionSchemes(
      capability.encryption_schemes,
      {media::EncryptionScheme::kCenc, media::EncryptionScheme::kCbcs});
  CheckSessionTypes(capability.session_types,
                    {media::CdmSessionType::kTemporary,
                     media::CdmSessionType::kPersistentLicense});
  CheckProxyProtocols(capability.cdm_proxy_protocols,
                      {media::CdmProxy::Protocol::kIntel});
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
  manifest.SetStringKey(extensions::manifest_keys::kVersion, "bad version");
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
  manifest.SetStringKey(extensions::manifest_keys::kVersion, "1.2.3.4");
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
  CheckCodecs(capability.video_codecs, {});
  CheckEncryptionSchemes(capability.encryption_schemes,
                         {media::EncryptionScheme::kCenc});
  CheckSessionTypes(capability.session_types,
                    {media::CdmSessionType::kTemporary});
  CheckProxyProtocols(capability.cdm_proxy_protocols, {});
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
