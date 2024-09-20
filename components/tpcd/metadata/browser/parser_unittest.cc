// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tpcd/metadata/browser/parser.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/content_settings/core/common/features.h"
#include "components/tpcd/metadata/common/proto/metadata.pb.h"
#include "components/tpcd/metadata/browser/test_support.h"
#include "net/base/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zlib/google/compression_utils.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#endif

namespace tpcd::metadata {
namespace {
using ::testing::IsEmpty;

const base::FilePath::CharType kComponentFileName[] =
    FILE_PATH_LITERAL("metadata.pb");
}  // namespace

class ParserTest : public ::testing::Test {
 public:
  ParserTest() {
    CHECK(fake_install_dir_.CreateUniqueTempDir());
    CHECK(fake_install_dir_.IsValid());
    parser_ = std::make_unique<Parser>();
  }

  ~ParserTest() override = default;

 protected:
  // NOTE: we can initialize the ScopedFeatureList this way since this
  // unittest is single threaded. If the test is multi-threaded, this would
  // have to be initialized in the tests constructor.
  void EnableFeatureWithParams(base::FieldTrialParams params) {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        net::features::kTpcdMetadataGrants, params);
  }

  void EnableFeature() {
    scoped_feature_list_.InitAndEnableFeature(
        net::features::kTpcdMetadataGrants);
  }

  void ResetFeature() { scoped_feature_list_.Reset(); }

  void ExecFakeComponentInstallation(std::string_view contents) {
    base::FilePath path =
        fake_install_dir_.GetPath().Append(kComponentFileName);
    CHECK(base::WriteFile(path, contents));
  }

  std::string GetFakeComponent() {
    base::FilePath path =
        fake_install_dir_.GetPath().Append(kComponentFileName);
    CHECK(base::PathExists(path));
    std::string raw_metadata;
    CHECK(base::ReadFileToString(path, &raw_metadata));
    return raw_metadata;
  }

  Parser* parser() { return parser_.get(); }

  // The death test is not reliable on old x86 Android:
  // https://crbug.com/815537 &
  // https://source.chromium.org/chromium/chromium/src/+/main:base/test/test_suite.cc;l=633;drc=b24613adfd8336234c263d1cc8315752368ce7b5.
  bool ShouldSkipTest() {
#if BUILDFLAG(IS_ANDROID) && defined(ARCH_CPU_X86)
    return base::android::BuildInfo::GetInstance()->sdk_int() <=
           base::android::SDK_VERSION_NOUGAT;
    // This test type are also unreliable on Fuchsia.
#elif BUILDFLAG(IS_FUCHSIA)
    return true;
#else
    return false;
#endif
  }

 private:
  std::unique_ptr<Parser> parser_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::ScopedTempDir fake_install_dir_;
};

using ParserDeathTest = ParserTest;

TEST_F(ParserDeathTest, ParseMetadata_NonSerializedProto) {
  if (ShouldSkipTest()) {
    GTEST_SKIP() << "Reason: The death test is not reliable on some platforms, "
                    "like on old x86 Android: https://crbug.com/815537.";
  }

  EXPECT_CHECK_DEATH_WITH(
      parser()->ParseMetadata("clearly not a proto"),
      "Check failed: metadata.ParseFromString.raw_metadata.");
}

TEST_F(ParserDeathTest, ParseMetadata_InvalidComponent) {
  if (ShouldSkipTest()) {
    GTEST_SKIP() << "Reason: The death test is not reliable on some platforms, "
                    "like on old x86 Android: https://crbug.com/815537.";
  }

  ExecFakeComponentInstallation("clearly not a proto");
  EXPECT_CHECK_DEATH_WITH(
      parser()->ParseMetadata("clearly not a proto"),
      "Check failed: metadata.ParseFromString.raw_metadata.");
}

TEST_F(ParserDeathTest, ParseMetadataFromFeatureParam_FailedToDecode) {
  if (ShouldSkipTest()) {
    GTEST_SKIP() << "Reason: The death test is not reliable on some platforms, "
                    "like on old x86 Android: https://crbug.com/815537.";
  }

  const base::FieldTrialParams params = {
      {Parser::kMetadataFeatureParamName, "clearly not a proto"}};
  // No-op: for consistency.
  EnableFeatureWithParams(params);

  EXPECT_CHECK_DEATH_WITH(
      parser()->ParseMetadataFromFeatureParamForTesting(params),
      "Check failed: base::Base64Decode. "
      "params.find.Parser::kMetadataFeatureParamName.->second, &raw_metadata.");
}

