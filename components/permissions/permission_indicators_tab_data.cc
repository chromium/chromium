// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/permission_indicators_tab_data.h"

#include "base/metrics/histogram_functions_internal_overloads.h"
#include "components/permissions/permission_uma_util.h"
#include "content/public/browser/web_contents.h"

namespace permissions {

PermissionIndicatorsTabData::PermissionIndicatorsTabData(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  DCHECK(web_contents);
  origin_ = web_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin();
}

PermissionIndicatorsTabData::~PermissionIndicatorsTabData() = default;

bool PermissionIndicatorsTabData::IsVerboseIndicatorAllowed(
    IndicatorsType type) const {
  return !displayed_indicators_.contains(type);
}

void PermissionIndicatorsTabData::SetVerboseIndicatorDisplayed(
    IndicatorsType type) {
  displayed_indicators_.insert(type);
}

void PermissionIndicatorsTabData::RecordStartGeolocationService() {
  if (!geolocation_last_usage_time_.has_value()) {
    return;
  }

  base::TimeDelta time_delta =
      base::TimeTicks::Now() - geolocation_last_usage_time_.value();

  if (time_delta <= base::Seconds(4)) {
    base::UmaHistogramEnumeration(
        "Permissions.Usage.ElapsedTimeSinceLastUsage.Geolocation",
        Duration::kFourSeconds);
  } else if (time_delta <= base::Seconds(10)) {
    base::UmaHistogramEnumeration(
        "Permissions.Usage.ElapsedTimeSinceLastUsage.Geolocation",
        Duration::kTenSeconds);
  } else if (time_delta <= base::Minutes(1)) {
    base::UmaHistogramEnumeration(
        "Permissions.Usage.ElapsedTimeSinceLastUsage.Geolocation",
        Duration::kOneMinute);
  } else if (time_delta <= base::Minutes(5)) {
    base::UmaHistogramEnumeration(
        "Permissions.Usage.ElapsedTimeSinceLastUsage.Geolocation",
        Duration::kFiveMinutes);
  } else if (time_delta <= base::Minutes(10)) {
    base::UmaHistogramEnumeration(
        "Permissions.Usage.ElapsedTimeSinceLastUsage.Geolocation",
        Duration::kTenMinutes);
  } else if (time_delta <= base::Hours(1)) {
    base::UmaHistogramEnumeration(
        "Permissions.Usage.ElapsedTimeSinceLastUsage.Geolocation",
        Duration::kOneHour);
  } else {
    base::UmaHistogramEnumeration(
        "Permissions.Usage.ElapsedTimeSinceLastUsage.Geolocation",
        Duration::kMoreThanOneHour);
  }

  geolocation_last_usage_time_ = base::TimeTicks::Now();
}

void PermissionIndicatorsTabData::OnCapabilityTypesChanged(
    content::WebContents::CapabilityType connection_type,
    bool used) {
  if (connection_type == content::WebContents::CapabilityType::kGeolocation) {
    if (used) {
      RecordStartGeolocationService();
    } else {
      geolocation_last_usage_time_ = base::TimeTicks::Now();
    }
  }
}

void PermissionIndicatorsTabData::ClearData() {
  geolocation_last_usage_time_.reset();
  displayed_indicators_.clear();
}

void PermissionIndicatorsTabData::WebContentsDestroyed() {
  ClearData();
}

void PermissionIndicatorsTabData::PrimaryPageChanged(content::Page& page) {
  if (origin_ != page.GetMainDocument().GetLastCommittedOrigin()) {
    origin_ = page.GetMainDocument().GetLastCommittedOrigin();
    ClearData();
  }
}
}  // namespace permissions
