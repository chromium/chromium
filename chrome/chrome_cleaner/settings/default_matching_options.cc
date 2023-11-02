// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/settings/default_matching_options.h"

#include "chrome/chrome_cleaner/settings/settings.h"

namespace chrome_cleaner {

// For the cleaner we should always perform full scan so we know exactly what is
// found, even for report only UwS.
MatchingOptions DefaultCleanerMatchingOptions() {
  MatchingOptions options;
  options.set_only_one_footprint(false);
  return options;
}

// For the reporter, we should only perform a full scan if we report back to
// Google what files and registry entries were matched. Otherwise, we should
// stop looking as soon as any piece of the UwS is found, since we just care if
// it is present.
MatchingOptions DefaultReporterMatchingOptions() {
  MatchingOptions options;
  options.set_only_one_footprint(
      !Settings::GetInstance()->logs_collection_enabled());
  return options;
}

}  // namespace chrome_cleaner
