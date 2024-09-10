// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/privacy_sandbox_notice_storage.h"

#include <string>

#include "base/json/values_util.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "components/prefs/pref_registry.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/privacy_sandbox/privacy_sandbox_notice_constants.h"

namespace privacy_sandbox {
namespace {

// Notice data will be saved as a dictionary in the PrefService of a profile.

// PrefService path.
constexpr char kPrivacySandboxNoticeDataPath[] = "privacy_sandbox.notices";

// Unsynced pref that indicates the schema version this profile is using in
// regards to the data model.
constexpr char kPrivacySandboxSchemaVersion[] = "schema_version";

// Unsynced pref that indicates the action taken relating to the notice.
constexpr char kPrivacySandboxNoticeActionTaken[] = "notice_action_taken";

// Unsynced pref that indicates the timestamp at which the action was taken. The
// action taken can be determined by the `notice_action_taken` pref.
constexpr char kPrivacySandboxNoticeActionTakenTime[] =
    "notice_action_taken_time";

// Unsynced pref that indicates when the notice was first shown. If this value
// isn't set, we can assume the notice was never shown.
constexpr char kPrivacySandboxNoticeFirstShown[] = "notice_first_shown";

// Unsynced pref that indicates when the notice was last shown across all
// sessions.
constexpr char kPrivacySandboxNoticeLastShown[] = "notice_last_shown";

// Unsynced pref that indicates the duration of how long the notice was shown
// across all sessions to when a user took action.
constexpr char kPrivacySandboxNoticeShownDuration[] = "notice_shown_duration";

std::string CreatePrefPath(std::string_view notice,
                           std::string_view pref_name) {
  return base::StrCat({notice, ".", pref_name});
}

void CreateTimingHistogram(const std::string& name, base::TimeDelta sample) {
  base::UmaHistogramCustomTimes(name, sample, base::Milliseconds(1),
                                base::Days(10), 100);
}

std::string GetNoticeActionString(NoticeActionTaken action) {
  switch (action) {
    case NoticeActionTaken::kNotSet:
    case NoticeActionTaken::kUnknownActionPreMigration:
      return "";
    case NoticeActionTaken::kAck:
      return "Ack";
    case NoticeActionTaken::kClosed:
      return "Closed";
    case NoticeActionTaken::kLearnMore:
      return "LearnMore";
    case NoticeActionTaken::kOptIn:
      return "OptIn";
    case NoticeActionTaken::kOptOut:
      return "OptOut";
    case NoticeActionTaken::kSettings:
      return "Settings";
    case NoticeActionTaken::kTimedOut:
      return "TimedOut";
    case NoticeActionTaken::kOther:
      return "Other";
  }
}

void SetSchemaVersion(PrefService* pref_service, std::string_view notice) {
  ScopedDictPrefUpdate update(pref_service, kPrivacySandboxNoticeDataPath);
  update.Get().SetByDottedPath(
      CreatePrefPath(notice, kPrivacySandboxSchemaVersion),
      kPrivacySandboxNoticeSchemaVersion);
}

void CheckNoticeNameEligibility(std::string_view notice_name) {
  CHECK(privacy_sandbox::kPrivacySandboxNoticeNames.contains(notice_name))
      << "Notice name " << notice_name
      << " does not exist in privacy_sandbox_notice_constants.h";
}

}  // namespace

// PrivacySandboxNoticeData definitions.
PrivacySandboxNoticeData::PrivacySandboxNoticeData() = default;
PrivacySandboxNoticeData& PrivacySandboxNoticeData::operator=(
    const PrivacySandboxNoticeData&) = default;
PrivacySandboxNoticeData::~PrivacySandboxNoticeData() = default;

// PrivacySandboxNoticeStorage definitions.
void PrivacySandboxNoticeStorage::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kPrivacySandboxNoticeDataPath);
}

