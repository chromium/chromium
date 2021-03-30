// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/mac_notifications/public/cpp/notification_utils_mac.h"

#include "base/strings/strcat.h"

std::string DeriveMacNotificationId(bool incognito,
                                    const std::string& profile_id,
                                    const std::string& notification_id) {
  // i: incognito, r: regular profile
  return base::StrCat(
      {incognito ? "i" : "r", "|", profile_id, "|", notification_id});
}
