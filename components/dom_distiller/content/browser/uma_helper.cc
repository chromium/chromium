// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/content/browser/uma_helper.h"

#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "components/dom_distiller/content/browser/distillability_driver.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"

namespace dom_distiller {

void UMAHelper::DistillabilityDriverTimer::Start(bool is_distilled_page) {
  if (HasStarted())
    DCHECK_EQ(is_distilled_page_, is_distilled_page);
  is_distilled_page_ = is_distilled_page;
  if (active_time_start_ != base::Time())
    return;
  active_time_start_ = base::Time::Now();
}

void UMAHelper::DistillabilityDriverTimer::Resume() {
  DCHECK(HasStarted());
  if (active_time_start_ != base::Time())
    return;
  active_time_start_ = base::Time::Now();
}

void UMAHelper::DistillabilityDriverTimer::Pause() {
  // Return early if already paused.
  if (active_time_start_ == base::Time())
    return;
  total_active_time_ += base::Time::Now() - active_time_start_;
  active_time_start_ = base::Time();
}

void UMAHelper::DistillabilityDriverTimer::Reset() {
  active_time_start_ = base::Time();
  total_active_time_ = base::TimeDelta();
  is_distilled_page_ = false;
}

bool UMAHelper::DistillabilityDriverTimer::HasStarted() {
  return active_time_start_ != base::Time() ||
         total_active_time_ != base::TimeDelta();
}

base::TimeDelta UMAHelper::DistillabilityDriverTimer::GetElapsedTime() {
  // If the timer is unpaused, add in the current time too.
  if (active_time_start_ != base::Time())
    return total_active_time_ + (base::Time::Now() - active_time_start_);
  return total_active_time_;
}

// static
void UMAHelper::RecordReaderModeEntry(ReaderModeEntryPoint entry_point) {
  // Use histograms instead of user actions because order doesn't matter.
  base::UmaHistogramEnumeration("DomDistiller.ReaderMode.EntryPoint",
                                entry_point);
}

// static
void UMAHelper::RecordReaderModeExit(ReaderModeEntryPoint exit_point) {
  // Use histograms instead of user actions because order doesn't matter.
  base::UmaHistogramEnumeration("DomDistiller.ReaderMode.ExitPoint",
                                exit_point);
}

// static
void UMAHelper::UpdateTimersOnContentsChange(
    content::WebContents* web_contents,
    content::WebContents* old_contents) {
  if (old_contents && old_contents != web_contents) {
    // Pause the timer on the the driver at the old contents.
    DistillabilityDriver::CreateForWebContents(old_contents);
    DistillabilityDriver* old_driver =
        DistillabilityDriver::FromWebContents(old_contents);
    CHECK(old_driver);
    if (old_driver->GetTimer().HasStarted()) {
      old_driver->GetTimer().Pause();
    }
  }

  CHECK(web_contents);
  DistillabilityDriver::CreateForWebContents(web_contents);
  DistillabilityDriver* driver =
      DistillabilityDriver::FromWebContents(web_contents);
  CHECK(driver);

  // If we were already timing the new page, pause or resume as necessary.
  if (!driver->GetTimer().HasStarted())
    return;

  if (web_contents->GetVisibility() != content::Visibility::VISIBLE) {
    // Pause any running timer if the web contents are no longer visible.
    driver->GetTimer().Pause();
    return;
  }
  // Resume the driver's timer when contents have come back into focus.
  driver->GetTimer().Resume();
}

// static
void UMAHelper::StartTimerIfNeeded(content::WebContents* web_contents,
                                   ReaderModePageType page_type) {
  CHECK(web_contents);
  DistillabilityDriver::CreateForWebContents(web_contents);
  DistillabilityDriver* driver =
      DistillabilityDriver::FromWebContents(web_contents);
  CHECK(driver);

  if (page_type == ReaderModePageType::kDistilled) {
    // If this is a distilled page, ensure the timer is running.
    driver->GetTimer().Start(/* is_distilled_page */ true);
  } else if (page_type == ReaderModePageType::kDistillable) {
    // If we are on a distillable page, ensure the timer is running.
    driver->GetTimer().Start(false);
  }
}

// static
void UMAHelper::UpdateTimersOnNavigation(content::WebContents* web_contents,
                                         ReaderModePageType page_type) {
  CHECK(web_contents);
  DistillabilityDriver::CreateForWebContents(web_contents);
  DistillabilityDriver* driver =
      DistillabilityDriver::FromWebContents(web_contents);
  CHECK(driver);

  if (!driver->GetTimer().HasStarted())
    return;

  // Stop timing distilled pages when a user navigates away.
  driver->GetTimer().Reset();
}

}  // namespace dom_distiller
