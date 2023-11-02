// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_LOGGING_DETAILED_INFO_SAMPLER_H_
#define CHROME_CHROME_CLEANER_LOGGING_DETAILED_INFO_SAMPLER_H_

#include "chrome/chrome_cleaner/logging/info_sampler.h"
#include "chrome/chrome_cleaner/os/file_path_set.h"

namespace chrome_cleaner {

class DetailedInfoSampler : public InfoSampler {
 public:
  // Maximum number of files for which we will collect detailed information by
  // default.
  static constexpr int kDefaultMaxFiles = 5;

  explicit DetailedInfoSampler(int max_files);
  void SelectPathSetToSample(const FilePathSet& file_paths,
                             FilePathSet* paths_to_sample) override;

 private:
  int max_files_;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_LOGGING_DETAILED_INFO_SAMPLER_H_
