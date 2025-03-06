// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/privacy_sandbox_notice_storage.h"

#include <string>

#include "base/containers/adapters.h"
#include "base/json/values_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "base/version_info/version_info.h"
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

// Unsynced pref that indicates the chrome version this profile was initially
// shown the notice at. For migrated notices, this pref is empty.
constexpr char kPrivacySandboxChromeVersion[] = "chrome_version";

// Unsynced pref that indicates the events taken on the notice. Stored as a
// sorted list in order of event performed containing dict entries.
constexpr char kPrivacySandboxEvents[] = "events";

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

// Key value in the dict entry contained within `events`
constexpr char kPrivacySandboxNoticeEvent[] = "event";

// Key value in the dict entry contained within `events`
constexpr char kPrivacySandboxNoticeEventTime[] = "timestamp";

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
    case NoticeActionTaken::kAck:
      return "Ack";
    case NoticeActionTaken::kClosed:
      return "Closed";
    case NoticeActionTaken::kOptIn:
      return "OptIn";
    case NoticeActionTaken::kOptOut:
      return "OptOut";
    case NoticeActionTaken::kSettings:
      return "Settings";
    default:
      return "";
  }
}

void SetSchemaVersion(PrefService* pref_service, std::string_view notice) {
  ScopedDictPrefUpdate update(pref_service, kPrivacySandboxNoticeDataPath);
  update.Get().SetByDottedPath(
      CreatePrefPath(notice, kPrivacySandboxSchemaVersion),
      kPrivacySandboxNoticeSchemaVersion);
}

base::Value::Dict BuildDictEntryEvent(NoticeEvent event,
                                      base::Time event_time) {
  base::Value::Dict params;
  params.Set(kPrivacySandboxNoticeEvent, static_cast<int>(event));
  params.Set(kPrivacySandboxNoticeEventTime, base::TimeToValue(event_time));
  return params;
}

void SetChromeVersion(PrefService* pref_service, std::string_view notice) {
  ScopedDictPrefUpdate update(pref_service, kPrivacySandboxNoticeDataPath);
  update.Get().SetByDottedPath(
      CreatePrefPath(notice, kPrivacySandboxChromeVersion),
      version_info::GetVersionNumber());
}

void CheckNoticeNameEligibility(std::string_view notice_name) {
  CHECK(privacy_sandbox::kPrivacySandboxNoticeNames.contains(notice_name))
      << "Notice name " << notice_name
      << " does not exist in privacy_sandbox_notice_constants.h";
}

std::optional<V1MigrationData> ExtractV1NoticeData(
    PrefService* pref_service,
    std::string_view notice,
    const base::Value::Dict& data) {
  std::optional<int> schema_version = data.FindIntByDottedPath(
      CreatePrefPath(notice, kPrivacySandboxSchemaVersion));

  if (!schema_version.has_value() || *schema_version != 1) {
    return std::nullopt;
  }

  // Notice last shown.
  std::optional<base::Time> shown_v1 = base::ValueToTime(data.FindByDottedPath(
      CreatePrefPath(notice, kPrivacySandboxNoticeLastShown)));
  V1MigrationData migration_data;
  if (shown_v1.has_value()) {
    migration_data.notice_last_shown = *shown_v1;
  }

  // Action taken.
  std::optional<int> action_v1 = data.FindIntByDottedPath(
      CreatePrefPath(notice, kPrivacySandboxNoticeActionTaken));
  if (action_v1.has_value()) {
    migration_data.notice_action_taken =
        static_cast<NoticeActionTaken>(*action_v1);
  }

  // Action taken time.
  std::optional<base::Time> action_time_v1 =
      base::ValueToTime(data.FindByDottedPath(
          CreatePrefPath(notice, kPrivacySandboxNoticeActionTakenTime)));
  if (action_time_v1.has_value()) {
    migration_data.notice_action_taken_time = *action_time_v1;
  }

  return migration_data;
}

