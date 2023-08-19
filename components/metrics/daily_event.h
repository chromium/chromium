// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_DAILY_EVENT_H_
#define COMPONENTS_METRICS_DAILY_EVENT_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"

class PrefRegistrySimple;
class PrefService;

namespace metrics {

// DailyEvent is used for throttling an event to about once per day, even if
// the program is restarted more frequently.  It is based on local machine
// time, so it could be fired more often if the clock is changed.
//
// The service using the DailyEvent should first provide all of the Observers
// for the interval, and then arrange for CheckInterval() to be called
// periodically to test if the event should be fired.
class DailyEvent {
 public:
  // Different reasons that Observer::OnDailyEvent() is called.
  // This enum is used for histograms and must not be renumbered.
  enum class IntervalType {
    FIRST_RUN,
    DAY_ELAPSED,
    CLOCK_CHANGED,
    kMaxValue = CLOCK_CHANGED,
  };

  // Observer receives notifications from a DailyEvent.
  // Observers must be added before the DailyEvent begins checking time,
  // and will be owned by the DailyEvent.
  class Observer {
   public:
    Observer();

    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;

    virtual ~Observer();

    // Called when the daily event is fired.
    virtual void OnDailyEvent(IntervalType type) = 0;
  };

  // Constructs DailyEvent monitor which stores the time it last fired in the
  // preference |pref_name|. |pref_name| should be registered by calling
  // RegisterPref before using this object.
  // Caller is responsible for ensuring that |pref_service| and |pref_name|
  // outlive the DailyEvent.
  // |histogram_name| is the name of the UMA metric which record when this
  // interval fires, and should be registered in histograms.xml. If
  // |histogram_name| is empty - interval fires are not recorded.
  DailyEvent(PrefService* pref_service,
             const char* pref_name,
             const std::string& histogram_name);

  DailyEvent(const DailyEvent&) = delete;
  DailyEvent& operator=(const DailyEvent&) = delete;

  ~DailyEvent();

  // Adds a observer to be notified when a day elapses. All observers should
  // be registered before the the DailyEvent starts checking time.
  void AddObserver(std::unique_ptr<Observer> observer);

  // Checks if a day has elapsed. If it has, OnDailyEvent will be called on
  // all observers.
  void CheckInterval();

  // Registers the preference used by this interval.
  static void RegisterPref(PrefRegistrySimple* registry,
                           const std::string& pref_name);

 private:
  // Handles an interval elapsing because of |type|.
  void OnInterval(base::Time now, IntervalType type);

  // A weak pointer to the PrefService object to read and write preferences
  // from. Calling code should ensure this object continues to exist for the
  // lifetime of the DailyEvent object.
  raw_ptr<PrefService, LeakedDanglingUntriaged> pref_service_;

  // The name of the preference to store the last fired time in.
  // Calling code should ensure this outlives the DailyEvent.
  const char* pref_name_;

  // The name of the histogram to record intervals.
  std::string histogram_name_;

  // A list of observers.
  std::vector<std::unique_ptr<Observer>> observers_;

  // The time that the daily event was last fired.
  base::Time last_fired_;
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_DAILY_EVENT_H_
