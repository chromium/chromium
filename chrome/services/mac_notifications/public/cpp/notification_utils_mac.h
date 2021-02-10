// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_MAC_NOTIFICATIONS_PUBLIC_CPP_NOTIFICATION_UTILS_MAC_H_
#define CHROME_SERVICES_MAC_NOTIFICATIONS_PUBLIC_CPP_NOTIFICATION_UTILS_MAC_H_

#include <string>

// Derives a unique notification identifier to be used by the macOS system
// notification center to uniquely identify a notification.
std::string DeriveMacNotificationId(bool incognito,
                                    const std::string& profile_id,
                                    const std::string& notification_id);

#endif  // CHROME_SERVICES_MAC_NOTIFICATIONS_PUBLIC_CPP_NOTIFICATION_UTILS_MAC_H_
