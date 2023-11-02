// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/logging/detailed_info_sampler.h"

#include <algorithm>
#include <random>
#include <vector>

#include "base/files/file_path.h"
#include "base/rand_util.h"
#include "chrome/chrome_cleaner/os/disk_util.h"

namespace chrome_cleaner {

DetailedInfoSampler::DetailedInfoSampler(int max_files)
    : max_files_(max_files) {}

void DetailedInfoSampler::SelectPathSetToSample(const FilePathSet& file_paths,
                                                FilePathSet* paths_to_sample) {
  std::vector<base::FilePath> active_paths = file_paths.ToVector();

  std::shuffle(active_paths.begin(), active_paths.end(),
               std::default_random_engine(base::RandDouble()));

  // There might already be some files in |path_to_sample|, hence we use
  // |selected_files| to count how many files we are adding.
  int selected_files = 0;
  for (auto it = active_paths.begin();
       selected_files < max_files_ && it != active_paths.end(); it++) {
    paths_to_sample->Insert(*it);
    selected_files++;
  }
}

}  // namespace chrome_cleaner
