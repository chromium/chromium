// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_SETTINGS_DEFAULT_MATCHING_OPTIONS_H_
#define CHROME_CHROME_CLEANER_SETTINGS_DEFAULT_MATCHING_OPTIONS_H_

#include "chrome/chrome_cleaner/settings/matching_options.h"

namespace chrome_cleaner {

// Return the default MatchingOptions for the cleaner and the reporter.
MatchingOptions DefaultCleanerMatchingOptions();
MatchingOptions DefaultReporterMatchingOptions();

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_SETTINGS_DEFAULT_MATCHING_OPTIONS_H_
