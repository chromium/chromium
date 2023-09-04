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
#include "third_party/abseil-cpp/absl/types/optional.h"

// Base class for Safety Hub services. The background and UI tasks of the
// derived classes will be executed periodically, according to the time delta
// interval returned by GetRepeatedUpdateInterval().
class SafetyHubService : public KeyedService,
                         public base::SupportsWeakPtr<SafetyHubService> {
 public:
  // Base class for results returned after the periodic execution of the Safety
  // Hub service. Each service should implement a derived class that captures
  // the specific information that is retrieved. Any intermediate data that is
  // required for the background task, or that needs to passed through to the UI
  // thread task should be included as well.
  class Result {
   public:
    explicit Result(base::TimeTicks timestamp = base::TimeTicks::Now());

    Result(const Result&) = default;
    Result& operator=(const Result&) = delete;
    virtual ~Result() = default;

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

  // Makes an asynchronous call to the background task, which will be followed
  // by the UI task.
  void UpdateAsync();

  // Adds an observer to be notified when a new result is available.
  void AddObserver(Observer* observer);

  // Removes an observer from the observer list.
  void RemoveObserver(Observer* observer);

  // Indicates whether the update process is currently running.
  bool IsUpdateRunning();

  // Returns the latest result that is available in memory.
  absl::optional<SafetyHubService::Result*> GetCachedResult();

  // KeyedService implementation.
  void Shutdown() override;

 protected:
  // Triggers the repeated update task that updates the state of the Safety Hub
  // service.
  void StartRepeatedUpdates();

  // SafetyHubService overrides.

  // Initializes the latest result such that it is available in memory.
  virtual void InitializeLatestResult() = 0;

  // The value returned by this function determines the interval of how often
  // the Update function will be called.
  virtual base::TimeDelta GetRepeatedUpdateInterval() = 0;

  // Should return the background task that will be executed, containing the
  // computation-heavy part of the update process. This task should be static
  // and not be bound to the service, as it will be executed on a separate
  // background thread. As such, only thread-safe parameters should be bound.
  // The returned Result will be passed along to the UpdateOnUIThread function.
  virtual base::OnceCallback<std::unique_ptr<Result>()> GetBackgroundTask() = 0;

  // This function contains the part of the update task that will be executed
  // synchronously on the UI thread. Hence, it should not be computation-heavy
  // to avoid freezing the browser. It will be passed the intermediate result
  // that was produced by the background task. The result returned by this UI
  // task will be the final result that will be sent to the observers.
  virtual std::unique_ptr<Result> UpdateOnUIThread(
      std::unique_ptr<Result> result) = 0;

  virtual base::WeakPtr<SafetyHubService> GetAsWeakRef() = 0;

  // The latest available result, which is initialized at the start.
  std::unique_ptr<Result> latest_result_ = nullptr;

 private:
  FRIEND_TEST_ALL_PREFIXES(SafetyHubServiceTest, ManageObservers);
  FRIEND_TEST_ALL_PREFIXES(SafetyHubServiceTest, UpdateOnBackgroundThread);

  // Called as soon as the update has been finished.
  void OnUpdateFinished(std::unique_ptr<Result> result);

  // Notifies each of the added observers that a new result is available.
  void NotifyObservers(Result* result);

  // Posts the background task on a background thread.
  void UpdateAsyncInternal();

  // Repeating timer that runs the recurring tasks.
  base::RepeatingTimer update_timer_;

  // List of observers that have to be notified when a new result is available.
  base::ObserverList<Observer> observers_;

  // Indicator of how many requested updates are still pending.
  int pending_updates_ = 0;
};

#endif  // CHROME_BROWSER_UI_SAFETY_HUB_SAFETY_HUB_SERVICE_H_
