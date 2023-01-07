// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/daily_event.h"

#include <utility>

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace metrics {

namespace {

void RecordIntervalTypeHistogram(const std::string& histogram_name,
                                 DailyEvent::IntervalType type) {
  if (histogram_name.empty())
    return;
  base::UmaHistogramEnumeration(histogram_name, type);
}

}  // namespace

DailyEvent::Observer::Observer() {
}

DailyEvent::Observer::~Observer() {
}

DailyEvent::DailyEvent(PrefService* pref_service,
                       const char* pref_name,
                       const std::string& histogram_name)
    : pref_service_(pref_service),
      pref_name_(pref_name),
      histogram_name_(histogram_name) {
}

DailyEvent::~DailyEvent() {
}

// static
void DailyEvent::RegisterPref(PrefRegistrySimple* registry,
                              const std::string& pref_name) {
  registry->RegisterInt64Pref(pref_name, 0);
}

void DailyEvent::AddObserver(std::unique_ptr<DailyEvent::Observer> observer) {
  DVLOG(2) << "DailyEvent observer added.";
  DCHECK(last_fired_.is_null());
  observers_.push_back(std::move(observer));
}

void DailyEvent::CheckInterval() {
  base::Time now = base::Time::Now();
  if (last_fired_.is_null()) {
    // The first time we call CheckInterval, we read the time stored in prefs.
    last_fired_ =
        base::Time() + base::Microseconds(pref_service_->GetInt64(pref_name_));

    DVLOG(1) << "DailyEvent time loaded: " << last_fired_;
    if (last_fired_.is_null()) {
      DVLOG(1) << "DailyEvent first run.";
      RecordIntervalTypeHistogram(histogram_name_, IntervalType::FIRST_RUN);
      OnInterval(now, IntervalType::FIRST_RUN);
      return;
    }
  }
  int days_elapsed = (now - last_fired_).InDays();
  if (days_elapsed >= 1) {
    DVLOG(1) << "DailyEvent day elapsed.";
    RecordIntervalTypeHistogram(histogram_name_, IntervalType::DAY_ELAPSED);
    OnInterval(now, IntervalType::DAY_ELAPSED);
  } else if (days_elapsed <= -1) {
    // The "last fired" time is more than a day in the future, so the clock
    // must have been changed.
    DVLOG(1) << "DailyEvent clock change detected.";
    RecordIntervalTypeHistogram(histogram_name_, IntervalType::CLOCK_CHANGED);
    OnInterval(now, IntervalType::CLOCK_CHANGED);
  }
}

void DailyEvent::OnInterval(base::Time now, IntervalType type) {
  DCHECK(!now.is_null());
  last_fired_ = now;
  pref_service_->SetInt64(pref_name_,
                          last_fired_.since_origin().InMicroseconds());

  for (auto it = observers_.begin(); it != observers_.end(); ++it) {
    (*it)->OnDailyEvent(type);
  }
}

}  // namespace metrics
