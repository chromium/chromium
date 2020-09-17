// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PHONEHUB_PHONE_MODEL_TEST_UTIL_H_
#define CHROMEOS_COMPONENTS_PHONEHUB_PHONE_MODEL_TEST_UTIL_H_

#include <stdint.h>

#include "chromeos/components/phonehub/browser_tabs_model.h"
#include "chromeos/components/phonehub/notification.h"
#include "chromeos/components/phonehub/phone_status_model.h"

namespace chromeos {
namespace phonehub {

// Fake data for phone status.
extern const char kFakeMobileProviderName[];

// Creates fake phone status data for use in tests.
const PhoneStatusModel::MobileConnectionMetadata&
CreateFakeMobileConnectionMetadata();
const PhoneStatusModel& CreateFakePhoneStatusModel();

// Fake data for browser tabs.
extern const char kFakeBrowserTabUrl1[];
extern const char kFakeBrowserTabName1[];
extern const base::Time kFakeBrowserTabLastAccessedTimestamp1;
extern const char kFakeBrowserTabUrl2[];
extern const char kFakeBrowserTabName2[];
extern const base::Time kFakeBrowserTabLastAccessedTimestamp2;

// Creates fake browser tab data for use in tests.
const BrowserTabsModel::BrowserTabMetadata& CreateFakeBrowserTabMetadata();
const BrowserTabsModel& CreateFakeBrowserTabsModel();

// Fake data for notifications.
extern const char kFakeAppVisibleName[];
extern const char kFakeAppPackageName[];
extern const int64_t kFakeAppId;
extern const int64_t kFakeInlineReplyId;
extern const char kFakeNotificationTitle[];
extern const char kFakeNotificationText[];

// Creates fake notification data for use in tests.
const Notification::AppMetadata& CreateFakeAppMetadata();
const Notification& CreateFakeNotification();

}  // namespace phonehub
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_PHONEHUB_PHONE_MODEL_TEST_UTIL_H_