void PopulateV2NoticeData(PrefService* pref_service,
                          std::string_view notice,
                          const PrivacySandboxNoticeData& data) {
  ScopedDictPrefUpdate update(pref_service, kPrivacySandboxNoticeDataPath);
  update.Get().SetByDottedPath(
      CreatePrefPath(notice, kPrivacySandboxSchemaVersion),
      data.GetSchemaVersion());

  for (const auto& event : data.GetNoticeEvents()) {
    update.Get()
        .EnsureDict(notice)
        ->EnsureList(kPrivacySandboxEvents)
        ->Append(BuildDictEntryEvent(event.first, event.second));
  }
}

}  // namespace

// PrivacySandboxNoticeData definitions.
PrivacySandboxNoticeData::PrivacySandboxNoticeData() = default;
PrivacySandboxNoticeData& PrivacySandboxNoticeData::operator=(
    const PrivacySandboxNoticeData&) = default;
PrivacySandboxNoticeData::~PrivacySandboxNoticeData() = default;
PrivacySandboxNoticeData::PrivacySandboxNoticeData(
    const PrivacySandboxNoticeData& data) = default;

int PrivacySandboxNoticeData::GetSchemaVersion() const {
  return schema_version_;
}
std::string PrivacySandboxNoticeData::GetChromeVersion() const {
  return chrome_version_;
}
std::vector<std::pair<NoticeEvent, base::Time>>
PrivacySandboxNoticeData::GetNoticeEvents() const {
  return notice_events_;
}

void PrivacySandboxNoticeData::SetSchemaVersion(int schema_version) {
  schema_version_ = schema_version;
}

void PrivacySandboxNoticeData::SetChromeVersion(
    std::string_view chrome_version) {
  chrome_version_ = chrome_version;
}

void PrivacySandboxNoticeData::SetNoticeEvents(
    const std::vector<std::pair<NoticeEvent, base::Time>>& events) {
  notice_events_ = events;
}

std::optional<base::Time>
PrivacySandboxNoticeData::GetNoticeFirstShownFromEvents() {
  for (const auto event : notice_events_) {
    if (event.first == NoticeEvent::kShown) {
      return event.second;
    }
  }
  return std::nullopt;
}

std::optional<base::Time>
PrivacySandboxNoticeData::GetNoticeLastShownFromEvents() {
  for (const auto& notice_event : base::Reversed(notice_events_)) {
    if (notice_event.first == NoticeEvent::kShown) {
      return notice_event.second;
    }
  }
  return std::nullopt;
}

std::optional<std::pair<NoticeEvent, base::Time>>
PrivacySandboxNoticeData::GetNoticeActionTakenForFirstShownFromEvents() {
  std::optional<std::pair<NoticeEvent, base::Time>> notice_action_pair;
  int last_shown_idx = 0;
  int first_notice_idx = 0;
  for (auto event : notice_events_) {
    if (event.first == NoticeEvent::kShown) {
      last_shown_idx++;
    } else if (!notice_action_pair.has_value() ||
               first_notice_idx == last_shown_idx) {
      first_notice_idx = last_shown_idx;
      notice_action_pair = event;
    }
  }
  return notice_action_pair;
}

// V1MigrationData definitions.
V1MigrationData::V1MigrationData() = default;
V1MigrationData::~V1MigrationData() = default;

// PrivacySandboxNoticeStorage definitions.
void PrivacySandboxNoticeStorage::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kPrivacySandboxNoticeDataPath);
}

// static
std::optional<NoticeEvent>
PrivacySandboxNoticeStorage::NoticeActionToNoticeEvent(
    NoticeActionTaken action) {
  switch (action) {
    case NoticeActionTaken::kAck:
      return NoticeEvent::kAck;
    case NoticeActionTaken::kClosed:
      return NoticeEvent::kClosed;
    case NoticeActionTaken::kOptIn:
      return NoticeEvent::kOptIn;
    case NoticeActionTaken::kOptOut:
      return NoticeEvent::kOptOut;
    case NoticeActionTaken::kSettings:
      return NoticeEvent::kSettings;
    default:
      return std::nullopt;
  }
}

