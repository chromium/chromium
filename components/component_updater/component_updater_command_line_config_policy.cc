// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/component_updater_command_line_config_policy.h"

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/sys_string_conversions.h"
#include "build/build_config.h"
#include "components/component_updater/component_updater_switches.h"

namespace component_updater {

namespace {

// Debug values you can pass to --component-updater=value1,value2. Do not
// use these values in production code.

// Speed up the initial component checking.
const char kSwitchFastUpdate[] = "fast-update";

// Add "testrequest=1" attribute to the update check request.
const char kSwitchTestRequestParam[] = "test-request";

// Disables pings. Pings are the requests sent to the update server that report
// the success or the failure of component install or update attempts.
const char kSwitchDisablePings[] = "disable-pings";

// Sets the URL for updates.
const char kSwitchUrlSource[] = "url-source";

// Disables differential updates.
const char kSwitchDisableDeltaUpdates[] = "disable-delta-updates";

// Configures the initial delay before the first component update check. The
// value is in seconds.
const char kInitialDelay[] = "initial-delay";

#if defined(OS_WIN)
// Disables background downloads.
const char kSwitchDisableBackgroundDownloads[] = "disable-background-downloads";
#endif  // defined(OS_WIN)

// If there is an element of |vec| of the form |test|=.*, returns the right-
// hand side of that assignment. Otherwise, returns an empty string.
// The right-hand side may contain additional '=' characters, allowing for
// further nesting of switch arguments.
std::string GetSwitchArgument(const std::vector<std::string>& vec,
                              const char* test) {
  if (vec.empty())
    return std::string();
  for (auto it = vec.begin(); it != vec.end(); ++it) {
    const std::size_t found = it->find("=");
    if (found != std::string::npos) {
      if (it->substr(0, found) == test) {
        return it->substr(found + 1);
      }
    }
  }
  return std::string();
}

}  // namespace

ComponentUpdaterCommandLineConfigPolicy::
    ComponentUpdaterCommandLineConfigPolicy(const base::CommandLine* cmdline) {
  DCHECK(cmdline);
  // Parse comma-delimited debug flags.
  std::vector<std::string> switch_values = base::SplitString(
      cmdline->GetSwitchValueASCII(switches::kComponentUpdater), ",",
      base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

#if defined(OS_WIN)
  background_downloads_enabled_ =
      !base::Contains(switch_values, kSwitchDisableBackgroundDownloads);
#else
  background_downloads_enabled_ = false;
#endif

  deltas_enabled_ = !base::Contains(switch_values, kSwitchDisableDeltaUpdates);
  fast_update_ = base::Contains(switch_values, kSwitchFastUpdate);
  pings_enabled_ = !base::Contains(switch_values, kSwitchDisablePings);
  test_request_ = base::Contains(switch_values, kSwitchTestRequestParam);

  const std::string switch_url_source =
      GetSwitchArgument(switch_values, kSwitchUrlSource);
  if (!switch_url_source.empty()) {
    url_source_override_ = GURL(switch_url_source);
    DCHECK(url_source_override_.is_valid());
  }

  const std::string initial_delay =
      GetSwitchArgument(switch_values, kInitialDelay);
  int initial_delay_seconds = 0;
  if (base::StringToInt(initial_delay, &initial_delay_seconds))
    initial_delay_ = initial_delay_seconds;
}

bool ComponentUpdaterCommandLineConfigPolicy::BackgroundDownloadsEnabled()
    const {
  return background_downloads_enabled_;
}

bool ComponentUpdaterCommandLineConfigPolicy::DeltaUpdatesEnabled() const {
  return deltas_enabled_;
}

bool ComponentUpdaterCommandLineConfigPolicy::FastUpdate() const {
  return fast_update_;
}

bool ComponentUpdaterCommandLineConfigPolicy::PingsEnabled() const {
  return pings_enabled_;
}

bool ComponentUpdaterCommandLineConfigPolicy::TestRequest() const {
  return test_request_;
}

GURL ComponentUpdaterCommandLineConfigPolicy::UrlSourceOverride() const {
  return url_source_override_;
}

int ComponentUpdaterCommandLineConfigPolicy::InitialDelay() const {
  return initial_delay_;
}

}  // namespace component_updater
