// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/push_notification_manager.h"

#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "components/optimization_guide/core/optimization_guide_features.h"

namespace optimization_guide {

PushNotificationManager::PushNotificationManager() = default;

PushNotificationManager::~PushNotificationManager() = default;

void PushNotificationManager::SetDelegate(
    PushNotificationManager::Delegate* delegate) {
  delegate_ = delegate;
}

void PushNotificationManager::OnDelegateReady() {
  DCHECK(delegate_);
  DCHECK(features::IsPushNotificationsEnabled());
}

void PushNotificationManager::OnNewPushNotification(
    const proto::HintNotificationPayload& notification) {
  if (!notification.has_hint_key())
    return;

  if (!notification.has_key_representation())
    return;

  DispatchPayload(notification);
}

void PushNotificationManager::AddObserver(
    PushNotificationManager::Observer* observer) {
  observers_.AddObserver(observer);
}

void PushNotificationManager::RemoveObserver(
    PushNotificationManager::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void PushNotificationManager::DispatchPayload(
    const proto::HintNotificationPayload& notification) {
  // No custom payload or optimization type.
  if (!notification.has_payload() || !notification.has_optimization_type()) {
    return;
  }

  base::UmaHistogramEnumeration(
      "OptimizationGuide.PushNotifications.ReceivedNotificationType",
      notification.optimization_type(),
      static_cast<optimization_guide::proto::OptimizationType>(
          optimization_guide::proto::OptimizationType_ARRAYSIZE));

  for (Observer& observer : observers_) {
    observer.OnNotificationPayload(notification.optimization_type(),
                                   notification.payload());
  }
  delegate_->RemoveFetchedEntriesByHintKeys(base::DoNothing(),
                                            notification.key_representation(),
                                            {notification.hint_key()});
}

}  // namespace optimization_guide