void PrivacySandboxNoticeStorage::RecordHistogramsOnStartup(
    PrefService* pref_service,
    std::string_view notice) const {
  CheckNoticeNameEligibility(notice);
  auto notice_data = ReadNoticeData(pref_service, notice);

  NoticeStartupState startup_state;

  // If the notice entry doesn't exist, we don't emit any histograms.
  if (!pref_service->GetDict(kPrivacySandboxNoticeDataPath).contains(notice)) {
    return;
  }

  if (!notice_data.has_value() ||
      (notice_data->notice_first_shown == base::Time() &&
       notice_data->notice_action_taken == NoticeActionTaken::kNotSet)) {
    startup_state = NoticeStartupState::kPromptNotShown;
  } else if (notice_data->notice_first_shown == base::Time()) {
    // E.g. UnknownActionPreMigration && no first shown time set.
    startup_state = NoticeStartupState::kUnknownState;
  } else {  // Notice has been shown, action handling below.
    switch (notice_data->notice_action_taken) {
      case NoticeActionTaken::kNotSet:
        startup_state = NoticeStartupState::kPromptWaiting;
        break;
      case NoticeActionTaken::kOptIn:
        startup_state = NoticeStartupState::kFlowCompletedWithOptIn;
        break;
      case NoticeActionTaken::kOptOut:
        startup_state = NoticeStartupState::kFlowCompletedWithOptOut;
        break;
      case NoticeActionTaken::kUnknownActionPreMigration:
        startup_state = NoticeStartupState::kUnknownState;
        break;
      case NoticeActionTaken::kOther:
        startup_state = NoticeStartupState::kPromptOtherAction;
        break;
      case NoticeActionTaken::kTimedOut:
        startup_state = NoticeStartupState::kTimedOut;
        break;
      case NoticeActionTaken::kAck:
      case NoticeActionTaken::kClosed:
      case NoticeActionTaken::kSettings:
      case NoticeActionTaken::kLearnMore:
        startup_state = NoticeStartupState::kFlowCompleted;
        break;
    }
  }
  base::UmaHistogramEnumeration(
      base::StrCat({"PrivacySandbox.Notice.NoticeStartupState.", notice}),
      startup_state);
}

std::optional<PrivacySandboxNoticeData>
PrivacySandboxNoticeStorage::ReadNoticeData(PrefService* pref_service,
                                            std::string_view notice) const {
  CheckNoticeNameEligibility(notice);
  const base::Value::Dict& pref_data =
      pref_service->GetDict(kPrivacySandboxNoticeDataPath);
  if (!pref_data.contains(notice)) {
    return std::nullopt;
  }
  // Populate notice data values.
  std::optional<PrivacySandboxNoticeData> notice_data =
      PrivacySandboxNoticeData();

  // Schema version.
  std::optional<int> schema_version = pref_data.FindIntByDottedPath(
      CreatePrefPath(notice, kPrivacySandboxSchemaVersion));
  if (schema_version.has_value()) {
    notice_data->schema_version = *schema_version;
  }

  // Notice action taken time.
  std::optional<base::Time> notice_action_taken_time =
      base::ValueToTime(pref_data.FindByDottedPath(
          CreatePrefPath(notice, kPrivacySandboxNoticeActionTakenTime)));
  if (notice_action_taken_time.has_value()) {
    notice_data->notice_action_taken_time = *notice_action_taken_time;
  }

  // Notice first shown.
  std::optional<base::Time> notice_first_shown =
      base::ValueToTime(pref_data.FindByDottedPath(
          CreatePrefPath(notice, kPrivacySandboxNoticeFirstShown)));
  if (notice_first_shown.has_value()) {
    notice_data->notice_first_shown = *notice_first_shown;
  }

  // Notice last shown.
  std::optional<base::Time> notice_last_shown =
      base::ValueToTime(pref_data.FindByDottedPath(
          CreatePrefPath(notice, kPrivacySandboxNoticeLastShown)));
  if (notice_last_shown.has_value()) {
    notice_data->notice_last_shown = *notice_last_shown;
  }

  // Notice shown duration.
  std::optional<base::TimeDelta> notice_shown_duration =
      base::ValueToTimeDelta(pref_data.FindByDottedPath(
          CreatePrefPath(notice, kPrivacySandboxNoticeShownDuration)));
  if (notice_shown_duration.has_value()) {
    notice_data->notice_shown_duration = *notice_shown_duration;
  }

  // Enum handling.
  std::optional<int> notice_action_taken = pref_data.FindIntByDottedPath(
      CreatePrefPath(notice, kPrivacySandboxNoticeActionTaken));
  if (notice_action_taken && *notice_action_taken > 0 &&
      *notice_action_taken <= static_cast<int>(NoticeActionTaken::kMaxValue)) {
    notice_data->notice_action_taken =
        static_cast<NoticeActionTaken>(*notice_action_taken);
  }

  return notice_data;
}

