// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/site_engagement/content/site_engagement_score.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "base/metrics/field_trial_params.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/site_engagement/content/engagement_type.h"
#include "components/site_engagement/content/site_engagement_metrics.h"
#include "third_party/blink/public/mojom/site_engagement/site_engagement.mojom.h"

namespace site_engagement {

namespace {

// Delta within which to consider scores equal.
const double kScoreDelta = 0.001;

// Delta within which to consider internal time values equal. Internal time
// values are in microseconds, so this delta comes out at one second.
const double kTimeDelta = 1000000;

// Number of days after the last launch of an origin from an installed shortcut
// for which WEB_APP_INSTALLED_POINTS will be added to the engagement score.
const int kMaxDaysSinceShortcutLaunch = 10;

bool DoublesConsideredDifferent(double value1, double value2, double delta) {
  double abs_difference = fabs(value1 - value2);
  return abs_difference > delta;
}

base::Value::Dict GetSiteEngagementScoreDictForSettings(
    const HostContentSettingsMap* settings,
    const GURL& origin_url) {
  if (!settings)
    return base::Value::Dict();

  base::Value value = settings->GetWebsiteSetting(
      origin_url, origin_url, ContentSettingsType::SITE_ENGAGEMENT, nullptr);
  if (!value.is_dict())
    return base::Value::Dict();

  return std::move(value).TakeDict();
}

}  // namespace

const double SiteEngagementScore::kMaxPoints = 100;

const char SiteEngagementScore::kRawScoreKey[] = "rawScore";
const char SiteEngagementScore::kPointsAddedTodayKey[] = "pointsAddedToday";
const char SiteEngagementScore::kLastEngagementTimeKey[] = "lastEngagementTime";
const char SiteEngagementScore::kLastShortcutLaunchTimeKey[] =
    "lastShortcutLaunchTime";

// static
SiteEngagementScore::ParamValues& SiteEngagementScore::GetParamValues() {
  static base::NoDestructor<ParamValues> param_values([]() {
    SiteEngagementScore::ParamValues param_values;
    param_values[MAX_POINTS_PER_DAY] = {"max_points_per_day", 15};
    param_values[DECAY_PERIOD_IN_HOURS] = {"decay_period_in_hours", 2};
    param_values[DECAY_POINTS] = {"decay_points", 0};
    param_values[DECAY_PROPORTION] = {"decay_proportion", 0.984};
    param_values[SCORE_CLEANUP_THRESHOLD] = {"score_cleanup_threshold", 0.5};
    param_values[NAVIGATION_POINTS] = {"navigation_points", 1.5};
    param_values[USER_INPUT_POINTS] = {"user_input_points", 0.6};
    param_values[VISIBLE_MEDIA_POINTS] = {"visible_media_playing_points", 0.06};
    param_values[HIDDEN_MEDIA_POINTS] = {"hidden_media_playing_points", 0.01};
    param_values[WEB_APP_INSTALLED_POINTS] = {"web_app_installed_points", 5};
    param_values[FIRST_DAILY_ENGAGEMENT] = {"first_daily_engagement_points",
                                            1.5};
    param_values[BOOTSTRAP_POINTS] = {"bootstrap_points", 24};
    param_values[MEDIUM_ENGAGEMENT_BOUNDARY] = {"medium_engagement_boundary",
                                                15};
    param_values[HIGH_ENGAGEMENT_BOUNDARY] = {"high_engagement_boundary", 50};
    param_values[MAX_DECAYS_PER_SCORE] = {"max_decays_per_score", 4};
    param_values[LAST_ENGAGEMENT_GRACE_PERIOD_IN_HOURS] = {
        "last_engagement_grace_period_in_hours", 1};
    param_values[NOTIFICATION_INTERACTION_POINTS] = {
        "notification_interaction_points", 1};
    return param_values;
  }());
  return *param_values;
}

double SiteEngagementScore::GetMaxPointsPerDay() {
  return GetParamValues()[MAX_POINTS_PER_DAY].second;
}

double SiteEngagementScore::GetDecayPeriodInHours() {
  return GetParamValues()[DECAY_PERIOD_IN_HOURS].second;
}

double SiteEngagementScore::GetDecayPoints() {
  return GetParamValues()[DECAY_POINTS].second;
}

double SiteEngagementScore::GetDecayProportion() {
  return GetParamValues()[DECAY_PROPORTION].second;
}

double SiteEngagementScore::GetScoreCleanupThreshold() {
  return GetParamValues()[SCORE_CLEANUP_THRESHOLD].second;
}

double SiteEngagementScore::GetNavigationPoints() {
  return GetParamValues()[NAVIGATION_POINTS].second;
}

double SiteEngagementScore::GetUserInputPoints() {
  return GetParamValues()[USER_INPUT_POINTS].second;
}

double SiteEngagementScore::GetVisibleMediaPoints() {
  return GetParamValues()[VISIBLE_MEDIA_POINTS].second;
}

double SiteEngagementScore::GetHiddenMediaPoints() {
  return GetParamValues()[HIDDEN_MEDIA_POINTS].second;
}

double SiteEngagementScore::GetWebAppInstalledPoints() {
  return GetParamValues()[WEB_APP_INSTALLED_POINTS].second;
}

double SiteEngagementScore::GetFirstDailyEngagementPoints() {
  return GetParamValues()[FIRST_DAILY_ENGAGEMENT].second;
}

double SiteEngagementScore::GetBootstrapPoints() {
  return GetParamValues()[BOOTSTRAP_POINTS].second;
}

double SiteEngagementScore::GetMediumEngagementBoundary() {
  return GetParamValues()[MEDIUM_ENGAGEMENT_BOUNDARY].second;
}

double SiteEngagementScore::GetHighEngagementBoundary() {
  return GetParamValues()[HIGH_ENGAGEMENT_BOUNDARY].second;
}

double SiteEngagementScore::GetMaxDecaysPerScore() {
  return GetParamValues()[MAX_DECAYS_PER_SCORE].second;
}

double SiteEngagementScore::GetLastEngagementGracePeriodInHours() {
  return GetParamValues()[LAST_ENGAGEMENT_GRACE_PERIOD_IN_HOURS].second;
}

double SiteEngagementScore::GetNotificationInteractionPoints() {
  return GetParamValues()[NOTIFICATION_INTERACTION_POINTS].second;
}

void SiteEngagementScore::SetParamValuesForTesting() {
  GetParamValues()[MAX_POINTS_PER_DAY].second = 5;
  GetParamValues()[DECAY_PERIOD_IN_HOURS].second = 7 * 24;
  GetParamValues()[DECAY_POINTS].second = 5;
  GetParamValues()[NAVIGATION_POINTS].second = 0.5;
  GetParamValues()[USER_INPUT_POINTS].second = 0.05;
  GetParamValues()[VISIBLE_MEDIA_POINTS].second = 0.02;
  GetParamValues()[HIDDEN_MEDIA_POINTS].second = 0.01;
  GetParamValues()[WEB_APP_INSTALLED_POINTS].second = 5;
  GetParamValues()[BOOTSTRAP_POINTS].second = 8;
  GetParamValues()[MEDIUM_ENGAGEMENT_BOUNDARY].second = 5;
  GetParamValues()[HIGH_ENGAGEMENT_BOUNDARY].second = 50;
  GetParamValues()[MAX_DECAYS_PER_SCORE].second = 1;
  GetParamValues()[LAST_ENGAGEMENT_GRACE_PERIOD_IN_HOURS].second = 72;
  GetParamValues()[NOTIFICATION_INTERACTION_POINTS].second = 1;

  // This is set to values that avoid interference with tests and are set when
  // testing these features.
  GetParamValues()[FIRST_DAILY_ENGAGEMENT].second = 0;
  GetParamValues()[DECAY_PROPORTION].second = 1;
  GetParamValues()[SCORE_CLEANUP_THRESHOLD].second = 0;
}
// static
void SiteEngagementScore::UpdateFromVariations(const char* param_name) {
  double param_vals[MAX_VARIATION];

  for (int i = 0; i < MAX_VARIATION; ++i) {
    std::string param_string =
        base::GetFieldTrialParamValue(param_name, GetParamValues()[i].first);

    // Bail out if we didn't get a param string for the key, or if we couldn't
    // convert the param string to a double, or if we get a negative value.
    if (param_string.empty() ||
        !base::StringToDouble(param_string, &param_vals[i]) ||
        param_vals[i] < 0) {
      return;
    }
  }

  // Once we're sure everything is valid, assign the variation to the param
  // values array.
  for (int i = 0; i < MAX_VARIATION; ++i)
    SiteEngagementScore::GetParamValues()[i].second = param_vals[i];
}

SiteEngagementScore::SiteEngagementScore(base::Clock* clock,
                                         const GURL& origin,
                                         HostContentSettingsMap* settings)
    : SiteEngagementScore(
          clock,
          origin,
          GetSiteEngagementScoreDictForSettings(settings, origin)) {
  settings_map_ = settings;
}

SiteEngagementScore::SiteEngagementScore(SiteEngagementScore&& other) = default;

SiteEngagementScore::~SiteEngagementScore() = default;

SiteEngagementScore& SiteEngagementScore::operator=(
    SiteEngagementScore&& other) = default;

void SiteEngagementScore::AddPoints(double points) {
  DCHECK_NE(0, points);

  // As the score is about to be updated, commit any decay that has happened
  // since the last update.
  raw_score_ = DecayedScore();

  base::Time now = clock_->Now();
  if (!last_engagement_time_.is_null() &&
      now.LocalMidnight() != last_engagement_time_.LocalMidnight()) {
    points_added_today_ = 0;
  }

  if (points_added_today_ == 0) {
    // Award bonus engagement for the first engagement of the day for a site.
    points += GetFirstDailyEngagementPoints();
    SiteEngagementMetrics::RecordEngagement(
        EngagementType::kFirstDailyEngagement);
  }

  double to_add = std::min(kMaxPoints - raw_score_,
                           GetMaxPointsPerDay() - points_added_today_);
  to_add = std::min(to_add, points);

  points_added_today_ += to_add;
  raw_score_ += to_add;

  last_engagement_time_ = now;
}

double SiteEngagementScore::GetTotalScore() const {
  return std::min(DecayedScore() + BonusIfShortcutLaunched(), kMaxPoints);
}

mojom::SiteEngagementDetails SiteEngagementScore::GetDetails() const {
  mojom::SiteEngagementDetails engagement;
  engagement.origin = origin_;
  engagement.base_score = DecayedScore();
  engagement.installed_bonus = BonusIfShortcutLaunched();
  engagement.total_score = GetTotalScore();
  return engagement;
}

void SiteEngagementScore::Commit() {
  DCHECK(settings_map_);
  DCHECK(score_dict_);
  if (!UpdateScoreDict(*score_dict_))
    return;

  settings_map_->SetWebsiteSettingDefaultScope(
      origin_, GURL(), ContentSettingsType::SITE_ENGAGEMENT,
      base::Value(std::move(*score_dict_)));
}

blink::mojom::EngagementLevel SiteEngagementScore::GetEngagementLevel() const {
  DCHECK_LT(GetMediumEngagementBoundary(), GetHighEngagementBoundary());

  double score = GetTotalScore();
  if (score == 0)
    return blink::mojom::EngagementLevel::NONE;

  if (score < 1)
    return blink::mojom::EngagementLevel::MINIMAL;

  if (score < GetMediumEngagementBoundary())
    return blink::mojom::EngagementLevel::LOW;

  if (score < GetHighEngagementBoundary())
    return blink::mojom::EngagementLevel::MEDIUM;

  if (score < SiteEngagementScore::kMaxPoints)
    return blink::mojom::EngagementLevel::HIGH;

  return blink::mojom::EngagementLevel::MAX;
}

bool SiteEngagementScore::MaxPointsPerDayAdded() const {
  if (!last_engagement_time_.is_null() &&
      clock_->Now().LocalMidnight() != last_engagement_time_.LocalMidnight()) {
    return false;
  }

  return points_added_today_ == GetMaxPointsPerDay();
}

void SiteEngagementScore::Reset(double points,
                                const base::Time last_engagement_time) {
  raw_score_ = points;
  points_added_today_ = 0;

  // This must be set in order to prevent the score from decaying when read.
  last_engagement_time_ = last_engagement_time;
}

void SiteEngagementScore::SetLastEngagementTime(const base::Time& time) {
  if (!last_engagement_time_.is_null() &&
      time.LocalMidnight() != last_engagement_time_.LocalMidnight()) {
    points_added_today_ = 0;
  }
  last_engagement_time_ = time;
}

bool SiteEngagementScore::UpdateScoreDict(base::Value::Dict& score_dict) {
  double raw_score_orig = score_dict.FindDouble(kRawScoreKey).value_or(0);
  double points_added_today_orig =
      score_dict.FindDouble(kPointsAddedTodayKey).value_or(0);
  double last_engagement_time_internal_orig =
      score_dict.FindDouble(kLastEngagementTimeKey).value_or(0);
  double last_shortcut_launch_time_internal_orig =
      score_dict.FindDouble(kLastShortcutLaunchTimeKey).value_or(0);

  bool changed =
      DoublesConsideredDifferent(raw_score_orig, raw_score_, kScoreDelta) ||
      DoublesConsideredDifferent(points_added_today_orig, points_added_today_,
                                 kScoreDelta) ||
      DoublesConsideredDifferent(last_engagement_time_internal_orig,
                                 last_engagement_time_.ToInternalValue(),
                                 kTimeDelta) ||
      DoublesConsideredDifferent(last_shortcut_launch_time_internal_orig,
                                 last_shortcut_launch_time_.ToInternalValue(),
                                 kTimeDelta);

  if (!changed)
    return false;

  score_dict.Set(kRawScoreKey, raw_score_);
  score_dict.Set(kPointsAddedTodayKey, points_added_today_);
  score_dict.Set(kLastEngagementTimeKey,
                 static_cast<double>(last_engagement_time_.ToInternalValue()));
  score_dict.Set(
      kLastShortcutLaunchTimeKey,
      static_cast<double>(last_shortcut_launch_time_.ToInternalValue()));

  return true;
}

SiteEngagementScore::SiteEngagementScore(
    base::Clock* clock,
    const GURL& origin,
    std::optional<base::Value::Dict> score_dict)
    : clock_(clock),
      raw_score_(0),
      points_added_today_(0),
      last_engagement_time_(),
      last_shortcut_launch_time_(),
      score_dict_(std::move(score_dict)),
      origin_(origin),
      settings_map_(nullptr) {
  if (!score_dict_)
    return;

  raw_score_ = score_dict_->FindDouble(kRawScoreKey).value_or(0);
  points_added_today_ =
      score_dict_->FindDouble(kPointsAddedTodayKey).value_or(0);

  std::optional<double> maybe_last_engagement_time =
      score_dict_->FindDouble(kLastEngagementTimeKey);
  if (maybe_last_engagement_time.has_value())
    last_engagement_time_ =
        base::Time::FromInternalValue(maybe_last_engagement_time.value());

  std::optional<double> maybe_last_shortcut_launch_time =
      score_dict_->FindDouble(kLastShortcutLaunchTimeKey);
  if (maybe_last_shortcut_launch_time.has_value())
    last_shortcut_launch_time_ =
        base::Time::FromInternalValue(maybe_last_shortcut_launch_time.value());
}

double SiteEngagementScore::DecayedScore() const {
  // Note that users can change their clock, so from this system's perspective
  // time can go backwards. If that does happen and the system detects that the
  // current day is earlier than the last engagement, no decay (or growth) is
  // applied.
  int hours_since_engagement =
      (clock_->Now() - last_engagement_time_).InHours();
  if (hours_since_engagement < 0)
    return raw_score_;

  int periods = hours_since_engagement / GetDecayPeriodInHours();
  return std::max(0.0, raw_score_ * pow(GetDecayProportion(), periods) -
                           periods * GetDecayPoints());
}

double SiteEngagementScore::BonusIfShortcutLaunched() const {
  int days_since_shortcut_launch =
      (clock_->Now() - last_shortcut_launch_time_).InDays();
  if (days_since_shortcut_launch <= kMaxDaysSinceShortcutLaunch)
    return GetWebAppInstalledPoints();
  return 0;
}

}  // namespace site_engagement
