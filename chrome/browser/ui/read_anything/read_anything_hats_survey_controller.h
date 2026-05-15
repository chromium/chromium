// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_READ_ANYTHING_READ_ANYTHING_HATS_SURVEY_CONTROLLER_H_
#define CHROME_BROWSER_UI_READ_ANYTHING_READ_ANYTHING_HATS_SURVEY_CONTROLLER_H_

#include <optional>

#include "base/time/time.h"
#include "chrome/browser/ui/read_anything/read_anything_lifecycle_observer.h"
#include "components/tabs/public/tab_interface.h"

class ReadAnythingController;

class ReadAnythingHatsSurveyController : public ReadAnythingLifecycleObserver {
 public:
  ReadAnythingHatsSurveyController(
      ReadAnythingController* read_anything_controller,
      tabs::TabInterface* tab);
  ReadAnythingHatsSurveyController(const ReadAnythingHatsSurveyController&) =
      delete;
  ReadAnythingHatsSurveyController& operator=(
      const ReadAnythingHatsSurveyController&) = delete;
  ~ReadAnythingHatsSurveyController() override;

  // ReadAnythingLifecycleObserver:
  void Activate(
      bool active,
      std::optional<ReadAnythingOpenTrigger> trigger,
      std::optional<base::TimeDelta> completed_session_duration) override;
  void OnDestroyed() override;

 private:
  // Checks if the conditions to show the Reading Mode HaTS survey are met
  // and triggers the survey if appropriate.
  void MaybeShowReadingModeHatsSurvey();

  // Records a timestamp of the current Reading Mode usage in preferences,
  // maintaining a list of recent usages.
  void RecordUsageForHatsSurvey();

  // Tracks if this session has already been recorded for the HaTS survey.
  // Prevents recording usage multiple times in a single session for HaTS.
  bool has_recorded_usage_ = false;

  // The Reading Mode controller observed for lifecycle events.
  raw_ptr<ReadAnythingController> read_anything_controller_;
  // The tab for which a survey may be launched.
  raw_ptr<tabs::TabInterface> tab_;
};

#endif  // CHROME_BROWSER_UI_READ_ANYTHING_READ_ANYTHING_HATS_SURVEY_CONTROLLER_H_
