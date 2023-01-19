// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_PERFORMANCE_CONTROLS_HATS_SERVICE_H_
#define CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_PERFORMANCE_CONTROLS_HATS_SERVICE_H_

#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/core/keyed_service.h"

class PerformanceControlsHatsService : public KeyedService {
 public:
  explicit PerformanceControlsHatsService(Profile* profile);
  ~PerformanceControlsHatsService() override = default;

  // Called when the user opens an NTP. This allows the service to check if one
  // of the performance controls surveys should be shown.
  void OpenedNewTabPage();

 private:
  raw_ptr<Profile> profile_;
};

#endif  // CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_PERFORMANCE_CONTROLS_HATS_SERVICE_H_
