// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/calendar/calendar_client_impl.h"

#include <string>
#include <utility>

#include "ash/constants/ash_pref_names.h"
#include "base/functional/callback_helpers.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/calendar/calendar_keyed_service.h"
#include "chrome/browser/ui/ash/calendar/calendar_keyed_service_factory.h"
#include "components/prefs/pref_service.h"
#include "google_apis/common/api_error_codes.h"

namespace ash {
namespace {

bool IsCalendarIntegrationEnabled(Profile* profile) {
  PrefService* pref = profile->GetPrefs();
  return (pref && !pref->GetBoolean(ash::prefs::kCalendarIntegrationEnabled));
}
}  // namespace

CalendarClientImpl::CalendarClientImpl(Profile* profile) : profile_(profile) {}

CalendarClientImpl::~CalendarClientImpl() = default;

base::OnceClosure CalendarClientImpl::GetCalendarList(
    google_apis::calendar::CalendarListCallback callback) {
  if (IsCalendarIntegrationEnabled(profile_)) {
    std::move(callback).Run(google_apis::OTHER_ERROR, /*calendars=*/nullptr);
    return base::DoNothing();
  }

  CalendarKeyedService* service =
      CalendarKeyedServiceFactory::GetInstance()->GetService(profile_);

  // For non-gaia users this `service` is not set.
  if (service) {
    return service->GetCalendarList(std::move(callback));
  }

  std::move(callback).Run(google_apis::OTHER_ERROR, /*calendars=*/nullptr);

  return base::DoNothing();
}

base::OnceClosure CalendarClientImpl::GetEventList(
    google_apis::calendar::CalendarEventListCallback callback,
    const base::Time start_time,
    const base::Time end_time) {
  if (IsCalendarIntegrationEnabled(profile_)) {
    std::move(callback).Run(google_apis::OTHER_ERROR, /*events=*/nullptr);
    return base::DoNothing();
  }

  CalendarKeyedService* service =
      CalendarKeyedServiceFactory::GetInstance()->GetService(profile_);

  // For non-gaia users this `service` is not set.
  if (service)
    return service->GetEventList(std::move(callback), start_time, end_time);

  std::move(callback).Run(google_apis::OTHER_ERROR, /*events=*/nullptr);

  return base::DoNothing();
}

base::OnceClosure CalendarClientImpl::GetEventList(
    google_apis::calendar::CalendarEventListCallback callback,
    const base::Time start_time,
    const base::Time end_time,
    const std::string& calendar_id,
    const std::string& calendar_color_id) {
  if (IsCalendarIntegrationEnabled(profile_)) {
    std::move(callback).Run(google_apis::OTHER_ERROR, /*events=*/nullptr);
    return base::DoNothing();
  }

  CalendarKeyedService* service =
      CalendarKeyedServiceFactory::GetInstance()->GetService(profile_);

  // For non-gaia users this `service` is not set.
  if (service) {
    return service->GetEventList(std::move(callback), start_time, end_time,
                                 calendar_id, calendar_color_id);
  }

  std::move(callback).Run(google_apis::OTHER_ERROR, /*events=*/nullptr);

  return base::DoNothing();
}

}  // namespace ash
