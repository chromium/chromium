// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/calendar/calendar_client_impl.h"

#include <utility>

#include "ash/constants/ash_pref_names.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/calendar/calendar_keyed_service.h"
#include "chrome/browser/ui/ash/calendar/calendar_keyed_service_factory.h"
#include "components/prefs/pref_service.h"
#include "google_apis/common/api_error_codes.h"

namespace ash {

CalendarClientImpl::CalendarClientImpl(Profile* profile) : profile_(profile) {}

CalendarClientImpl::~CalendarClientImpl() = default;

base::OnceClosure CalendarClientImpl::GetEventList(
    google_apis::calendar::CalendarEventListCallback callback,
    const base::Time& start_time,
    const base::Time& end_time) {
  PrefService* pref = profile_->GetPrefs();

  if (!pref->GetBoolean(ash::prefs::kCalendarIntegrationEnabled)) {
    std::move(callback).Run(google_apis::OTHER_ERROR, nullptr);
    return base::DoNothing();
  }

  CalendarKeyedService* service =
      CalendarKeyedServiceFactory::GetInstance()->GetService(profile_);

  // For non-gaia user this `service` is not set.
  if (service)
    return service->GetEventList(std::move(callback), start_time, end_time);

  std::move(callback).Run(google_apis::OTHER_ERROR, nullptr);

  return base::DoNothing();
}

}  // namespace ash
