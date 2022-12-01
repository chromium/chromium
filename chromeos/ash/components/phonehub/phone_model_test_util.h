// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_PHONE_MODEL_TEST_UTIL_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_PHONE_MODEL_TEST_UTIL_H_

#include <stdint.h>

#include "base/time/time.h"
#include "chromeos/ash/components/phonehub/browser_tabs_model.h"
#include "chromeos/ash/components/phonehub/notification.h"
#include "chromeos/ash/components/phonehub/phone_status_model.h"

namespace ash {
namespace phonehub {

// Fake data for phone status.
extern const char16_t kFakeMobileProviderName[];

// Creates fake phone status data for use in tests.
const PhoneStatusModel::MobileConnectionMetadata&
CreateFakeMobileConnectionMetadata();
const PhoneStatusModel& CreateFakePhoneStatusModel();

// Fake data for browser tabs.
extern const char kFakeBrowserTabUrl1[];
extern const char16_t kFakeBrowserTabName1[];
extern const base::Time kFakeBrowserTabLastAccessedTimestamp1;
extern const char kFakeBrowserTabUrl2[];
extern const char16_t kFakeBrowserTabName2[];
extern const base::Time kFakeBrowserTabLastAccessedTimestamp2;

// Creates fake browser tab data for use in tests.
const BrowserTabsModel::BrowserTabMetadata& CreateFakeBrowserTabMetadata();
const BrowserTabsModel& CreateFakeBrowserTabsModel();

// Fake data for notifications.
extern const char16_t kFakeAppVisibleName[];
extern const char kFakeAppPackageName[];
extern const int64_t kFakeAppId;
extern const int64_t kFakeInlineReplyId;
extern const char16_t kFakeNotificationTitle[];
extern const char16_t kFakeNotificationText[];

// Creates fake notification data for use in tests.
const Notification::AppMetadata& CreateFakeAppMetadata();
const Notification& CreateFakeNotification();

}  // namespace phonehub
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_PHONE_MODEL_TEST_UTIL_H_
