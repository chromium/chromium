// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_THROTTLE_THROTTLE_OBSERVER_H_
#define CHROMEOS_ASH_COMPONENTS_THROTTLE_THROTTLE_OBSERVER_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list_types.h"
#include "base/component_export.h"

namespace content {
class BrowserContext;
}

namespace ash {

// Base throttle observer class. Each throttle observer watches a particular
// condition (window activates, mojom instance disconnects, and so on) and
// calls the ObserverStateChangedCallback when there is a change.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_THROTTLE) ThrottleObserver {
 public:
  using ObserverStateChangedCallback =
      base::RepeatingCallback<void(const ThrottleObserver*)>;

  explicit ThrottleObserver(const std::string& name);

  ThrottleObserver(const ThrottleObserver&) = delete;
  ThrottleObserver& operator=(const ThrottleObserver&) = delete;

  virtual ~ThrottleObserver();

  // Starts observing. This is overridden in derived classes to register self as
  // observer for a particular condition. However, the base method should be
  // called in overridden methods, so that the callback_ member is initialized.
  virtual void StartObserving(content::BrowserContext* content,
                              const ObserverStateChangedCallback& callback);

  // Stops observing. This method is the last place in which context can be
  // used.
  virtual void StopObserving();

  // Sets the `active_` variable to `active` and runs
  // ObserverStateChangedCallback. When `active` is true, the target is
  // unthrottled. When it is false, the target is throttled as long as all other
  // observers are also inactive.
  void SetActive(bool active);

  // Sets the `enforced_` variable to `enforced` and runs the callback. When
  // the observer is enforced, that observer always controls the target's
  // throttling state. If the observer is active, the target is unthrottled.
  // When it is inactive, the target is throttled ignoring other observers'
  // states. Only one observer at most should be in the enforcing mode.
  void SetEnforced(bool enforced);

  bool active() const { return active_; }
  bool enforced() const { return enforced_; }
  const std::string& name() const { return name_; }

 protected:
  content::BrowserContext* context() { return context_; }

  bool active_ = false;
  bool enforced_ = false;
  const std::string name_;  // For logging purposes
  ObserverStateChangedCallback callback_;

 private:
  raw_ptr<content::BrowserContext> context_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_THROTTLE_THROTTLE_OBSERVER_H_
