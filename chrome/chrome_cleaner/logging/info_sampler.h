// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_LOGGING_INFO_SAMPLER_H_
#define CHROME_CHROME_CLEANER_LOGGING_INFO_SAMPLER_H_

#include "chrome/chrome_cleaner/os/file_path_set.h"

namespace chrome_cleaner {

class InfoSampler {
 public:
  virtual void SelectPathSetToSample(const FilePathSet& file_paths,
                                     FilePathSet* sampled_file_paths) = 0;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_LOGGING_INFO_SAMPLER_H_
