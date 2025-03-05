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

// Startup states. These values are persisted to logs. Entries should not be
// renumbered and numeric values should never be reused.
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
  // kPromptOtherAction = 6,  // no longer used
  // kTimedOut = 7,  // no longer used,
  kMaxValue = kPromptWaiting,
};
// LINT.ThenChange(//tools/metrics/histograms/enums.xml:PrivacySandboxNoticeStartupState)

// Different notice actions. These values are persisted to logs. Entries should
// not be renumbered and numeric values should never be reused.
// LINT.IfChange(NoticeActionTaken)
enum class NoticeActionTaken {
  // No Ack action set.
  kNotSet = 0,
  // ACK'ed the notice using 'GotIt' or some other form of acknowledgement.
  kAck = 1,
  // Action taken clicking the 'x' button.
  kClosed = 2,
  // TODO(crbug.com/392088228): In the process of deprecating, do not use.
  kLearnMore_Deprecated = 3,
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

enum class NoticeEvent {
  // ACK'ed the notice using 'GotIt' or some other form of acknowledgement.
  kAck = 0,
  // Action taken clicking the 'x' button.
  kClosed = 1,
  // Opted in/Consented to the notice using 'Turn it on' or some other form of
  // explicit consent.
  kOptIn = 2,
  // Action taken to dismiss or opt out of the notice using 'No Thanks' or some
  // other form of dismissal.
  kOptOut = 3,
  // Action taken clicking the settings button.
  kSettings = 4,
  // Notice shown.
  kShown = 5,
  kMaxValue = kShown,
};

// Different notice action outcomes. These values are persisted to logs. Entries
// should not be renumbered and numeric values should never be reused.
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

class PrivacySandboxNoticeData {
 public:
  PrivacySandboxNoticeData();
  PrivacySandboxNoticeData& operator=(const PrivacySandboxNoticeData&);
  ~PrivacySandboxNoticeData();
  PrivacySandboxNoticeData(const PrivacySandboxNoticeData& data);

  int GetSchemaVersion() const;
  std::string GetChromeVersion() const;
  std::vector<std::pair<NoticeEvent, base::Time>> GetNoticeEvents() const;

  void SetSchemaVersion(int schema_version);
  void SetChromeVersion(std::string_view chrome_version);
  void SetNoticeEvents(
      const std::vector<std::pair<NoticeEvent, base::Time>>& events);

  // Gets the timestamp when the notice was first shown. If the notice was never
  // shown, the default timestamp will be returned.
  std::optional<base::Time> GetNoticeFirstShownFromEvents();

  // Gets the timestamp when the notice was last shown. If the notice was never
  // shown, the default timestamp will be returned.
  std::optional<base::Time> GetNoticeLastShownFromEvents();

  // Gets the notice action taken and when it was taken the first time the
  // notice was shown. If the notice hasn't been shown for the first time, or
  // there was no action associated, no value is returned. If there are multiple
  // actions associated, only the last action is returned.
  std::optional<std::pair<NoticeEvent, base::Time>>
  GetNoticeActionTakenForFirstShownFromEvents();

  // TODO(crbug.com/392088228): Remove other actions once the new event fields
  // are written to. Stores information about profile interactions on a notice.
  NoticeActionTaken notice_action_taken_ = NoticeActionTaken::kNotSet;
  base::Time notice_action_taken_time_;
  base::Time notice_first_shown_;
  base::Time notice_last_shown_;
  base::TimeDelta notice_shown_duration_;

 private:
  int schema_version_ = 0;
  std::string chrome_version_;
  std::vector<std::pair<NoticeEvent, base::Time>> notice_events_;
};

// Stores pre-migration interactions on a notice in the v1 schema.
struct V1MigrationData {
  V1MigrationData();
  ~V1MigrationData();
  NoticeActionTaken notice_action_taken = NoticeActionTaken::kNotSet;
  base::Time notice_action_taken_time;
  base::Time notice_last_shown;
};

class PrivacySandboxNoticeStorage {
 public:
  PrivacySandboxNoticeStorage() = default;
  ~PrivacySandboxNoticeStorage() = default;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Reads PrivacySandbox notice & consent prefs. Returns std::nullopt if all
  // prefs aren't set. If an event is tracked but the event timestamp is
  // missing, return base::Time(). If an event timestamp is tracked but the
  // event itself is missing, return NoticeEvent::kUnknownAction.
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

  // Migration functions.

  // Updates fields to schema version 2.
  // TODO(crbug.com/392088228): Remove this once deprecation of old V1 fields is
  // complete.
  static void UpdateNoticeSchemaV2(PrefService* pref_service);

  // Migrates fields in the notice data v1 schema to the notice data v2 schema.
  static PrivacySandboxNoticeData ConvertV1SchemaToV2Schema(
      const V1MigrationData& data_v1);

  // Converts the schema v1 NoticeActionTaken to the schema v2 NoticeEvent.
  static std::optional<NoticeEvent> NoticeActionToNoticeEvent(
      NoticeActionTaken action);

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
