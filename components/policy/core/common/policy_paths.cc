// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_paths.h"

#include "build/branding_buildflags.h"
#include "build/build_config.h"

namespace policy {

// Directory for system-wide read-only policy files that allow sys-admins to set
// policies for the browser.
// Intentionally not using base::FilePath to minimize dependencies.
#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC)
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
const char kPolicyPath[] = "/etc/opt/chrome/policies";
#elif BUILDFLAG(GOOGLE_CHROME_FOR_TESTING_BRANDING)
const char kPolicyPath[] = "/etc/opt/chrome_for_testing/policies";
#else
const char kPolicyPath[] = "/etc/chromium/policies";
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
#endif  // BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC)

}  // namespace policy