TEST_F(ParserDeathTest, ParseMetadataFromFeatureParam_FailedToUnzip) {
  if (ShouldSkipTest()) {
    GTEST_SKIP() << "Reason: The death test is not reliable on some platforms, "
                    "like on old x86 Android: https://crbug.com/815537.";
  }

  std::string encoded = base::Base64Encode("clearly not a proto");
  const base::FieldTrialParams params = {
      {Parser::kMetadataFeatureParamName, encoded}};
  // No-op: for consistency.
  EnableFeatureWithParams(params);

  EXPECT_CHECK_DEATH_WITH(
      parser()->ParseMetadataFromFeatureParamForTesting(params),
      "Check failed: compression::GzipUncompress.raw_metadata, &uncompressed.");
}

TEST_F(ParserDeathTest, ParseMetadataFromFeatureParam_InvalidProto) {
  if (ShouldSkipTest()) {
    GTEST_SKIP() << "Reason: The death test is not reliable on some platforms, "
                    "like on old x86 Android: https://crbug.com/815537.";
  }

  std::string compressed;
  compression::GzipCompress(base::span_from_cstring("clearly not a proto"),
                            &compressed);
  std::string encoded = base::Base64Encode(compressed);
  const base::FieldTrialParams params = {
      {Parser::kMetadataFeatureParamName, encoded}};
  // No-op: for consistency.
  EnableFeatureWithParams(params);

  EXPECT_CHECK_DEATH_WITH(
      parser()->ParseMetadataFromFeatureParamForTesting(params),
      "Check failed: metadata.ParseFromString.uncompressed.");
}

// Should be empty when neither Component Updater nor Feature provide metadata.
TEST_F(ParserTest, GetMetadata_MissingMetadata) {
  EXPECT_THAT(parser()->GetMetadata(), IsEmpty());
}

TEST_F(ParserTest, ParseMetadata_EmptyList) {
  Metadata metadata;

  ExecFakeComponentInstallation(metadata.SerializeAsString());
  parser()->ParseMetadata(GetFakeComponent());
  EXPECT_THAT(parser()->GetMetadata(), IsEmpty());
}

TEST_F(ParserTest, ParseMetadata_NonEmptyList) {
  const std::string primary_pattern_spec = "[*.]bar.com";
  const std::string secondary_pattern_spec = "[*.]foo.com";

  Metadata metadata;
  helpers::AddEntryToMetadata(metadata, primary_pattern_spec,
                              secondary_pattern_spec);
  ASSERT_EQ(metadata.metadata_entries_size(), 1);

  ExecFakeComponentInstallation(metadata.SerializeAsString());
  parser()->ParseMetadata(GetFakeComponent());

  MetadataEntries me = parser()->GetInstalledMetadataForTesting();
  ASSERT_EQ(me.size(), 1u);
  ASSERT_EQ(me.front().primary_pattern_spec(), primary_pattern_spec);
  ASSERT_EQ(me.front().secondary_pattern_spec(), secondary_pattern_spec);
  ASSERT_EQ(me.front().source(), Parser::kSourceTest);
}

TEST_F(ParserTest, GetMetadata_ComponentUpdaterOnly) {
  EnableFeature();

  const std::string primary_pattern_spec = "[*.]bar.com";
  const std::string secondary_pattern_spec = "[*.]foo.com";

  Metadata metadata;
  helpers::AddEntryToMetadata(metadata, primary_pattern_spec,
                              secondary_pattern_spec);
  ASSERT_EQ(metadata.metadata_entries_size(), 1);

  ExecFakeComponentInstallation(metadata.SerializeAsString());
  parser()->ParseMetadata(GetFakeComponent());

  MetadataEntries me = parser()->GetMetadata();
  ASSERT_EQ(me.size(), 1u);
  ASSERT_EQ(me.front().primary_pattern_spec(), primary_pattern_spec);
  ASSERT_EQ(me.front().secondary_pattern_spec(), secondary_pattern_spec);
}

TEST_F(ParserTest, GetMetadata_FeatureParamsOnly) {
  const std::string primary_pattern_spec = "[*.]bar.com";
  const std::string secondary_pattern_spec = "[*.]foo.com";

  Metadata metadata;
  helpers::AddEntryToMetadata(metadata, primary_pattern_spec,
                              secondary_pattern_spec);
  EnableFeatureWithParams({{Parser::kMetadataFeatureParamName,
                            MakeBase64EncodedMetadata(metadata)}});

  MetadataEntries me = parser()->GetMetadata();
  ASSERT_EQ(me.size(), 1u);
  ASSERT_EQ(me.front().primary_pattern_spec(), primary_pattern_spec);
  ASSERT_EQ(me.front().secondary_pattern_spec(), secondary_pattern_spec);
}

