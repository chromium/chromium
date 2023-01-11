// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_ANDROID_WRAPPING_TEST_TRACKER_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_ANDROID_WRAPPING_TEST_TRACKER_H_

#include <memory>
#include <string>

#include "base/android/jni_android.h"
#include "base/functional/callback.h"
#include "components/feature_engagement/public/tracker.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace feature_engagement {
// This class wraps a Tracker from Java and forwards to it all calls received.
class WrappingTestTracker : public Tracker {
 public:
  explicit WrappingTestTracker(const base::android::JavaRef<jobject>& jtracker);

  WrappingTestTracker(const WrappingTestTracker&) = delete;
  WrappingTestTracker& operator=(const WrappingTestTracker&) = delete;

  ~WrappingTestTracker() override;

  // TrackerImpl:

  void NotifyEvent(const std::string& event) override;
  bool ShouldTriggerHelpUI(const base::Feature& feature) override;
  TriggerDetails ShouldTriggerHelpUIWithSnooze(
      const base::Feature& feature) override;
  bool WouldTriggerHelpUI(const base::Feature& feature) const override;
  bool HasEverTriggered(const base::Feature& feature,
                        bool from_window) const override;
  TriggerState GetTriggerState(const base::Feature& feature) const override;
  void Dismissed(const base::Feature& feature) override;
  void DismissedWithSnooze(const base::Feature& feature,
                           absl::optional<SnoozeAction> snooze_action) override;
  std::unique_ptr<DisplayLockHandle> AcquireDisplayLock() override;
  absl::optional<std::string> GetPendingPriorityNotification() override;
  void SetPriorityNotification(const base::Feature& feature) override;
  void RegisterPriorityNotificationHandler(const base::Feature& feature,
                                           base::OnceClosure callback) override;
  void UnregisterPriorityNotificationHandler(
      const base::Feature& feature) override;
  bool IsInitialized() const override;
  void AddOnInitializedCallback(OnInitializedCallback callback) override;

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_tracker_;
};

}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_ANDROID_WRAPPING_TEST_TRACKER_H_
