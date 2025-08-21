// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safety_check/features.h"

#include "base/feature_list.h"

namespace safety_check::features {

BASE_FEATURE(SafetyHub, base::FEATURE_ENABLED_BY_DEFAULT);

// Time between automated runs of the password check.
const base::FeatureParam<base::TimeDelta> kBackgroundPasswordCheckInterval{
    &kSafetyHub, "background-password-check-interval", base::Days(30)};

// When the password check didn't run at its scheduled time (e.g. client was
// offline) it will be scheduled to run within this time frame. Changing the
// value  will flaten the picks on rush hours, e.g: 1h will cause higher
// picks than 4h.
const base::FeatureParam<base::TimeDelta> kPasswordCheckOverdueInterval{
    &kSafetyHub, "password-check-overdue-interval", base::Hours(4)};

// Password check runs randomly based on the weight of each day. Parameters
// below will be used to adjust weights, if necessary. Weight to randomly
// schedule for Mondays.
const base::FeatureParam<int> kPasswordCheckMonWeight{
    &kSafetyHub, "password-check-mon-weight", 6};

// Weight to randomly schedule for Tuesdays.
const base::FeatureParam<int> kPasswordCheckTueWeight{
    &kSafetyHub, "password-check-tue-weight", 9};

// Weight to randomly schedule for Wednesdays.
const base::FeatureParam<int> kPasswordCheckWedWeight{
    &kSafetyHub, "password-check-wed-weight", 9};

// Weight to randomly schedule for Thursdays.
const base::FeatureParam<int> kPasswordCheckThuWeight{
    &kSafetyHub, "password-check-thu-weight", 9};

// Weight to randomly schedule for Fridays.
const base::FeatureParam<int> kPasswordCheckFriWeight{
    &kSafetyHub, "password-check-fri-weight", 9};

// Weight to randomly schedule for Saturdays.
const base::FeatureParam<int> kPasswordCheckSatWeight{
    &kSafetyHub, "password-check-sat-weight", 6};

// Weight to randomly schedule for Sundays.
const base::FeatureParam<int> kPasswordCheckSunWeight{
    &kSafetyHub, "password-check-sun-weight", 6};

// Engagement limits Notification permissions module.
const base::FeatureParam<int>
    kSafetyCheckNotificationPermissionsMinEnagementLimit{
        &kSafetyHub, "min-engagement-notification-count", 0};
const base::FeatureParam<int>
    kSafetyCheckNotificationPermissionsLowEnagementLimit{
        &kSafetyHub, "low-engagement-notification-count", 4};

const char kPasswordCheckNotificationIntervalName[] =
    "password-check-notification-interval";
const char kRevokedPermissionsNotificationIntervalName[] =
    "revoked-permissions-notification-interval";
const char kNotificationPermissionsNotificationIntervalName[] =
    "notification-permissions-notification-interval";
const char kSafeBrowsingNotificationIntervalName[] =
    "safe-browsing-notification-interval";

// Interval to show notification for compromised password in Safety Hub
// notifications.
const base::FeatureParam<base::TimeDelta> kPasswordCheckNotificationInterval{
    &kSafetyHub, kPasswordCheckNotificationIntervalName, base::Days(0)};

// Interval to show notification for revoked permissions in Safety Hub
// notifications.
const base::FeatureParam<base::TimeDelta>
    kRevokedPermissionsNotificationInterval{
        &kSafetyHub, kRevokedPermissionsNotificationIntervalName,
        base::Days(10)};

// Interval to show notification for notification permissions in Safety Hub
// notifications.
const base::FeatureParam<base::TimeDelta>
    kNotificationPermissionsNotificationInterval{
        &kSafetyHub, kNotificationPermissionsNotificationIntervalName,
        base::Days(10)};

// Interval to show notification for safe browsing in Safety Hub notifications.
const base::FeatureParam<base::TimeDelta> kSafeBrowsingNotificationInterval{
    &kSafetyHub, kSafeBrowsingNotificationIntervalName, base::Days(90)};

}  // namespace safety_check::features