// static
PrivacySandboxNoticeData PrivacySandboxNoticeStorage::ConvertV1SchemaToV2Schema(
    const V1MigrationData& data_v1) {
  PrivacySandboxNoticeData data_v2;
  std::vector<std::pair<NoticeEvent, base::Time>> notice_events;
  data_v2.SetSchemaVersion(2);

  if (data_v1.notice_last_shown != base::Time()) {
    notice_events.emplace_back(NoticeEvent::kShown, data_v1.notice_last_shown);
  }

  auto notice_event = NoticeActionToNoticeEvent(data_v1.notice_action_taken);
  if (notice_event.has_value()) {
    notice_events.emplace_back(*notice_event, data_v1.notice_action_taken_time);
  }

  data_v2.SetNoticeEvents(notice_events);
  return data_v2;
}

// static
void PrivacySandboxNoticeStorage::UpdateNoticeSchemaV2(
    PrefService* pref_service) {
  const base::Value::Dict* data =
      pref_service->GetUserPrefValue(kPrivacySandboxNoticeDataPath)
          ->GetIfDict();

  for (const auto notice : kPrivacySandboxNoticeNames) {
    if (!data || !data->contains(notice)) {
      continue;
    }

    std::optional<int> schema_version = data->FindIntByDottedPath(
        CreatePrefPath(notice, kPrivacySandboxSchemaVersion));
    if (schema_version.has_value() && *schema_version == 2) {
      continue;
    }

    auto data_v1 = ExtractV1NoticeData(pref_service, notice, *data);
    if (!data_v1) {
      return;
    }

    PrivacySandboxNoticeData data_v2 = ConvertV1SchemaToV2Schema(*data_v1);

    PopulateV2NoticeData(pref_service, notice, data_v2);
  }
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
      (notice_data->notice_first_shown_ == base::Time() &&
       notice_data->notice_action_taken_ == NoticeActionTaken::kNotSet)) {
    startup_state = NoticeStartupState::kPromptNotShown;
  } else if (notice_data->notice_first_shown_ == base::Time()) {
    // E.g. UnknownActionPreMigration && no first shown time set.
    startup_state = NoticeStartupState::kUnknownState;
  } else {  // Notice has been shown, action handling below.
    switch (notice_data->notice_action_taken_) {
      case NoticeActionTaken::kNotSet:
      case NoticeActionTaken::kLearnMore_Deprecated:
        startup_state = NoticeStartupState::kPromptWaiting;
        break;
      case NoticeActionTaken::kOptIn:
        startup_state = NoticeStartupState::kFlowCompletedWithOptIn;
        break;
      case NoticeActionTaken::kOptOut:
        startup_state = NoticeStartupState::kFlowCompletedWithOptOut;
        break;
      case NoticeActionTaken::kAck:
      case NoticeActionTaken::kClosed:
      case NoticeActionTaken::kSettings:
        startup_state = NoticeStartupState::kFlowCompleted;
        break;
      default:
        startup_state = NoticeStartupState::kUnknownState;
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
    notice_data->SetSchemaVersion(*schema_version);
  }

  // Chrome version.
  const std::string* chrome_version = pref_data.FindStringByDottedPath(
      CreatePrefPath(notice, kPrivacySandboxChromeVersion));
  if (chrome_version) {
    notice_data->SetChromeVersion(*chrome_version);
  }

  // Notice action taken time.
  std::optional<base::Time> notice_action_taken_time =
      base::ValueToTime(pref_data.FindByDottedPath(
          CreatePrefPath(notice, kPrivacySandboxNoticeActionTakenTime)));
  if (notice_action_taken_time.has_value()) {
    notice_data->notice_action_taken_time_ = *notice_action_taken_time;
  }

  // Notice first shown.
  std::optional<base::Time> notice_first_shown =
      base::ValueToTime(pref_data.FindByDottedPath(
          CreatePrefPath(notice, kPrivacySandboxNoticeFirstShown)));
  if (notice_first_shown.has_value()) {
    notice_data->notice_first_shown_ = *notice_first_shown;
  }

  // Notice last shown.
  std::optional<base::Time> notice_last_shown =
      base::ValueToTime(pref_data.FindByDottedPath(
          CreatePrefPath(notice, kPrivacySandboxNoticeLastShown)));
  if (notice_last_shown.has_value()) {
    notice_data->notice_last_shown_ = *notice_last_shown;
  }

  // Notice shown duration.
  std::optional<base::TimeDelta> notice_shown_duration =
      base::ValueToTimeDelta(pref_data.FindByDottedPath(
          CreatePrefPath(notice, kPrivacySandboxNoticeShownDuration)));
  if (notice_shown_duration.has_value()) {
    notice_data->notice_shown_duration_ = *notice_shown_duration;
  }

  // Enum handling.
  std::optional<int> notice_action_taken = pref_data.FindIntByDottedPath(
      CreatePrefPath(notice, kPrivacySandboxNoticeActionTaken));
  if (notice_action_taken && *notice_action_taken > 0 &&
      *notice_action_taken <= static_cast<int>(NoticeActionTaken::kMaxValue)) {
    notice_data->notice_action_taken_ =
        static_cast<NoticeActionTaken>(*notice_action_taken);
  }

  const base::Value::List* events = pref_data.FindListByDottedPath(
      CreatePrefPath(notice, kPrivacySandboxEvents));

  std::vector<std::pair<NoticeEvent, base::Time>> notice_events;
  if (events) {
    for (const base::Value& event : *events) {
      const auto* dict = event.GetIfDict();
      if (!dict) {
        continue;
      }
      auto notice_event_taken = dict->FindInt(kPrivacySandboxNoticeEvent);
      if (!notice_event_taken) {
        continue;
      }
      const base::Value* notice_event_taken_time =
          dict->Find(kPrivacySandboxNoticeEventTime);

      std::optional<base::Time> timestamp;
      if (notice_event_taken_time) {
        timestamp = base::ValueToTime(*notice_event_taken_time);
      }
      notice_events.emplace_back(static_cast<NoticeEvent>(*notice_event_taken),
                                 timestamp.value_or(base::Time()));
    }
  }
  notice_data->SetNoticeEvents(notice_events);

  return notice_data;
}

