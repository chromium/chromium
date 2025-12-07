// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_PERFORMANCE_CONTROLS_HATS_SERVICE_H_
#define CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_PERFORMANCE_CONTROLS_HATS_SERVICE_H_

#include <optional>
#include <string>

#include "base/byte_count.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/core/keyed_service.h"

class PerformanceControlsHatsService : public KeyedService {
 public:
  // Names of Product Specific Data bit entries.
  static constexpr char kBatterySaverPSDName[] = "Battery Saver Mode Enabled";
  static constexpr char kMemorySaverPSDName[] = "Memory Saver Mode Enabled";
  static constexpr char kUniformSamplePSDName[] = "Selected for Uniform Sample";

  // Names of Product Specific Data string entries.
  static constexpr char kChannelPSDName[] = "Channel";
  static constexpr char kPerformanceSegmentPSDName[] =
      "Performance Characteristics (OS and Total Memory)";

  explicit PerformanceControlsHatsService(Profile* profile);

  // Called when the user opens an NTP. This allows the service to check if one
  // of the performance controls surveys should be shown.
  void OpenedNewTabPage();

  // Returns the delay that must pass after the session starts before showing
  // the PPM survey.
  base::TimeDelta delay_before_ppm_survey() const {
    return delay_before_ppm_survey_;
  }

  // Lets tests override the random delay before showing the PPM survey.
  void SetDelayBeforePPMSurveyForTesting(base::TimeDelta delay) {
    delay_before_ppm_survey_ = delay;
  }

  // Overrides the amount of physical memory reported for testing.
  void SetAmountOfPhysicalMemoryForTesting(base::ByteCount memory) {
    memory_amount_for_testing_ = memory;
  }

 private:
  // Returns true if the PPM survey can be shown at this time.
  bool MayLaunchPPMSurvey() const;

  // Returns the name of the segment this client falls into for the PPM survey,
  // or an empty string if it doesn't fall into any defined survey segment.
  std::string GetPPMSurveySegmentName();

  raw_ptr<Profile> profile_;

  // Delay before showing the PPM UMA survey. Randomly generated when the
  // service is created.
  base::TimeDelta delay_before_ppm_survey_;

  // A value to use instead of calling base::SysInfo::AmountOfPhysicalMemoryMB()
  // in tests.
  std::optional<base::ByteCount> memory_amount_for_testing_;
};

#endif  // CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_PERFORMANCE_CONTROLS_HATS_SERVICE_H_
