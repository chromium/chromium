// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_crash_keys_chromeos.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/task/sequenced_task_runner.h"

namespace variations {

namespace {

// Path where we put variations in cryptohome.
constexpr char kCrashVariationsFileName[] = ".variations-list.txt";

void WriteVariationsToFile(ExperimentListInfo info) {
  std::string combined_string = base::StrCat(
      {kNumExperimentsKey, "=", base::NumberToString(info.num_experiments),
       "\n", kExperimentListKey, "=", info.experiment_list, "\n"});

  base::FilePath path = base::PathService::CheckedGet(base::DIR_HOME);
  if (path == base::FilePath("/")) {
    // Fallback to /home/chronos if DIR_HOME is not overridden and
    // no user has signed in.
    path = base::FilePath("/home/chronos");
  }

  path = path.Append(kCrashVariationsFileName);

  if (!base::WriteFile(path, combined_string)) {
    VLOG(1) << "Failed to write " << path.value();
  }
}

}  // namespace

void ReportVariationsToChromeOs(base::CancelableTaskTracker* tracker,
                                scoped_refptr<base::SequencedTaskRunner> runner,
                                ExperimentListInfo info) {
  // Since a subsequent call to WriteVariationsToFile() overwrites the value
  // saved by an earlier call, cancel any previously posted tasks that have not
  // started running yet before posting the latest snapshot.
  tracker->TryCancelAll();
  tracker->PostTask(runner.get(), FROM_HERE,
                    base::BindOnce(&WriteVariationsToFile, std::move(info)));
}

}  // namespace variations
