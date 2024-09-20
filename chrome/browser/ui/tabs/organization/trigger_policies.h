// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_ORGANIZATION_TRIGGER_POLICIES_H_
#define CHROME_BROWSER_UI_TABS_ORGANIZATION_TRIGGER_POLICIES_H_

#include "chrome/browser/ui/tabs/organization/trigger.h"

#include <memory>

#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "chrome/browser/metrics/desktop_session_duration/desktop_session_duration_tracker.h"

namespace content {
class BrowserContext;
}
class PrefService;

// We want to parameterize trigger policies with things like target triggering
// frequencies. That begs the question - how should we define those frequencies?
// Once per week? Once every 8 hours of active chrome usage? Once per 200 tabs
// opened?

// A simple way to model this is with clocks that measure time differently. Some
// clock options, in increasing complexity order:

// 1. Wall time. Simple and predictable, but could spam users who don't often
// use Chrome and under-serve users who use Chrome frequently.

// 2. Chrome foreground time, implemented below. Still fairly simple, and maps
// better to actual usage, but fails for cases like streaming a movie or leaving
// a computer on overnight.

// 3. Number of browser actions of some kind, e.g. page loads. Weirder to think
// about, some risk of degenerate behavior for unusual usage patterns, but can
// map very directly to e.g. tabstrip usage.

// 4. The above, but deduplicate events performed in quick succession. This
// effectively amounts to defining our own notion of 'active tabstrip time'.
// Fixes the issues with 3 but might just be overkill.

// A clock that runs only while Chrome is in the foreground. See also
// chrome/browser/resource_coordinator/usage_clock.h which implements the same
// concept in a resource_coordinator specific way.
class UsageTickClock final : public base::TickClock,
                             metrics::DesktopSessionDurationTracker::Observer {
 public:
  explicit UsageTickClock(const base::TickClock* base_clock);

  UsageTickClock(const UsageTickClock&) = delete;
  UsageTickClock& operator=(const UsageTickClock&) = delete;

  ~UsageTickClock() override;

  base::TimeTicks NowTicks() const override;

 private:
  void OnSessionStarted(base::TimeTicks session_start) override;
  void OnSessionEnded(base::TimeDelta session_length,
                      base::TimeTicks session_end) override;

  const raw_ptr<const base::TickClock> base_clock_;
  const base::TimeTicks start_time_;

  base::TimeDelta usage_time_in_completed_sessions_ = base::TimeDelta();
  std::optional<base::TimeTicks> current_usage_session_start_time_ =
      std::nullopt;
};

class BackoffLevelProvider {
 public:
  virtual ~BackoffLevelProvider() = default;
  virtual unsigned int Get() const = 0;
  virtual void Increment() = 0;
  virtual void Decrement() = 0;
};

class ProfilePrefBackoffLevelProvider final : public BackoffLevelProvider {
 public:
  explicit ProfilePrefBackoffLevelProvider(content::BrowserContext* context);
  ~ProfilePrefBackoffLevelProvider() override;

  // BackoffLevelProvider:
  unsigned int Get() const override;
  void Increment() override;
  void Decrement() override;

 private:
  raw_ptr<PrefService> prefs_;
};

// A policy which triggers up to once per period, based on the classic solution
// to the secretary problem. Has an observation phase and a trigger phase.
// During the observation phase, it keeps track of the best score seen so far.
// During the trigger phase, it triggers the first time the best score from the
// observation phase is beaten.
//
// For any given period, it has a 1/e chance to not trigger at all, but the rest
// of the time it will likely trigger on a very good moment, relative to the
// other moments in this period.
class TargetFrequencyTriggerPolicy final : public TriggerPolicy {
 public:
  TargetFrequencyTriggerPolicy(std::unique_ptr<base::TickClock> clock,
                               base::TimeDelta base_period,
                               float backoff_base,
                               BackoffLevelProvider* backoff_level_provider);
  ~TargetFrequencyTriggerPolicy() override;
  bool ShouldTrigger(float score) override;
  void OnTriggerSucceeded();
  void OnTriggerFailed();

 private:
  const std::unique_ptr<base::TickClock> clock_;
  const base::TimeDelta base_period_;
  const float backoff_base_;
  const raw_ptr<BackoffLevelProvider> backoff_level_provider_;

  base::TimeTicks cycle_start_time_;
  std::optional<float> best_score = std::nullopt;
  bool has_triggered_ = false;
};

// Never trigger. Useful for disabling the trigger under certain conditions.
class NeverTriggerPolicy final : public TriggerPolicy {
 public:
  bool ShouldTrigger(float score) override;
};

// Trigger every time. Very spammy, but suitable for testing or demoing.
class DemoTriggerPolicy final : public TriggerPolicy {
 public:
  bool ShouldTrigger(float score) override;
};

#endif  // CHROME_BROWSER_UI_TABS_ORGANIZATION_TRIGGER_POLICIES_H_
