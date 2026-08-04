// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/data_protection/utils.h"

#include "base/feature_list.h"
#include "base/i18n/time_formatting.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "components/enterprise/connectors/core/connectors_prefs.h"
#include "components/enterprise/data_protection/features.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace enterprise_data_protection {

namespace {

base::Time TimestampToTime(safe_browsing::Timestamp timestamp) {
  return base::Time::UnixEpoch() + base::Seconds(timestamp.seconds()) +
         base::Nanoseconds(timestamp.nanos());
}

}  // namespace

UrlSettings::UrlSettings() = default;
UrlSettings::UrlSettings(const UrlSettings&) = default;
UrlSettings& UrlSettings::operator=(const UrlSettings&) = default;
UrlSettings::~UrlSettings() = default;

bool UrlSettings::operator==(const UrlSettings& other) const {
  return watermark_text == other.watermark_text &&
         allow_screenshots == other.allow_screenshots;
}

// static
const UrlSettings& UrlSettings::None() {
  static base::NoDestructor<UrlSettings> empty;
  return *empty.get();
}

UrlSettings GetUrlSettings(
    const std::string& identifier,
    const safe_browsing::RTLookupResponse* rt_lookup_response,
    const std::optional<std::string>& timestamp_timezone) {
  UrlSettings settings;
  if (!rt_lookup_response) {
    return settings;
  }

  for (const auto& threat_info : rt_lookup_response->threat_info()) {
    if (!threat_info.has_matched_url_navigation_rule()) {
      continue;
    }

    const auto& rule = threat_info.matched_url_navigation_rule();
    if (settings.watermark_text.empty()) {
      settings.watermark_text =
          GetWatermarkString(identifier, rule, timestamp_timezone);
    }
    if (settings.allow_screenshots) {
      settings.allow_screenshots = !rule.block_screenshot();
    }
  }

  return settings;
}

std::string FormatWatermarkTimestamp(
    const base::Time& time,
    const std::optional<std::string>& timestamp_timezone) {
  std::unique_ptr<icu::TimeZone> icu_timezone;
  if (timestamp_timezone.has_value() &&
      *timestamp_timezone !=
          enterprise_connectors::kWatermarkStyleTimestampTimezoneDefault) {
    icu_timezone.reset(icu::TimeZone::createTimeZone(
        icu::UnicodeString::fromUTF8(*timestamp_timezone)));
  }

  return base::UnlocalizedTimeFormatWithPattern(
      time, "yyyy-MM-dd'T'HH:mm:ssxxx", icu_timezone.get());
}

std::string GetWatermarkString(
    const std::string& identifier,
    const safe_browsing::MatchedUrlNavigationRule& rule,
    const std::optional<std::string>& timestamp_timezone) {
  if (!rule.has_watermark_message()) {
    return std::string();
  }

  const safe_browsing::MatchedUrlNavigationRule::WatermarkMessage& watermark =
      rule.watermark_message();

  std::string watermark_text = base::StrCat(
      {identifier, "\n",
       base::FeatureList::IsEnabled(kEnableWatermarkTimestampTimezone)
           ? FormatWatermarkTimestamp(TimestampToTime(watermark.timestamp()),
                                      timestamp_timezone)
           : base::TimeFormatAsIso8601(
                 TimestampToTime(watermark.timestamp()))});

  if (!watermark.watermark_message().empty()) {
    watermark_text =
        base::StrCat({watermark.watermark_message(), "\n", watermark_text});
  }

  return watermark_text;
}

}  // namespace enterprise_data_protection