void PrivacySandboxNoticeStorage::SetNoticeActionTaken(
    PrefService* pref_service,
    std::string_view notice,
    NoticeActionTaken notice_action_taken,
    base::Time notice_action_taken_time) {
  CheckNoticeNameEligibility(notice);
  // TODO(crbug.com/392088228): Remove once migration is complete.
  CHECK(notice_action_taken != NoticeActionTaken::kLearnMore_Deprecated)
      << "kLearnMore_Deprecated action is deprecated and should not be used.";
  ScopedDictPrefUpdate update(pref_service, kPrivacySandboxNoticeDataPath);
  auto notice_data = ReadNoticeData(pref_service, notice);

  // The notice should be shown first before action can be taken on it.
  if (!notice_data.has_value() ||
      notice_data->notice_first_shown_ == base::Time() ||
      notice_data->notice_last_shown_ == base::Time()) {
    base::UmaHistogramEnumeration(
        base::StrCat(
            {"PrivacySandbox.Notice.NoticeActionTakenBehavior.", notice}),
        NoticeActionBehavior::kActionBeforeShown);
    return;
  }

  // Overriding an existing notice action is unexpected.
  if (!(notice_data->notice_action_taken_ == NoticeActionTaken::kNotSet)) {
    base::UmaHistogramEnumeration(
        base::StrCat(
            {"PrivacySandbox.Notice.NoticeActionTakenBehavior.", notice}),
        NoticeActionBehavior::kDuplicateActionTaken);
    return;
  }

  // Emitting histograms.
  base::UmaHistogramEnumeration(
      base::StrCat({"PrivacySandbox.Notice.NoticeAction.", notice}),
      notice_action_taken);

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

  std::string notice_action_str = GetNoticeActionString(notice_action_taken);
  // First shown to interacted duration.
  if (!notice_action_str.empty()) {
    // Set first shown to interacted.
    base::TimeDelta first_shown_to_interacted_duration =
        notice_action_taken_time - notice_data->notice_first_shown_;
    update.Get().SetByDottedPath(
        CreatePrefPath(notice, kPrivacySandboxNoticeShownDuration),
        base::TimeDeltaToValue(first_shown_to_interacted_duration));
    CreateTimingHistogram(
        base::StrCat({"PrivacySandbox.Notice.FirstShownToInteractedDuration.",
                      notice, "_", notice_action_str}),
        first_shown_to_interacted_duration);

    // Set last shown to interacted.
    auto last_shown_to_interacted_duration =
        notice_action_taken_time - notice_data->notice_last_shown_;
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
    base::UmaHistogramBoolean(
        base::StrCat({"PrivacySandbox.Notice.NoticeShown.", notice}), true);
    base::UmaHistogramBoolean(
        base::StrCat(
            {"PrivacySandbox.Notice.NoticeShownForFirstTime.", notice}),
        true);
  } else {
    base::UmaHistogramBoolean(
        base::StrCat(
            {"PrivacySandbox.Notice.NoticeShownForFirstTime.", notice}),
        false);
  }

  SetSchemaVersion(pref_service, notice);
  SetChromeVersion(pref_service, notice);
  // Always set notice last shown.
  update.Get().SetByDottedPath(
      CreatePrefPath(notice, kPrivacySandboxNoticeLastShown),
      base::TimeToValue(notice_shown_time));
}