void PrivacySandboxNoticeStorage::SetNoticeActionTaken(
    PrefService* pref_service,
    std::string_view notice,
    NoticeActionTaken notice_action_taken,
    base::Time notice_action_taken_time) {
  CheckNoticeNameEligibility(notice);
  ScopedDictPrefUpdate update(pref_service, kPrivacySandboxNoticeDataPath);
  auto notice_data = ReadNoticeData(pref_service, notice);

  // The notice should be shown first before action can be taken on it.
  if (!notice_data.has_value() ||
      notice_data->notice_first_shown == base::Time() ||
      notice_data->notice_last_shown == base::Time()) {
    base::UmaHistogramEnumeration(
        base::StrCat(
            {"PrivacySandbox.Notice.NoticeActionTakenBehavior.", notice}),
        NoticeActionBehavior::kActionBeforeShown);
    return;
  }

  // Overriding an existing notice action is unexpected.
  if (!(notice_data->notice_action_taken == NoticeActionTaken::kNotSet)) {
    base::UmaHistogramEnumeration(
        base::StrCat(
            {"PrivacySandbox.Notice.NoticeActionTakenBehavior.", notice}),
        NoticeActionBehavior::kDuplicateActionTaken);
    return;
  }

  update.Get().SetByDottedPath(
      CreatePrefPath(notice, kPrivacySandboxNoticeActionTaken),
      static_cast<int>(notice_action_taken));
  update.Get().SetByDottedPath(
      CreatePrefPath(notice, kPrivacySandboxNoticeActionTakenTime),
      base::TimeToValue(notice_action_taken_time));
  base::UmaHistogramEnumeration(
      base::StrCat(
          {"PrivacySandbox.Notice.NoticeActionTakenBehavior.", notice}),
      NoticeActionBehavior::kSuccess);

  // Emitting histograms.
  base::UmaHistogramEnumeration(
      base::StrCat({"PrivacySandbox.Notice.NoticeAction.", notice}),
      notice_action_taken);

  std::string notice_action_str = GetNoticeActionString(notice_action_taken);
  // First shown to interacted duration.
  if (!notice_action_str.empty()) {
    // Set first shown to interacted.
    base::TimeDelta first_shown_to_interacted_duration =
        notice_action_taken_time - notice_data->notice_first_shown;
    update.Get().SetByDottedPath(
        CreatePrefPath(notice, kPrivacySandboxNoticeShownDuration),
        base::TimeDeltaToValue(first_shown_to_interacted_duration));
    CreateTimingHistogram(
        base::StrCat({"PrivacySandbox.Notice.FirstShownToInteractedDuration.",
                      notice, "_", notice_action_str}),
        first_shown_to_interacted_duration);

    // Set last shown to interacted.
    auto last_shown_to_interacted_duration =
        notice_action_taken_time - notice_data->notice_last_shown;
    CreateTimingHistogram(
        base::StrCat({"PrivacySandbox.Notice.LastShownToInteractedDuration.",
                      notice, "_", notice_action_str}),
        last_shown_to_interacted_duration);
  }
}

