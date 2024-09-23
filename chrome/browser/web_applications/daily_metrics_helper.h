// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_DAILY_METRICS_HELPER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_DAILY_METRICS_HELPER_H_

#include <optional>

#include "base/time/time.h"
#include "build/build_config.h"
#include "url/gurl.h"

class PrefRegistrySimple;
class Profile;

namespace web_app {

struct DailyInteraction {
  // Required.
  // TODO(crbug.com/40267110): Use manifest_identity_url here instead of
  // start_url.
  GURL start_url;
  // Implied bool used = true;
  bool installed = false;
  std::optional<int> install_source;
  int effective_display_mode = 0;
  bool captures_links = false;
  bool promotable = false;
  // Durations and sessions emitted iff non-zero.
  base::TimeDelta foreground_duration;
  base::TimeDelta background_duration;
  int num_sessions = 0;

  DailyInteraction();
  explicit DailyInteraction(GURL start_url);
  DailyInteraction(const DailyInteraction&);
  ~DailyInteraction();
};

// Emits UKM metrics for existing records if the date has changed, removing them
// from storage. Then stores the given record, updating any stored values for
// that start_url (ie. replacing or summing as appropriate).
void FlushOldRecordsAndUpdate(DailyInteraction& record, Profile* profile);

// Emits UKM metrics for all existing records. Note that this is asynchronous
// unless |SkipOriginCheckForTesting| has been called.
void FlushAllRecordsForTesting(Profile* profile);

// Skip the origin check, which is async and requires a history service.
void SkipOriginCheckForTesting();

void RegisterDailyWebAppMetricsProfilePrefs(PrefRegistrySimple* registry);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_DAILY_METRICS_HELPER_H_