// TODO(chrstne): Create new histograms for migration.
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
  if (input.notice_action_taken_ != NoticeActionTaken::kNotSet &&
      (!existing_notice_data.has_value() ||
       existing_notice_data->notice_action_taken_ ==
           NoticeActionTaken::kNotSet)) {
    update.Get().SetByDottedPath(
        CreatePrefPath(notice, kPrivacySandboxNoticeActionTaken),
        static_cast<int>(input.notice_action_taken_));
    base::UmaHistogramEnumeration(
        base::StrCat({"PrivacySandbox.Notice.NoticeAction.", notice}),
        input.notice_action_taken_);
  }

  if (input.notice_action_taken_time_ != base::Time() &&
      (!existing_notice_data.has_value() ||
       existing_notice_data->notice_action_taken_time_ == base::Time())) {
    update.Get().SetByDottedPath(
        CreatePrefPath(notice, kPrivacySandboxNoticeActionTakenTime),
        base::TimeToValue(input.notice_action_taken_time_));

    // First shown to interacted histogram.
    std::string notice_action_str =
        GetNoticeActionString(input.notice_action_taken_);
    if (!notice_action_str.empty() &&
        input.notice_first_shown_ != base::Time()) {
      base::TimeDelta first_shown_to_interacted_duration =
          input.notice_action_taken_time_ - input.notice_first_shown_;
      update.Get().SetByDottedPath(
          CreatePrefPath(notice, kPrivacySandboxNoticeShownDuration),
          base::TimeDeltaToValue(first_shown_to_interacted_duration));
      CreateTimingHistogram(
          base::StrCat({"PrivacySandbox.Notice.FirstShownToInteractedDuration.",
                        notice, "_", notice_action_str}),
          first_shown_to_interacted_duration);
    }

    // Last shown to interacted histogram.
    if (!notice_action_str.empty() &&
        input.notice_last_shown_ != base::Time()) {
      auto last_shown_to_interacted_duration =
          input.notice_action_taken_time_ - input.notice_last_shown_;
      CreateTimingHistogram(
          base::StrCat({"PrivacySandbox.Notice.LastShownToInteractedDuration.",
                        notice, "_", notice_action_str}),
          last_shown_to_interacted_duration);
    }
  }

  if (input.notice_first_shown_ != base::Time() &&
      (!existing_notice_data.has_value() ||
       existing_notice_data->notice_first_shown_ == base::Time())) {
    update.Get().SetByDottedPath(
        CreatePrefPath(notice, kPrivacySandboxNoticeFirstShown),
        base::TimeToValue(input.notice_first_shown_));
    base::UmaHistogramBoolean(
        base::StrCat({"PrivacySandbox.Notice.NoticeShown.", notice}), true);
  }

  if (input.notice_last_shown_ != base::Time() &&
      (!existing_notice_data.has_value() ||
       existing_notice_data->notice_last_shown_ == base::Time())) {
    update.Get().SetByDottedPath(
        CreatePrefPath(notice, kPrivacySandboxNoticeLastShown),
        base::TimeToValue(input.notice_last_shown_));
  }
}

}  // namespace privacy_sandbox
