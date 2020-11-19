// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_READING_LIST_FEATURES_READING_LIST_SWITCHES_H_
#define COMPONENTS_READING_LIST_FEATURES_READING_LIST_SWITCHES_H_

#include "base/feature_list.h"

namespace reading_list {
namespace switches {

// Feature flag used for enabling Read later on desktop and Android.
extern const base::Feature kReadLater;
// Whether Reading List is enabled on this device.
bool IsReadingListEnabled();

#ifdef OS_ANDROID
// Feature flag used for enabling read later reminder notification.
extern const base::Feature kReadLaterReminderNotification;
#endif

}  // namespace switches
}  // namespace reading_list

#endif  // COMPONENTS_READING_LIST_FEATURES_READING_LIST_SWITCHES_H_
