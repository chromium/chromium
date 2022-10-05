// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/notifications_engagement_service.h"

#include "components/permissions/permissions_client.h"
#include "url/gurl.h"

namespace permissions {

namespace {

// For each origin that has the  |ContentSettingsType::NOTIFICATIONS|
// permission, we record the number of notifications that were displayed
// and interacted with. The data is stored in the website setting
// |NOTIFICATION_INTERACTIONS|  keyed to the same origin. The internal
// structure of this metadata is a dictionary:
//
//   {"1644163200": {"display_count": 3},  # Implied click_count = 0.
//    "1644768000": {"display_count": 6, "click_count": 1}}
//
// Where the value stored for  date_i  summarizes notification activity
// for the time period between  date_i  and  date_i+1  (or today for the
// last entry). Currently, the dates will be space one week apart, and
// correspond to Monday midnights.

constexpr char kEngagementKey[] = "click_count";
constexpr char kDisplayedKey[] = "display_count";

// Entries in notifications engagement expire after they become this old.
constexpr base::TimeDelta kMaxAge = base::Days(90);

// Discards notification interactions stored in `engagement` for time
// periods older than |kMaxAge|.
void EraseStaleEntries(base::Value::Dict& engagement) {
  const base::Time cutoff = base::Time::Now() - kMaxAge;

  for (auto it = engagement.begin(); it != engagement.end();) {
    const auto& [key, value] = *it;

    absl::optional<base::Time> last_time =
        NotificationsEngagementService::ParsePeriodBeginFromBucketLabel(key);
    if (!last_time.has_value() || last_time.value() < cutoff) {
      it = engagement.erase(it);
      continue;
    }
    ++it;
  }
}
}  // namespace

NotificationsEngagementService::NotificationsEngagementService(
    content::BrowserContext* context,
    PrefService* pref_service)
    : pref_service_(pref_service) {
  settings_map_ =
      permissions::PermissionsClient::Get()->GetSettingsMap(context);
}

void NotificationsEngagementService::Shutdown() {
  settings_map_ = nullptr;
}

void NotificationsEngagementService::RecordNotificationDisplayed(
    const GURL& url) {
  IncrementCounts(url, 1 /*display_count_delta*/, 0 /*click_count_delta*/);
}

void NotificationsEngagementService::RecordNotificationDisplayed(
    const GURL& url,
    int display_count) {
  IncrementCounts(url, display_count, 0 /*click_count_delta*/);
}

void NotificationsEngagementService::RecordNotificationInteraction(
    const GURL& url) {
  IncrementCounts(url, 0 /*display_count_delta*/, 1 /*click_count_delta*/);
}

void NotificationsEngagementService::IncrementCounts(const GURL& url,
                                                     int display_count_delta,
                                                     int click_count_delta) {
  base::Value engagement_as_value = settings_map_->GetWebsiteSetting(
      url, GURL(), ContentSettingsType::NOTIFICATION_INTERACTIONS, nullptr);

  base::Value::Dict engagement;
  if (engagement_as_value.is_dict())
    engagement = std::move(engagement_as_value).TakeDict();

  std::string date = GetBucketLabelForLastMonday(base::Time::Now());
  if (date == std::string())
    return;

  EraseStaleEntries(engagement);
  base::Value::Dict* bucket = engagement.FindDict(date);
  if (!bucket) {
    bucket = &engagement.Set(date, base::Value::Dict())->GetDict();
  }
  if (display_count_delta) {
    bucket->Set(kDisplayedKey, display_count_delta +
                                   bucket->FindInt(kDisplayedKey).value_or(0));
  }
  if (click_count_delta) {
    bucket->Set(
        kEngagementKey,
        click_count_delta + bucket->FindInt(kEngagementKey).value_or(0));
  }

  // Set the website setting of this origin with the updated |engagement|.
  settings_map_->SetWebsiteSettingDefaultScope(
      url, GURL(), ContentSettingsType::NOTIFICATION_INTERACTIONS,
      base::Value(std::move(engagement)));
}

// static
std::string NotificationsEngagementService::GetBucketLabelForLastMonday(
    base::Time date) {
  // For human-readability, return the UTC Monday midnight on the same date as
  // local midnight.
  base::Time::Exploded date_exploded;
  date.LocalExplode(&date_exploded);
  base::Time local_monday =
      (date - base::Days((date_exploded.day_of_week + 6) % 7)).LocalMidnight();

  base::Time::Exploded local_monday_exploded;
  local_monday.LocalExplode(&local_monday_exploded);
  // Intentionally converting a locally exploded time, to an UTC time, so that
  // the Monday Midnight in UTC is on the same date the last Monday on local
  // time.
  base::Time last_monday;
  bool converted =
      base::Time::FromUTCExploded(local_monday_exploded, &last_monday);

  if (converted)
    return base::NumberToString(last_monday.base::Time::ToTimeT());

  return std::string();
}

// static
absl::optional<base::Time>
NotificationsEngagementService::ParsePeriodBeginFromBucketLabel(
    const std::string& label) {
  int maybe_engagement_time;
  base::Time local_period_begin;

  // Store the time as local time.
  if (base::StringToInt(label.c_str(), &maybe_engagement_time)) {
    base::Time::Exploded date_exploded;
    base::Time::FromTimeT(maybe_engagement_time).UTCExplode(&date_exploded);
    if (base::Time::FromLocalExploded(date_exploded, &local_period_begin))
      return local_period_begin;
  }

  return absl::nullopt;
}

}  // namespace permissions