void PrivacySandboxNoticeStorage::SetNoticeShown(PrefService* pref_service,
                                                 std::string_view notice,
                                                 base::Time notice_shown_time) {
  CheckNoticeNameEligibility(notice);
  ScopedDictPrefUpdate update(pref_service, kPrivacySandboxNoticeDataPath);
  // Only set notice first shown if it hasn't previously been set.
  if (!pref_service->GetDict(kPrivacySandboxNoticeDataPath)
           .FindByDottedPath(
               CreatePrefPath(notice, kPrivacySandboxNoticeFirstShown))) {
    update.Get().SetByDottedPath(
        CreatePrefPath(notice, kPrivacySandboxNoticeFirstShown),
        base::TimeToValue(notice_shown_time));
    SetSchemaVersion(pref_service, notice);
    base::UmaHistogramBoolean(
        base::StrCat({"PrivacySandbox.Notice.NoticeShown.", notice}), true);
  }

  // Always set notice last shown.
  update.Get().SetByDottedPath(
      CreatePrefPath(notice, kPrivacySandboxNoticeLastShown),
      base::TimeToValue(notice_shown_time));
}

void PrivacySandboxNoticeStorage::MigratePrivacySandboxNoticeData(
    PrefService* pref_service,
    const PrivacySandboxNoticeData& input,
    std::string_view notice) {
  CheckNoticeNameEligibility(notice);
  ScopedDictPrefUpdate update(pref_service, kPrivacySandboxNoticeDataPath);

  SetSchemaVersion(pref_service, notice);

  // We are only setting the new prefs and emitting histograms if the new prefs
  // haven't been set already.
  auto existing_notice_data = ReadNoticeData(pref_service, notice);
  if (input.notice_action_taken != NoticeActionTaken::kNotSet &&
      (!existing_notice_data.has_value() ||
       existing_notice_data->notice_action_taken ==
           NoticeActionTaken::kNotSet)) {
    update.Get().SetByDottedPath(
        CreatePrefPath(notice, kPrivacySandboxNoticeActionTaken),
        static_cast<int>(input.notice_action_taken));
    base::UmaHistogramEnumeration(
        base::StrCat({"PrivacySandbox.Notice.NoticeAction.", notice}),
        input.notice_action_taken);
  }

  if (input.notice_action_taken_time != base::Time() &&
      (!existing_notice_data.has_value() ||
       existing_notice_data->notice_action_taken_time == base::Time())) {
    update.Get().SetByDottedPath(
        CreatePrefPath(notice, kPrivacySandboxNoticeActionTakenTime),
        base::TimeToValue(input.notice_action_taken_time));

    // First shown to interacted histogram.
    std::string notice_action_str =
        GetNoticeActionString(input.notice_action_taken);
    if (!notice_action_str.empty() &&
        input.notice_first_shown != base::Time()) {
      base::TimeDelta first_shown_to_interacted_duration =
          input.notice_action_taken_time - input.notice_first_shown;
      update.Get().SetByDottedPath(
          CreatePrefPath(notice, kPrivacySandboxNoticeShownDuration),
          base::TimeDeltaToValue(first_shown_to_interacted_duration));
      CreateTimingHistogram(
          base::StrCat({"PrivacySandbox.Notice.FirstShownToInteractedDuration.",
                        notice, "_", notice_action_str}),
          first_shown_to_interacted_duration);
    }

    // Last shown to interacted histogram.
    if (!notice_action_str.empty() && input.notice_last_shown != base::Time()) {
      auto last_shown_to_interacted_duration =
          input.notice_action_taken_time - input.notice_last_shown;
      CreateTimingHistogram(
          base::StrCat({"PrivacySandbox.Notice.LastShownToInteractedDuration.",
                        notice, "_", notice_action_str}),
          last_shown_to_interacted_duration);
    }
  }

  if (input.notice_first_shown != base::Time() &&
      (!existing_notice_data.has_value() ||
       existing_notice_data->notice_first_shown == base::Time())) {
    update.Get().SetByDottedPath(
        CreatePrefPath(notice, kPrivacySandboxNoticeFirstShown),
        base::TimeToValue(input.notice_first_shown));
    base::UmaHistogramBoolean(
        base::StrCat({"PrivacySandbox.Notice.NoticeShown.", notice}), true);
  }

  if (input.notice_last_shown != base::Time() &&
      (!existing_notice_data.has_value() ||
       existing_notice_data->notice_last_shown == base::Time())) {
    update.Get().SetByDottedPath(
        CreatePrefPath(notice, kPrivacySandboxNoticeLastShown),
        base::TimeToValue(input.notice_last_shown));
  }
}

}  // namespace privacy_sandbox
