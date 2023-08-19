// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SAFETY_HUB_SAFETY_HUB_SERVICE_H_
#define CHROME_BROWSER_UI_SAFETY_HUB_SAFETY_HUB_SERVICE_H_

#include <memory>

#include "base/gtest_prod_util.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/keyed_service/core/keyed_service.h"

// Base class for Safety Hub services. The Update() function of the derived
// classes will be executed periodically, according to the time delta interval
// returned by GetRepeatedUpdateInterval().
class SafetyHubService : public KeyedService,
                         public base::SupportsWeakPtr<SafetyHubService> {
 public:
  // Base class for results returned after the periodic execution of the Safety
  // Hub service. Each service should implement a derived class that captures
  // the specific information that is retrieved.
  class Result {
   public:
    explicit Result(base::TimeTicks timestamp = base::TimeTicks::Now());

    Result(const Result&) = delete;
    Result& operator=(const Result&) = delete;
    ~Result();

    base::TimeTicks timestamp() const;

   private:
    base::TimeTicks timestamp_;
  };

  class Observer : public base::CheckedObserver {
   public:
    // Called when the result from the update of the service is available.
    virtual void OnResultAvailable(const Result* result) = 0;
  };

  SafetyHubService();

  SafetyHubService(const SafetyHubService&) = delete;
  SafetyHubService& operator=(const SafetyHubService&) = delete;

  ~SafetyHubService() override;

  // KeyedService implementation.
  void Shutdown() override;

  // Triggers the repeated update task that updates the state of the Safety Hub
  // service.
  void StartRepeatedUpdates();

  // Makes an asynchronous call to the Update function, and will call the
  // callback function upon completion.
  void UpdateAsync();

  // Adds an observer to be notified when a new result is available.
  void AddObserver(Observer* observer);

  // Removes an observer from the observer list.
  void RemoveObserver(Observer* observer);

  void SetClockForTesting(std::unique_ptr<base::Clock> clock) {
    clock_for_testing_ = std::move(clock);
  }

 protected:
  // The value returned by this function determines the interval of how often
  // the Update function will be called.
  virtual base::TimeDelta GetRepeatedUpdateInterval() = 0;

  // Contains the actual implementation to make updates to the Safety Hub
  // service.
  virtual std::unique_ptr<Result> UpdateOnBackgroundThread() = 0;

  base::Clock* GetClock() {
    return clock_for_testing_ ? clock_for_testing_.get()
                              : base::DefaultClock::GetInstance();
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(SafetyHubServiceTest, ManageObservers);

  // Called as soon as the update has been finished.
  void OnUpdateFinished(std::unique_ptr<Result> result);

  // Notifies each of the added observers that a new result is available.
  void NotifyObservers(Result* result);

  // Repeating timer that runs the recurring tasks.
  base::RepeatingTimer update_timer_;

  // List of observers that have to be notified when a new result is available.
  base::ObserverList<Observer> observers_;

  // Clock used in testing.
  std::unique_ptr<base::Clock> clock_for_testing_;
};

#endif  // CHROME_BROWSER_UI_SAFETY_HUB_SAFETY_HUB_SERVICE_H_
