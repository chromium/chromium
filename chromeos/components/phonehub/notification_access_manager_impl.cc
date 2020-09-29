// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/notification_access_manager_impl.h"

#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/components/phonehub/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace chromeos {
namespace phonehub {

// static
void NotificationAccessManagerImpl::RegisterPrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kNotificationAccessGranted, false);
}

NotificationAccessManagerImpl::NotificationAccessManagerImpl(
    PrefService* pref_service)
    : pref_service_(pref_service) {}

NotificationAccessManagerImpl::~NotificationAccessManagerImpl() = default;

bool NotificationAccessManagerImpl::HasAccessBeenGranted() const {
  return pref_service_->GetBoolean(prefs::kNotificationAccessGranted);
}

void NotificationAccessManagerImpl::SetHasAccessBeenGrantedInternal(
    bool has_access_been_granted) {
  PA_LOG(INFO) << "Notification access state has been set to: "
               << has_access_been_granted;
  // TODO(jimmyxgong): Implement this stub function.
}

void NotificationAccessManagerImpl::OnSetupAttemptStarted() {
  PA_LOG(INFO) << "Notification access setup flow started.";
  // TODO(khorimoto): Attempt notification setup flow.
}

void NotificationAccessManagerImpl::OnSetupAttemptEnded() {
  PA_LOG(INFO) << "Notification access setup flow ended.";
  // TODO(khorimoto): Stop ongoing notification setup flow.
}

}  // namespace phonehub
}  // namespace chromeos
