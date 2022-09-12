// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/base/file_icon_util.h"

#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

TEST(FileIconUtilTest, GetIconTypeForPath) {
  const std::vector<std::pair<std::string, IconType>> file_path_to_icon_type = {
      {"/my/test/file.pdf", IconType::kPdf},
      {"/my/test/file.Pdf", IconType::kPdf},

      {"/my/test/file.7z", IconType::kArchive},
      {"/my/test/file.bz", IconType::kArchive},
      {"/my/test/file.bz2", IconType::kArchive},
      {"/my/test/file.crx", IconType::kArchive},
      {"/my/test/file.gz", IconType::kArchive},
      {"/my/test/file.iso", IconType::kArchive},
      {"/my/test/file.lz", IconType::kArchive},
      {"/my/test/file.lzma", IconType::kArchive},
      {"/my/test/file.lzo", IconType::kArchive},
      {"/my/test/file.rar", IconType::kArchive},
      {"/my/test/file.tar", IconType::kArchive},
      {"/my/test/file.taz", IconType::kArchive},
      {"/my/test/file.tb2", IconType::kArchive},
      {"/my/test/file.tbz", IconType::kArchive},
      {"/my/test/file.tbz2", IconType::kArchive},
      {"/my/test/file.tgz", IconType::kArchive},
      {"/my/test/file.tlz", IconType::kArchive},
      {"/my/test/file.tlzma", IconType::kArchive},
      {"/my/test/file.txz", IconType::kArchive},
      {"/my/test/file.tz", IconType::kArchive},
      {"/my/test/file.tz2", IconType::kArchive},
      {"/my/test/file.tzst", IconType::kArchive},
      {"/my/test/file.xz", IconType::kArchive},
      {"/my/test/file.z", IconType::kArchive},
      {"/my/test/file.zip", IconType::kArchive},

      {"/my/test/.gslides", IconType::kGslide},
      {"/my/test/noextension", IconType::kGeneric},
      {"/my/test/file.missing", IconType::kGeneric}};

  for (const auto& pair : file_path_to_icon_type) {
    EXPECT_EQ(internal::GetIconTypeForPath(base::FilePath(pair.first)),
              pair.second);
  }
}

}  // namespace chromeos
