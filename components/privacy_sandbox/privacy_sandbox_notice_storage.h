// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_NOTICE_STORAGE_H_
#define COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_NOTICE_STORAGE_H_

#include <optional>
#include <string>

#include "base/no_destructor.h"
#include "base/time/time.h"
#include "components/prefs/pref_registry_simple.h"

class PrefService;

namespace privacy_sandbox {

// Startup states
// LINT.IfChange(NoticeStartupState)
enum class NoticeStartupState {
  // Incorrect or unknown states, for example if the notice hasn't been shown
  // but an action is set.
  kUnknownState = 0,
  // Prompt/notice not shown.
  kPromptNotShown = 1,
  // Notice action flow completed.
  kFlowCompleted = 2,
  // Notice action flow completed with action opt in.
  kFlowCompletedWithOptIn = 3,
  // Notice action flow completed with action opt out.
  kFlowCompletedWithOptOut = 4,
  // Prompt/notice still waiting for action.
  kPromptWaiting = 5,
  // Prompt/notice had an action other then the specified actions performed on
  // it.
  kPromptOtherAction = 6,
  // Prompt/notice timed out.
  kTimedOut = 7,
  kMaxValue = kTimedOut,
};
// LINT.ThenChange(//tools/metrics/histograms/enums.xml:PrivacySandboxNoticeStartupState)

// Different notice actions.
// LINT.IfChange(NoticeActionTaken)
enum class NoticeActionTaken {
  // No Ack action set.
  kNotSet = 0,
  // ACK'ed the notice using 'GotIt' or some other form of acknowledgement.
  kAck = 1,
  // Action taken clicking the 'x' button.
  kClosed = 2,
  // Action taken clicking the learn more button.
  kLearnMore = 3,
  // Opted in/Consented to the notice using 'Turn it on' or some other form of
  // explicit consent.
  kOptIn = 4,
  // Action taken to dismiss or opt out of the notice using 'No Thanks' or some
  // other form of dismissal.
  kOptOut = 5,
  // Action taken some other way.
  kOther = 6,
  // Action taken clicking the settings button.
  kSettings = 7,
  // Action taken unknown as it was recorded pre-migration.
  kUnknownActionPreMigration = 8,
  // No action taken, the notice timed out.
  kTimedOut = 9,
  kMaxValue = kTimedOut,
};
// LINT.ThenChange(//tools/metrics/histograms/enums.xml:PrivacySandboxNoticeAction)

// Different notice action outcomes.
// LINT.IfChange(NoticeActionBehavior)
enum class NoticeActionBehavior {
  // Action taken on notice set successfully.
  kSuccess = 0,
  // Tried to set action taken before notice was shown, unexpected behavior.
  kActionBeforeShown = 1,
  // Tried to set action taken twice, unexpected behavior.
  kDuplicateActionTaken = 2,
  kMaxValue = kDuplicateActionTaken,
};
// LINT.ThenChange(//tools/metrics/histograms/enums.xml:PrivacySandboxNoticeActionBehavior)

// Stores information about profile interactions on a notice.
struct PrivacySandboxNoticeData {
  PrivacySandboxNoticeData();
  PrivacySandboxNoticeData& operator=(const PrivacySandboxNoticeData&);
  ~PrivacySandboxNoticeData();
  int schema_version = 0;
  NoticeActionTaken notice_action_taken = NoticeActionTaken::kNotSet;
  base::Time notice_action_taken_time;
  base::Time notice_first_shown;
  base::Time notice_last_shown;
  base::TimeDelta notice_shown_duration;
};

class PrivacySandboxNoticeStorage {
 public:
  PrivacySandboxNoticeStorage() = default;
  ~PrivacySandboxNoticeStorage() = default;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Reads PrivacySandbox notice & consent prefs. Returns std::nullopt if prefs
  // aren't set.
  std::optional<PrivacySandboxNoticeData> ReadNoticeData(
      PrefService* pref_service,
      std::string_view notice) const;

  // Records histograms tracking the state of notice flow on startup.
  void RecordHistogramsOnStartup(PrefService* pref_service,
                                 std::string_view notice) const;

  // Sets the pref and histogram controlling the action taken on the notice.
  void SetNoticeActionTaken(PrefService* pref_service,
                            std::string_view notice,
                            NoticeActionTaken notice_action_taken,
                            base::Time notice_action_taken_time);

  // Updates the pref and histogram controlling whether the notice has been
  // shown.
  void SetNoticeShown(PrefService* pref_service,
                      std::string_view notice,
                      base::Time notice_shown_time);

  // Functionality should only be used to migrate pre-notice storage prefs.
  // TODO(crbug.com/333406690): Remove this once the old privacy sandbox prefs
  // are migrated to the new data model.
  void MigratePrivacySandboxNoticeData(
      PrefService* pref_service,
      const PrivacySandboxNoticeData& notice_data,
      std::string_view notice);

  PrivacySandboxNoticeStorage(const PrivacySandboxNoticeStorage&) = delete;
  PrivacySandboxNoticeStorage& operator=(const PrivacySandboxNoticeStorage&) =
      delete;
};

}  // namespace privacy_sandbox

#endif  // COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_NOTICE_STORAGE_H_
