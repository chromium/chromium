// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/media/component_widevine_cdm_hint_file_linux.h"

#include <stdint.h>
#include <stdlib.h>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/path_service.h"
#include "base/test/scoped_path_override.h"
#include "base/version.h"
#include "chrome/common/chrome_paths.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Name of the widevine directory. Does not have to exist for testing.
const char kWidevineDirectory[] = "WidevineCdmInstalledHere";

base::FilePath CreateWidevineComponentUpdatedDirectory() {
  base::FilePath widevine_dir;
  CHECK(base::PathService::Get(chrome::DIR_COMPONENT_UPDATED_WIDEVINE_CDM,
                               &widevine_dir));

  base::File::Error error;
  EXPECT_TRUE(base::CreateDirectoryAndGetError(widevine_dir, &error));
  return widevine_dir;
}

}  // namespace

TEST(ComponentWidevineHintFileTest, RecordUpdate) {
  const base::ScopedPathOverride path_override(chrome::DIR_USER_DATA);
  CreateWidevineComponentUpdatedDirectory();
  EXPECT_TRUE(UpdateWidevineCdmHintFile(base::FilePath(kWidevineDirectory),
                                        std::nullopt));
}

TEST(ComponentWidevineHintFileTest, RecordUpdateNoDir) {
  const base::ScopedPathOverride path_override(chrome::DIR_USER_DATA);
  EXPECT_FALSE(UpdateWidevineCdmHintFile(base::FilePath(kWidevineDirectory),
                                         std::nullopt));
}

TEST(ComponentWidevineHintFileTest, Read) {
  const base::ScopedPathOverride path_override(chrome::DIR_USER_DATA);

  CreateWidevineComponentUpdatedDirectory();
  EXPECT_TRUE(UpdateWidevineCdmHintFile(base::FilePath(kWidevineDirectory),
                                        base::Version("1.0")));

  EXPECT_EQ(GetHintedWidevineCdmDirectory().value(), kWidevineDirectory);
  EXPECT_EQ(GetBundledVersionDuringLastComponentUpdate(), base::Version("1.0"));
}

TEST(ComponentWidevineHintFileTest, ReadNoDir) {
  const base::ScopedPathOverride path_override(chrome::DIR_USER_DATA);

  EXPECT_TRUE(GetHintedWidevineCdmDirectory().empty());
  EXPECT_FALSE(GetBundledVersionDuringLastComponentUpdate().has_value());
}

TEST(ComponentWidevineHintFileTest, ReadNoFile) {
  const base::ScopedPathOverride path_override(chrome::DIR_USER_DATA);

  CreateWidevineComponentUpdatedDirectory();
  EXPECT_TRUE(GetHintedWidevineCdmDirectory().empty());
  EXPECT_FALSE(GetBundledVersionDuringLastComponentUpdate().has_value());
}

TEST(ComponentWidevineHintFileTest, ReplaceFile) {
  const base::ScopedPathOverride path_override(chrome::DIR_USER_DATA);
  const char kAltWidevineDirectory[] = "WidevineCdmInstalledOverThere";

  CreateWidevineComponentUpdatedDirectory();
  EXPECT_TRUE(UpdateWidevineCdmHintFile(base::FilePath(kWidevineDirectory),
                                        base::Version("1.0")));
  EXPECT_EQ(GetHintedWidevineCdmDirectory().value(), kWidevineDirectory);
  EXPECT_EQ(GetBundledVersionDuringLastComponentUpdate(), base::Version("1.0"));

  // Now write the hint file a second time.
  EXPECT_TRUE(UpdateWidevineCdmHintFile(base::FilePath(kAltWidevineDirectory),
                                        base::Version("0.1")));
  EXPECT_EQ(GetHintedWidevineCdmDirectory().value(), kAltWidevineDirectory);
  EXPECT_EQ(GetBundledVersionDuringLastComponentUpdate(), base::Version("0.1"));
}

TEST(ComponentWidevineHintFileTest, CorruptHintFile) {
  const base::ScopedPathOverride path_override(chrome::DIR_USER_DATA);

  CreateWidevineComponentUpdatedDirectory();
  base::FilePath hint_file_path;
  EXPECT_TRUE(base::PathService::Get(chrome::FILE_COMPONENT_WIDEVINE_CDM_HINT,
                                     &hint_file_path));
  EXPECT_TRUE(base::ImportantFileWriter::WriteFileAtomically(
      hint_file_path, "{not_what_is_expected}"));
  EXPECT_TRUE(GetHintedWidevineCdmDirectory().empty());
  EXPECT_FALSE(GetBundledVersionDuringLastComponentUpdate().has_value());
}

TEST(ComponentWidevineHintFileTest, MissingPath) {
  const base::ScopedPathOverride path_override(chrome::DIR_USER_DATA);

  CreateWidevineComponentUpdatedDirectory();
  base::FilePath hint_file_path;
  EXPECT_TRUE(base::PathService::Get(chrome::FILE_COMPONENT_WIDEVINE_CDM_HINT,
                                     &hint_file_path));
  EXPECT_TRUE(base::ImportantFileWriter::WriteFileAtomically(
      hint_file_path, "{\"One\": true}"));
  EXPECT_TRUE(GetHintedWidevineCdmDirectory().empty());
  EXPECT_FALSE(GetBundledVersionDuringLastComponentUpdate().has_value());
}

TEST(ComponentWidevineHintFileTest, ExtraFields) {
  const base::ScopedPathOverride path_override(chrome::DIR_USER_DATA);

  CreateWidevineComponentUpdatedDirectory();
  base::FilePath hint_file_path;
  EXPECT_TRUE(base::PathService::Get(chrome::FILE_COMPONENT_WIDEVINE_CDM_HINT,
                                     &hint_file_path));
  EXPECT_TRUE(base::ImportantFileWriter::WriteFileAtomically(
      hint_file_path,
      "{\"One\": true, \"Path\": \"WidevineCdmInstalledHere\", \"Two\": {}, "
      "\"LastBundledVersion\": \"2.0\"}"));
  EXPECT_EQ(GetHintedWidevineCdmDirectory().value(),
            "WidevineCdmInstalledHere");
  EXPECT_EQ(GetBundledVersionDuringLastComponentUpdate(), base::Version("2.0"));
}
