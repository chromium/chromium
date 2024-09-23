// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_THROTTLE_THROTTLE_SERVICE_H_
#define CHROMEOS_ASH_COMPONENTS_THROTTLE_THROTTLE_SERVICE_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "base/component_export.h"
#include "chromeos/ash/components/throttle/throttle_observer.h"

namespace content {
class BrowserContext;
}

namespace ash {

// This class is the base for different throttle services on Chrome OS.
// The class holds a number of ThrottleObservers which watch for several
// conditions. When the observers change from active to inactive or vice-versa,
// OnObserverStateChanged checks if there is an active observer and calls
// ThrottleInstance accordingly.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_THROTTLE) ThrottleService {
 public:
  class ServiceObserver : public base::CheckedObserver {
   public:
    // Notifies that throttling has been changed. If the target is now
    // throttled, the function is called with true. It is called with
    // false otherwise.
    virtual void OnThrottle(bool throttled) = 0;
  };

  explicit ThrottleService(content::BrowserContext* context);

  ThrottleService(const ThrottleService&) = delete;
  ThrottleService& operator=(const ThrottleService&) = delete;

  virtual ~ThrottleService();

  void AddServiceObserver(ServiceObserver* observer);
  void RemoveServiceObserver(ServiceObserver* observer);

  // Returns an observer whose name is |name|. Returns nullptr otherwise.
  ThrottleObserver* GetObserverByName(const std::string& name);

  // Functions for testing
  void NotifyObserverStateChangedForTesting();
  void SetObserversForTesting(
      std::vector<std::unique_ptr<ThrottleObserver>> observers);
  bool HasServiceObserverForTesting(ServiceObserver* candidate);

  bool should_throttle() const {
    // When `should_throttle_` hasn't been initialized, return true to throttle
    // the target (which is a safer bet for Chrome.)
    return !should_throttle_ || *should_throttle_;
  }

  void reset_should_throttle_for_testing() { should_throttle_.reset(); }
  const std::vector<std::unique_ptr<ThrottleObserver>>& observers_for_testing()
      const {
    return observers_;
  }

 protected:
  void AddObserver(std::unique_ptr<ThrottleObserver> observer);
  void StartObservers();
  void StopObservers();
  void OnObserverStateChanged(const ThrottleObserver* changed_observer);

  // This function is called whenever the target should be throttled or
  // unthrottled. Derived classes should implement ThrottleInstance to adjust
  // the throttling state of their relevant target.
  virtual void ThrottleInstance(bool should_throttle) = 0;

  // Whenever there is a change in the effective observer, this function is
  // called with the name of the previously effective observer and the duration
  // it was effective. Derived classes can implement this function to record UMA
  // metrics.
  virtual void RecordCpuRestrictionDisabledUMA(const std::string& observer_name,
                                               base::TimeDelta delta) {}

  const std::vector<std::unique_ptr<ThrottleObserver>>& observers() const {
    return observers_;
  }
  content::BrowserContext* context() const { return context_; }

 private:
  const raw_ptr<content::BrowserContext> context_;
  std::vector<std::unique_ptr<ThrottleObserver>> observers_;

  // True when the target should be throttled. Use optional<> to make sure this
  // service always calls ThrottleInstance() when one of the observers has
  // changed for the first time.
  std::optional<bool> should_throttle_;

  raw_ptr<ThrottleObserver> last_effective_observer_{nullptr};
  base::TimeTicks last_throttle_transition_;
  base::ObserverList<ServiceObserver> service_observers_;
  base::WeakPtrFactory<ThrottleService> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_THROTTLE_THROTTLE_SERVICE_H_
