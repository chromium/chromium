// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/tools/certificate_tag.h"
#include "base/base_paths.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zlib/google/compression_utils.h"

namespace updater {
namespace tools {

TEST(CertificateTag, RoundTrip) {
  base::FilePath source_path;
  ASSERT_TRUE(base::PathService::Get(base::DIR_SOURCE_ROOT, &source_path));
  const base::FilePath exe_path = source_path.AppendASCII("chrome")
                                      .AppendASCII("updater")
                                      .AppendASCII("test")
                                      .AppendASCII("data")
                                      .AppendASCII("signed.exe.gz");
  std::string exe;
  ASSERT_TRUE(base::ReadFileToString(exe_path, &exe));
  ASSERT_TRUE(compression::GzipUncompress(exe, &exe));
  const base::span<const uint8_t> exe_span(
      reinterpret_cast<const uint8_t*>(exe.data()), exe.size());

  absl::optional<Binary> bin(Binary::Parse(exe_span));
  ASSERT_TRUE(bin);

  // Binary should be untagged on disk.
  absl::optional<base::span<const uint8_t>> orig_tag(bin->tag());
  EXPECT_FALSE(orig_tag);

  static const uint8_t kTag[] = {1, 2, 3, 4, 5};
  absl::optional<std::vector<uint8_t>> updated_exe(bin->SetTag(kTag));
  ASSERT_TRUE(updated_exe);

  absl::optional<Binary> bin2(Binary::Parse(*updated_exe));
  ASSERT_TRUE(bin2);
  absl::optional<base::span<const uint8_t>> parsed_tag(bin2->tag());
  ASSERT_TRUE(parsed_tag);
  EXPECT_TRUE(parsed_tag->size() == sizeof(kTag) &&
              memcmp(kTag, parsed_tag->data(), sizeof(kTag)) == 0);

  // Update an existing tag.
  static const uint8_t kTag2[] = {1, 2, 3, 4, 6};
  absl::optional<std::vector<uint8_t>> updated_again_exe(bin2->SetTag(kTag2));
  ASSERT_TRUE(updated_again_exe);

  absl::optional<Binary> bin3(Binary::Parse(*updated_again_exe));
  ASSERT_TRUE(bin3);
  absl::optional<base::span<const uint8_t>> parsed_tag2(bin3->tag());
  ASSERT_TRUE(parsed_tag2);
  EXPECT_TRUE(parsed_tag2->size() == sizeof(kTag2) &&
              memcmp(kTag2, parsed_tag2->data(), sizeof(kTag2)) == 0);

  // Updating an existing tag with a tag of the same size should not have grown
  // the binary, i.e. the old tag should have been erased first.
  EXPECT_EQ(sizeof(kTag), sizeof(kTag2));
  EXPECT_EQ(updated_exe->size(), updated_again_exe->size());
}

}  // namespace tools
}  // namespace updater
