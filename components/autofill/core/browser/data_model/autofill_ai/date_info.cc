// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/autofill_ai/date_info.h"

#include <string>
#include <string_view>

#include "base/i18n/icubridge/date_time_formatter.h"
#include "base/i18n/icubridge/default_icu_locale.h"
#include "base/i18n/icubridge/icu_bridge.h"
#include "base/i18n/language_tag.h"
#include "base/i18n/tag_converters.h"
#include "base/i18n/timezone.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/data_model/data_model_util.h"
#include "components/personal_context/proto/features/common_data.pb.h"

namespace autofill {

DateInfo::DateInfo() = default;

DateInfo::DateInfo(const DateInfo& info) = default;

DateInfo& DateInfo::operator=(const DateInfo& info) = default;

DateInfo::DateInfo(DateInfo&& info) = default;

DateInfo& DateInfo::operator=(DateInfo&& info) = default;

DateInfo::~DateInfo() = default;

void DateInfo::SetDate(std::u16string_view date, std::u16string_view format) {
  data_util::Date d = date_;
  if (!data_util::ParseDate(date, format, d)) {
    d = {};
  }
  date_ = d;
}

std::u16string DateInfo::GetDate(std::u16string_view format) const {
  if (!data_util::IsValidDateFormat(format) ||
      !data_util::IsValidDateForFormat(date_, format)) {
    return {};
  }
  return data_util::FormatDate(date_, format);
}

std::u16string DateInfo::GetIcuDate(std::string_view locale) const {
  if (date_.day == 0 || date_.month == 0 || date_.year == 0) {
    return {};
  }

  base::Time::Exploded exploded = {
      .year = date_.year, .month = date_.month, .day_of_month = date_.day};
  base::Time time;
  if (!base::Time::FromUTCExploded(base::Time::Exploded{exploded}, &time)) {
    return {};
  }
  base::i18n::LanguageTag locale_tag =
      base::i18n::GetLanguageTagFromString(locale).value_or(
          base::i18n::GetDefaultIcuLocale());
  const base::i18n::IcuBridge::DateTimeFormatter& formatter =
      base::i18n::IcuBridge::GetInstance().date_time_formatter(locale_tag);
  base::i18n::DateTimeFormatterOptions options =
      base::i18n::datetime_options::MD::Medium()
          .with_time_zone(base::i18n::TimeZone::GMT())
          .Get();
  return formatter.Format(time, options);
}

personal_context::proto::Date DateInfo::GetDateProto() const {
  personal_context::proto::Date proto_date;
  proto_date.set_year(date_.year);
  proto_date.set_month(date_.month);
  proto_date.set_day(date_.day);
  return proto_date;
}

}  // namespace autofill