TEST_F(ParserTest, GetMetadata_ComponentUpdaterThenFeatureParams) {
  const std::string primary_pattern_spec = "[*.]bar.com";
  const std::string secondary_pattern_spec = "[*.]foo.com";
  const std::string wildcard_spec = "*";

  {
    EnableFeature();

    Metadata metadata;
    helpers::AddEntryToMetadata(metadata, primary_pattern_spec,
                                secondary_pattern_spec);
    ASSERT_EQ(metadata.metadata_entries_size(), 1);

    ExecFakeComponentInstallation(metadata.SerializeAsString());
    parser()->ParseMetadata(GetFakeComponent());

    MetadataEntries me = parser()->GetMetadata();
    ASSERT_EQ(me.size(), 1u);
    ASSERT_EQ(me.front().primary_pattern_spec(), primary_pattern_spec);
    ASSERT_EQ(me.front().secondary_pattern_spec(), secondary_pattern_spec);
  }

  ResetFeature();

  {
    Metadata metadata;
    helpers::AddEntryToMetadata(metadata, wildcard_spec, wildcard_spec);

    EnableFeatureWithParams({{Parser::kMetadataFeatureParamName,
                              MakeBase64EncodedMetadata(metadata)}});

    MetadataEntries me = parser()->GetInstalledMetadataForTesting();
    ASSERT_EQ(me.size(), 1u);
    ASSERT_EQ(me.front().primary_pattern_spec(), primary_pattern_spec);
    ASSERT_EQ(me.front().secondary_pattern_spec(), secondary_pattern_spec);

    me = parser()->GetMetadata();
    ASSERT_EQ(me.size(), 1u);
    ASSERT_EQ(me.front().primary_pattern_spec(), wildcard_spec);
    ASSERT_EQ(me.front().secondary_pattern_spec(), wildcard_spec);
  }
}

TEST_F(ParserTest, GetMetadata_FeatureParamsThenComponentUpdater_1) {
  const std::string primary_pattern_spec = "[*.]bar.com";
  const std::string secondary_pattern_spec = "[*.]foo.com";
  const std::string wildcard_spec = "*";

  {
    Metadata metadata;
    helpers::AddEntryToMetadata(metadata, wildcard_spec, wildcard_spec);

    EnableFeatureWithParams({{Parser::kMetadataFeatureParamName,
                              MakeBase64EncodedMetadata(metadata)}});

    MetadataEntries me = parser()->GetInstalledMetadataForTesting();
    EXPECT_THAT(me, IsEmpty());

    me = parser()->GetMetadata();
    ASSERT_EQ(me.size(), 1u);
    ASSERT_EQ(me.front().primary_pattern_spec(), wildcard_spec);
    ASSERT_EQ(me.front().secondary_pattern_spec(), wildcard_spec);
  }

  ResetFeature();

  {
    EnableFeature();

    Metadata metadata;
    helpers::AddEntryToMetadata(metadata, primary_pattern_spec,
                                secondary_pattern_spec);
    ASSERT_EQ(metadata.metadata_entries_size(), 1);

    ExecFakeComponentInstallation(metadata.SerializeAsString());
    parser()->ParseMetadata(GetFakeComponent());

    MetadataEntries me = parser()->GetMetadata();
    ASSERT_EQ(me.size(), 1u);
    ASSERT_EQ(me.front().primary_pattern_spec(), primary_pattern_spec);
    ASSERT_EQ(me.front().secondary_pattern_spec(), secondary_pattern_spec);
  }
}

TEST_F(ParserTest, GetMetadata_FeatureParamsThenComponentUpdater_2) {
  const std::string primary_pattern_spec = "[*.]bar.com";
  const std::string secondary_pattern_spec = "[*.]foo.com";
  const std::string wildcard_spec = "*";

  {
    Metadata metadata;
    helpers::AddEntryToMetadata(metadata, wildcard_spec, wildcard_spec);

    EnableFeatureWithParams({{Parser::kMetadataFeatureParamName,
                              MakeBase64EncodedMetadata(metadata)}});

    MetadataEntries me = parser()->GetInstalledMetadataForTesting();
    EXPECT_THAT(me, IsEmpty());

    me = parser()->GetMetadata();
    ASSERT_EQ(me.size(), 1u);
    ASSERT_EQ(me.front().primary_pattern_spec(), wildcard_spec);
    ASSERT_EQ(me.front().secondary_pattern_spec(), wildcard_spec);
  }

  {
    Metadata metadata;
    helpers::AddEntryToMetadata(metadata, primary_pattern_spec,
                                secondary_pattern_spec);
    ASSERT_EQ(metadata.metadata_entries_size(), 1);

    ExecFakeComponentInstallation(metadata.SerializeAsString());
    parser()->ParseMetadata(GetFakeComponent());

    MetadataEntries me = parser()->GetInstalledMetadataForTesting();
    ASSERT_EQ(me.size(), 1u);
    ASSERT_EQ(me.front().primary_pattern_spec(), primary_pattern_spec);
    ASSERT_EQ(me.front().secondary_pattern_spec(), secondary_pattern_spec);

    me = parser()->GetMetadata();
    ASSERT_EQ(me.size(), 1u);
    ASSERT_EQ(me.front().primary_pattern_spec(), wildcard_spec);
    ASSERT_EQ(me.front().secondary_pattern_spec(), wildcard_spec);
  }
}

}  // namespace tpcd::metadata
