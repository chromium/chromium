// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/minidump_uploader/rewrite_minidumps_as_mimes.h"

#include "components/crash/android/anr_build_id_provider.h"
#include "components/version_info/version_info.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/crashpad/crashpad/util/file/string_file.h"

namespace minidump_uploader {
TEST(RewriteMinidumpsAsMimesTest, FakeVersionAndBuildIdProvided) {
  crashpad::StringFile writer;
  std::string fake_build_id = "fakebuildid";
  WriteAnrAsMime(nullptr, &writer, "111.0.fake.123", fake_build_id, "");
  EXPECT_NE(writer.string().find("elf_build_id"), std::string::npos);
  EXPECT_NE(writer.string().find("fakebuildid"), std::string::npos);
}

TEST(RewriteMinidumpsAsMimesTest, RealVersionFakeBuildIdProvided) {
  crashpad::StringFile writer;
  std::string fake_build_id = "fakebuildid";
  WriteAnrAsMime(nullptr, &writer,
                 std::string(version_info::GetVersionNumber()), fake_build_id,
                 "");
  EXPECT_NE(writer.string().find("elf_build_id"), std::string::npos);
  EXPECT_NE(writer.string().find("fakebuildid"), std::string::npos);
}

TEST(RewriteMinidumpsAsMimesTest, FakeVersionNoBuildIdProvided) {
  crashpad::StringFile writer;
  std::string real_build_id = crash_reporter::GetElfBuildId();
  WriteAnrAsMime(nullptr, &writer, "111.0.fake.123", "", "");
  EXPECT_EQ(writer.string().find("elf_build_id"), std::string::npos);
}

TEST(RewriteMinidumpsAsMimesTest, RealVersionNoBuildIdProvided) {
  crashpad::StringFile writer;
  std::string real_build_id = crash_reporter::GetElfBuildId();
  WriteAnrAsMime(nullptr, &writer,
                 std::string(version_info::GetVersionNumber()), "", "");
  EXPECT_NE(writer.string().find("elf_build_id"), std::string::npos);
  EXPECT_NE(writer.string().find(real_build_id), std::string::npos);
}
}  // namespace minidump_uploader
