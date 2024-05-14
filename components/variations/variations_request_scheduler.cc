// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_request_scheduler.h"

#include <stddef.h>

#include <optional>

#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "components/variations/variations_associated_data.h"
#include "components/variations/variations_switches.h"

namespace variations {
namespace {

// The minimum time between consecutive variations seed fetches.
constexpr base::TimeDelta kVariationsSeedFetchIntervalMinimum =
    base::Minutes(1);

// Returns the variations seed fetch interval specified through the
// |kVariationsSeedFetchInterval| switch. The value returned is subject to a
// minimum value, |kVariationsSeedFetchIntervalMinimum|. If no overridden value
// is specified, or if it is malformed, an empty optional is returned.
std::optional<base::TimeDelta> GetOverriddenFetchPeriod() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  const std::string switch_value =
      command_line->GetSwitchValueASCII(switches::kVariationsSeedFetchInterval);

  if (switch_value.empty())
    return std::nullopt;

  int overridden_period;
  if (!base::StringToInt(switch_value, &overridden_period)) {
    LOG(DFATAL) << "Malformed value for --"
                << switches::kVariationsSeedFetchInterval << ". "
                << "Expected int, got: " << switch_value;
    return std::nullopt;
  }

  return std::max(base::Minutes(overridden_period),
                  kVariationsSeedFetchIntervalMinimum);
}

}  // namespace

VariationsRequestScheduler::VariationsRequestScheduler(
    const base::RepeatingClosure& task)
    : task_(task) {}

VariationsRequestScheduler::~VariationsRequestScheduler() = default;

void VariationsRequestScheduler::Start() {
  task_.Run();
  timer_.Start(FROM_HERE, GetFetchPeriod(), task_);
}

void VariationsRequestScheduler::Reset() {
  if (timer_.IsRunning())
    timer_.Reset();
  one_shot_timer_.Stop();
}

void VariationsRequestScheduler::ScheduleFetchShortly() {
  // Reset the regular timer to avoid it triggering soon after.
  Reset();
  // The delay before attempting a fetch shortly.
  base::TimeDelta fetch_shortly_delay = base::Minutes(5);

  // If there is a fetch interval specified in the command line, and it is
  // shorter than |fetch_shortly_delay|, use it instead.
  std::optional<base::TimeDelta> overridden_period = GetOverriddenFetchPeriod();
  if (overridden_period.has_value()) {
    fetch_shortly_delay =
        std::min(fetch_shortly_delay, overridden_period.value());
  }

  one_shot_timer_.Start(FROM_HERE, fetch_shortly_delay, task_);
}

void VariationsRequestScheduler::OnAppEnterForeground() {
  NOTREACHED_IN_MIGRATION()
      << "Attempted to OnAppEnterForeground on non-mobile device";
}

base::TimeDelta VariationsRequestScheduler::GetFetchPeriod() const {
  // The fetch interval can be overridden through the command line.
  std::optional<base::TimeDelta> overridden_period = GetOverriddenFetchPeriod();
  if (overridden_period.has_value())
    return overridden_period.value();

  // The fetch interval can be overridden by a variation param.
  std::string period_min_str = base::GetFieldTrialParamValue(
      "VariationsServiceControl", "fetch_period_min");
  size_t period_min;
  if (base::StringToSizeT(period_min_str, &period_min))
    return base::Minutes(period_min);

  // The default fetch interval is every 30 minutes.
  return base::Minutes(30);
}

base::RepeatingClosure VariationsRequestScheduler::task() const {
  return task_;
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
// static
VariationsRequestScheduler* VariationsRequestScheduler::Create(
    const base::RepeatingClosure& task,
    PrefService* local_state) {
  return new VariationsRequestScheduler(task);
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

}  // namespace variations
