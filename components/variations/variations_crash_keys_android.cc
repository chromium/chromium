// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_crash_keys_android.h"

#include "base/android/build_info.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/command_line.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "components/crash/android/anr_build_id_provider.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/crash/android/anr_collector_jni_headers/AnrCollector_jni.h"

namespace {

// This is defined in content/public/common/content_switches.h, which cannot be
// included here because of cyclic dependencies.
constexpr const char kProcessTypeSwitchName[] = "type";

// Turn the variations::ExperimentListInfo struct into a string and pass it to
// the JNI function, which will save the string to a file on disk and save the
// hash of the string to the process state summary, which will be queried later
// for ANR reporting purposes.
void SaveVariations(variations::ExperimentListInfo info) {
  std::string combined_variations_string = base::StrCat(
      {base::NumberToString(info.num_experiments), "\n", info.experiment_list});
  std::string elf_build_id = crash_reporter::GetElfBuildId();

  JNIEnv* env = base::android::AttachCurrentThread();
  anr_collector::Java_AnrCollector_saveVariations(
      env, combined_variations_string, elf_build_id);
}

}  // namespace

namespace variations {

void SaveVariationsForAnrReporting(
    base::CancelableTaskTracker* tracker,
    scoped_refptr<base::SequencedTaskRunner> runner,
    ExperimentListInfo info) {
  // ANR collection and reporting is only available on R and above.
  bool sdk_version_enough =
      base::android::BuildInfo::GetInstance()->sdk_int() >=
      base::android::SDK_VERSION_R;
  bool is_browser_process = base::CommandLine::ForCurrentProcess()
                                ->GetSwitchValueASCII(kProcessTypeSwitchName)
                                .empty();
  if (sdk_version_enough && is_browser_process) {
    // Since a second call to SaveVariations() overwrites the value saved by the
    // first call, we can safely cancel all the previously posted tasks here.
    tracker->TryCancelAll();
    tracker->PostTask(runner.get(), FROM_HERE,
                      base::BindOnce(&SaveVariations, info));
  }
}

}  // namespace variations
