// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/logging/detailed_info_sampler.h"

#include "base/files/file_path.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_cleaner {

namespace {

const wchar_t kBinaryPath1[] = L"C:\\Folder\\Binary1.exe";
const wchar_t kBinaryPath2[] = L"C:\\Folder\\Binary2.exe";
const wchar_t kBinaryPath3[] = L"C:\\Folder\\Binary3.exe";
const wchar_t kBinaryPath4[] = L"C:\\Folder\\Binary4.exe";
const unsigned int kMaxNumberOfFiles = 3;

}  // namespace

TEST(DetailedInfoSamplingTest, SampleOneFile) {
  FilePathSet files_to_sample;
  files_to_sample.Insert(base::FilePath(kBinaryPath1));

  FilePathSet selected_paths;
  DetailedInfoSampler sampler(kMaxNumberOfFiles);
  sampler.SelectPathSetToSample(files_to_sample, &selected_paths);
  EXPECT_EQ(1UL, selected_paths.size());
}

TEST(DetailedInfoSamplingTest, SampleMaxFiles) {
  FilePathSet files_to_sample;
  files_to_sample.Insert(base::FilePath(kBinaryPath1));
  files_to_sample.Insert(base::FilePath(kBinaryPath2));
  files_to_sample.Insert(base::FilePath(kBinaryPath3));
  files_to_sample.Insert(base::FilePath(kBinaryPath4));

  ASSERT_LT(kMaxNumberOfFiles, files_to_sample.size());
  DetailedInfoSampler sampler(kMaxNumberOfFiles);
  FilePathSet selected_paths;
  sampler.SelectPathSetToSample(files_to_sample, &selected_paths);

  EXPECT_EQ(kMaxNumberOfFiles, selected_paths.size());
  EXPECT_EQ(4UL, files_to_sample.size());
  // Count how many of the original list were found in out newly populated set.
  size_t nb_found = 0;
  for (const auto& path : files_to_sample.ToVector()) {
    if (selected_paths.Contains(path))
      nb_found++;
  }
  EXPECT_EQ(kMaxNumberOfFiles, nb_found);
}

TEST(DetailedInfoSamplingTest, SampleEmptyVector) {
  FilePathSet files_to_sample;
  FilePathSet selected_paths;
  DetailedInfoSampler sampler(kMaxNumberOfFiles);
  sampler.SelectPathSetToSample(files_to_sample, &selected_paths);
  EXPECT_EQ(0UL, selected_paths.size());
}

}  // namespace chrome_cleaner
