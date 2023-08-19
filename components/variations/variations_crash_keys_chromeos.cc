// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_crash_keys_chromeos.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"

namespace variations {

namespace {

// Path where we put variations in cryptohome.
#if BUILDFLAG(IS_CHROMEOS_ASH)
constexpr char kCrashVariationsFileName[] = ".variations-list.txt";
#endif  // IS_CHROMEOS_ASH
#if BUILDFLAG(IS_CHROMEOS_LACROS)
constexpr char kCrashVariationsFileName[] = ".variations-list-lacros.txt";
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

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

void ReportVariationsToChromeOs(scoped_refptr<base::SequencedTaskRunner> runner,
                                ExperimentListInfo info) {
  // On a thread in the background, write variants to a file.
  runner->PostTask(FROM_HERE, base::BindOnce(&WriteVariationsToFile, info));
}

}  // namespace variations
