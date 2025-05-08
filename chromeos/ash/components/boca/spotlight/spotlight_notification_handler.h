// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_SPOTLIGHT_SPOTLIGHT_NOTIFICATION_HANDLER_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_SPOTLIGHT_SPOTLIGHT_NOTIFICATION_HANDLER_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/message_center/public/cpp/notification.h"

namespace ash::boca {
// Handles Class hub notifications.
// TODO: http://crbug.com/397722474 - Investigate merging notification handlers.
class SpotlightNotificationHandler {
 public:
  using CountdownCompletionCallback = base::OnceClosure;
  // Delegate implementation that can be overridden by tests to stub
  // notification display actions. Especially relevant for tests that cannot
  // leverage Ash UI.
  class Delegate {
   public:
    Delegate() = default;
    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;
    virtual ~Delegate() = default;

    // Display notification using the specified data.
    virtual void ShowNotification(
        std::unique_ptr<message_center::Notification> notification);

    // Clears the notification with the specified id, if it exists
    virtual void ClearNotification(const std::string& notification_id);
  };

  explicit SpotlightNotificationHandler(
      std::unique_ptr<Delegate> delegate = std::make_unique<Delegate>());
  SpotlightNotificationHandler(const SpotlightNotificationHandler&) = delete;
  SpotlightNotificationHandler& operator=(const SpotlightNotificationHandler&) =
      delete;
  ~SpotlightNotificationHandler();

  // Handle notification presented to students when a Spotlight session
  // begins.
  void StartSpotlightCountdownNotification(
      CountdownCompletionCallback completion_callback);

  // Stops the Spotlight countdown when an in-progress request was cancelled.
  void StopSpotlightCountdown();

 private:
  void StartSpotlightCountdownNotificationInternal();

  const std::unique_ptr<Delegate> delegate_;
  base::TimeDelta notification_duration_;
  CountdownCompletionCallback completion_callback_;
  base::RepeatingTimer timer_;
};
}  // namespace ash::boca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_SPOTLIGHT_SPOTLIGHT_NOTIFICATION_HANDLER_H_
