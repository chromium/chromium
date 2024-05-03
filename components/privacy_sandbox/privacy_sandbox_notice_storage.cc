// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/privacy_sandbox_notice_storage.h"

#include <string>

#include "base/json/values_util.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "components/prefs/pref_registry.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "privacy_sandbox_notice_storage.h"

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

}  // namespace

// PrivacySandboxNoticeData definitions.
PrivacySandboxNoticeData::PrivacySandboxNoticeData() = default;
PrivacySandboxNoticeData::PrivacySandboxNoticeData(
    int schema_version,
    NoticeActionTaken notice_action_taken,
    base::Time notice_action_taken_time,
    base::Time notice_first_shown,
    base::Time notice_last_shown,
    base::TimeDelta notice_shown_duration)
    : schema_version(schema_version),
      notice_action_taken(notice_action_taken),
      notice_action_taken_time(notice_action_taken_time),
      notice_first_shown(notice_first_shown),
      notice_last_shown(notice_last_shown),
      notice_shown_duration(notice_shown_duration) {}
PrivacySandboxNoticeData& PrivacySandboxNoticeData::operator=(
    const PrivacySandboxNoticeData&) = default;
PrivacySandboxNoticeData::~PrivacySandboxNoticeData() = default;

// PrivacySandboxNoticeStorage definitions.
void PrivacySandboxNoticeStorage::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kPrivacySandboxNoticeDataPath);
}

std::optional<PrivacySandboxNoticeData>
PrivacySandboxNoticeStorage::ReadNoticeData(PrefService* pref_service,
                                            std::string_view notice) {
  const base::Value::Dict& pref_data =
      pref_service->GetDict(kPrivacySandboxNoticeDataPath);
  if (pref_data.empty()) {
    return std::nullopt;
  }

  // Populate notice data values.
  std::optional<PrivacySandboxNoticeData> notice_data =
      PrivacySandboxNoticeData();
  notice_data->schema_version = *pref_data.FindIntByDottedPath(
      CreatePrefPath(notice, kPrivacySandboxSchemaVersion));
  notice_data->notice_action_taken_time =
      *base::ValueToTime(pref_data.FindByDottedPath(
          CreatePrefPath(notice, kPrivacySandboxNoticeActionTakenTime)));
  notice_data->notice_first_shown =
      *base::ValueToTime(pref_data.FindByDottedPath(
          CreatePrefPath(notice, kPrivacySandboxNoticeFirstShown)));
  notice_data->notice_last_shown =
      *base::ValueToTime(pref_data.FindByDottedPath(
          CreatePrefPath(notice, kPrivacySandboxNoticeLastShown)));
  notice_data->notice_shown_duration =
      *base::ValueToTimeDelta(pref_data.FindByDottedPath(
          CreatePrefPath(notice, kPrivacySandboxNoticeShownDuration)));

  // Enum handling.
  std::optional<int> notice_action_taken = pref_data.FindIntByDottedPath(
      CreatePrefPath(notice, kPrivacySandboxNoticeActionTaken));
  if (!notice_action_taken || *notice_action_taken < 0 ||
      *notice_action_taken > static_cast<int>(NoticeActionTaken::kMaxValue)) {
    notice_data->notice_action_taken = NoticeActionTaken::kNotSet;
  } else {
    notice_data->notice_action_taken =
        static_cast<NoticeActionTaken>(*notice_action_taken);
  }

  return notice_data;
}

void PrivacySandboxNoticeStorage::SetSchemaVersion(PrefService* pref_service,
                                                   std::string_view notice,
                                                   int schema_version) {
  ScopedDictPrefUpdate update(pref_service, kPrivacySandboxNoticeDataPath);
  update.Get().SetByDottedPath(
      CreatePrefPath(notice, kPrivacySandboxSchemaVersion), schema_version);
}

void PrivacySandboxNoticeStorage::SetNoticeActionTaken(
    PrefService* pref_service,
    std::string_view notice,
    NoticeActionTaken notice_action_taken,
    base::Time notice_action_taken_time) {
  ScopedDictPrefUpdate update(pref_service, kPrivacySandboxNoticeDataPath);
  update.Get().SetByDottedPath(
      CreatePrefPath(notice, kPrivacySandboxNoticeActionTaken),
      static_cast<int>(notice_action_taken));
  update.Get().SetByDottedPath(
      CreatePrefPath(notice, kPrivacySandboxNoticeActionTakenTime),
      base::TimeToValue(notice_action_taken_time));
}

void PrivacySandboxNoticeStorage::SetNoticeShown(PrefService* pref_service,
                                                 std::string_view notice,
                                                 base::Time notice_shown_time) {
  ScopedDictPrefUpdate update(pref_service, kPrivacySandboxNoticeDataPath);
  const base::Value::Dict& pref_data =
      pref_service->GetDict(kPrivacySandboxNoticeDataPath);
  // Only set notice first shown if it hasn't previously been set.
  if (!pref_data.empty() && !pref_data.FindByDottedPath(CreatePrefPath(
                                notice, kPrivacySandboxNoticeFirstShown))) {
    update.Get().SetByDottedPath(
        CreatePrefPath(notice, kPrivacySandboxNoticeFirstShown),
        base::TimeToValue(notice_shown_time));
  }

  // Always set notice last shown.
  update.Get().SetByDottedPath(
      CreatePrefPath(notice, kPrivacySandboxNoticeLastShown),
      base::TimeToValue(notice_shown_time));
}

void PrivacySandboxNoticeStorage::SetNoticeShownDuration(
    PrefService* pref_service,
    std::string_view notice,
    base::TimeDelta notice_shown_duration) {
  ScopedDictPrefUpdate update(pref_service, kPrivacySandboxNoticeDataPath);
  update.Get().SetByDottedPath(
      CreatePrefPath(notice, kPrivacySandboxNoticeShownDuration),
      base::TimeDeltaToValue(notice_shown_duration));
}

}  // namespace privacy_sandbox
