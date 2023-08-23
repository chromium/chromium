// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SAFETY_HUB_SAFETY_HUB_TEST_UTIL_H_
#define CHROME_BROWSER_UI_SAFETY_HUB_SAFETY_HUB_TEST_UTIL_H_

#include "chrome/browser/ui/safety_hub/safety_hub_service.h"

namespace safety_hub_test_util {

// This will run the UpdateAsync function on the provided SafetyHubService and
// return when both the background task and UI task are completed. It will
// temporary add an observer to the service, which will be removed again before
// the function returns.
void UpdateSafetyHubServiceAsync(SafetyHubService* service);

}  // namespace safety_hub_test_util

#endif  // CHROME_BROWSER_UI_SAFETY_HUB_SAFETY_HUB_TEST_UTIL_H_
