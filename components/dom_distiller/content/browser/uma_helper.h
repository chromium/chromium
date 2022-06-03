// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CONTENT_BROWSER_UMA_HELPER_H_
#define COMPONENTS_DOM_DISTILLER_CONTENT_BROWSER_UMA_HELPER_H_

#include "base/time/time.h"

namespace content {
class WebContents;
}  // namespace content

namespace dom_distiller {

// A utility class for logging UMA metrics.
class UMAHelper {
 public:
  // Must agree with ReaderModeEntryPoint in enums.xml.
  enum class ReaderModeEntryPoint {
    kOmniboxIcon = 0,
    kMenuOption = 1,
    kMaxValue = kMenuOption,
  };

  // A page can be distillable (an article), distilled (a reader mode
  // page), or neither (some other webpage).
  enum class ReaderModePageType {
    kDistillable,
    kDistilled,
    kNone,
  };

  // A class to time the active time of a page, so that metrics for time spent
  // on distillable pages and distilled pages can be logged. There should be
  // exactly one of these associated with each DistillabilityDriver.
  class DistillabilityDriverTimer {
   public:
    DistillabilityDriverTimer() = default;

    ~DistillabilityDriverTimer() {
      // The timer has the same life as a DistillabilityDriver, which is
      // destroyed when a WebContents is destroyed. Thus if we are being
      // destroyed and on a distilled page, go ahead and log the total time to
      // UMA. This may happen if the user closes the tab or window, for example.
      if (IsTimingDistilledPage())
        UMAHelper::LogTimeOnDistilledPage(GetElapsedTime());
    }

    // Starts if not already started.
    void Start(bool is_distilled_page);
    // Starts up again.
    void Resume();
    // Pauses if not already paused.
    void Pause();
    // Resets the timer and whether it is tracking a distilled page.
    void Reset();
    // Returns true if the timer is not zeroed, whether it is paused or
    // currently running.
    bool HasStarted();
    // If the timer is not zeroed and the timer is associated with a distilled
    // page.
    bool IsTimingDistilledPage();
    // The amount of active time so far.
    base::TimeDelta GetElapsedTime();

   private:
    base::TimeDelta total_active_time_;
    base::Time active_time_start_;
    bool is_distilled_page_ = false;
  };

  static void RecordReaderModeEntry(ReaderModeEntryPoint entry_point);
  static void RecordReaderModeExit(ReaderModeEntryPoint exit_point);

  // Timers at the old contents may need to be stopped / paused.
  static void UpdateTimersOnContentsChange(content::WebContents* web_contents,
                                           content::WebContents* old_contents);
  // Starts the timer if the page time is one which should be timed.
  static void StartTimerIfNeeded(content::WebContents* web_contents,
                                 ReaderModePageType page_type);
  // Timers may need to be paused on navigation.
  static void UpdateTimersOnNavigation(content::WebContents* web_contents,
                                       ReaderModePageType page_type);

  // Logs the time spent on a distillable page. Should be called just before
  // when switching to a distilled page. We are only interested in time on
  // distillable pages when the user then decides to distill the article.
  static void LogTimeOnDistillablePage(content::WebContents* web_contents);

  // Logs the time spent on a distilled (reader mode) page. Should be called
  // when leaving a distilled page, i.e. if the tab is closed or if the user
  // navigates to another page.
  static void LogTimeOnDistilledPage(base::TimeDelta time);

  UMAHelper() = delete;
  UMAHelper(const UMAHelper&) = delete;
  UMAHelper& operator=(const UMAHelper&) = delete;
};

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CONTENT_BROWSER_UMA_HELPER_H_
