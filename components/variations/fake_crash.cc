// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/fake_crash.h"

#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"

namespace {
// This feature causes a crash report to be created after startup (without
// actually crashing). This is used for verifying safety measures that help
// catch features that cause real crashes.
BASE_FEATURE(kVariationsFakeCrashAfterStartup,
             "VariationsFakeCrashAfterStartup",
             base::FEATURE_DISABLED_BY_DEFAULT);
}  // namespace

namespace variations {

void MaybeScheduleFakeCrash() {
  if (base::FeatureList::IsEnabled(kVariationsFakeCrashAfterStartup)) {
    base::ThreadPool::PostDelayedTask(
        FROM_HERE, base::BindOnce([]() {
          LOG(ERROR) << "Creating dump for VariationsFakeCrashAfterStartup";
          base::debug::DumpWithoutCrashing();
        }),
        base::Seconds(15));
  }
}

}  // namespace variations