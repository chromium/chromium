// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/profiler/core_unwinders.h"

#include "base/profiler/core_unwinders.h"
#include "build/build_config.h"

static_assert(!BUILDFLAG(IS_ANDROID),
              "Android platform should use core_unwinders_android.cc instead");

bool AreUnwindPrerequisitesAvailable(
    version_info::Channel channel,
    UnwindPrerequisitesDelegate* prerequites_delegate) {
  return true;
}

void RequestUnwindPrerequisitesInstallation(
    version_info::Channel channel,
    UnwindPrerequisitesDelegate* prerequites_delegate) {}

base::StackSamplingProfiler::UnwindersFactory CreateCoreUnwindersFactory() {
  // Delegate to the base implementation for non-Android.
  return base::CreateCoreUnwindersFactory();
}
