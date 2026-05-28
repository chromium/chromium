// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_THEMES_NTP_CUSTOM_BACKGROUND_SERVICE_OBSERVER_H_
#define COMPONENTS_THEMES_NTP_CUSTOM_BACKGROUND_SERVICE_OBSERVER_H_

#include "base/observer_list_types.h"

// Observer for NtpCustomBackgroundServiceBase and its derived classes.
class NtpCustomBackgroundServiceObserver : public base::CheckedObserver {
 public:
  // Invoked when the custom background image is updated.
  virtual void OnCustomBackgroundImageUpdated() = 0;

  // Invoked when the service is shutting down.
  virtual void OnNtpCustomBackgroundServiceShuttingDown() {}
};

#endif  // COMPONENTS_THEMES_NTP_CUSTOM_BACKGROUND_SERVICE_OBSERVER_H_
