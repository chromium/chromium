// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SAFETY_HUB_SAFETY_HUB_SERVICE_H_
#define CHROME_BROWSER_UI_SAFETY_HUB_SAFETY_HUB_SERVICE_H_

#include <memory>
#include <optional>

#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/safety_hub/safety_hub_result.h"
#include "components/keyed_service/core/keyed_service.h"

// Base class for Safety Hub services. The background and UI tasks of the
// derived classes will be executed periodically, according to the time delta
// interval returned by GetRepeatedUpdateInterval().
class SafetyHubService : public KeyedService {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when the result from the update of the service is available.
    virtual void OnResultAvailable(const SafetyHubResult* result) = 0;
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
  std::optional<std::unique_ptr<SafetyHubResult>> GetCachedResult();

  // KeyedService implementation.
  void Shutdown() override;

  // Checks if the repeating timer is running.
  bool IsTimerRunningForTesting();

 protected:
  // Triggers the repeated update task that updates the state of the Safety Hub
  // service.
  void StartRepeatedUpdates();

  // Stops the repeating timer to stop recurring tasks.
  void StopTimer();

  // SafetyHubService overrides.

  // Initializes the latest result such that it is available in memory. It needs
  // to be called on creation of the service.
  void InitializeLatestResult();

  // The value returned by this function determines the interval of how often
  // the Update function will be called.
  virtual base::TimeDelta GetRepeatedUpdateInterval() = 0;

  // TODO(crbug.com/40267370): Not each service needs to execute a task in the
  // background. The SafetyHubService class should be redesigned such that
  // there's no needless boilerplate code needed in this case.

  // Should return the background task that will be executed, containing the
  // computation-heavy part of the update process. This task should be static
  // and not be bound to the service, as it will be executed on a separate
  // background thread. As such, only thread-safe parameters should be bound.
  // The returned SafetyHubResult will be passed along to the UpdateOnUIThread
  // function.
  virtual base::OnceCallback<std::unique_ptr<SafetyHubResult>()>
  GetBackgroundTask() = 0;

  // This function contains the part of the update task that will be executed
  // synchronously on the UI thread. Hence, it should not be computation-heavy
  // to avoid freezing the browser. It will be passed the intermediate result
  // that was produced by the background task. The result returned by this UI
  // task will be the final result that will be sent to the observers.
  virtual std::unique_ptr<SafetyHubResult> UpdateOnUIThread(
      std::unique_ptr<SafetyHubResult> result) = 0;

  virtual base::WeakPtr<SafetyHubService> GetAsWeakRef() = 0;

  // Returns the result that should be initialized upon starting the service.
  // Typically, this will be the outcome of running the lightweight step of the
  // update process (i.e. `UpdateOnUIThread()`).
  virtual std::unique_ptr<SafetyHubResult> InitializeLatestResultImpl() = 0;

  // Updates the latest result to the provided value.
  void SetLatestResult(std::unique_ptr<SafetyHubResult> result);

 private:
  FRIEND_TEST_ALL_PREFIXES(SafetyHubServiceTest, ManageObservers);
  FRIEND_TEST_ALL_PREFIXES(SafetyHubServiceTest, UpdateOnBackgroundThread);

  // Called as soon as the update has been finished.
  void OnUpdateFinished(std::unique_ptr<SafetyHubResult> result);

  // Notifies each of the added observers that a new result is available.
  void NotifyObservers(SafetyHubResult* result);

  // Posts the background task on a background thread.
  void UpdateAsyncInternal();

  // Repeating timer that runs the recurring tasks.
  base::RepeatingTimer update_timer_;

  // List of observers that have to be notified when a new result is available.
  base::ObserverList<Observer> observers_;

  // Indicator of how many requested updates are still pending.
  int pending_updates_ = 0;

  // The latest available result, which is initialized when the service is
  // started. The value is set by `InitializeLatestResult()`, which is called
  // in the constructor of each service.
  std::unique_ptr<SafetyHubResult> latest_result_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_SAFETY_HUB_SAFETY_HUB_SERVICE_H_
