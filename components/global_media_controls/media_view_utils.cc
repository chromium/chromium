// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/global_media_controls/media_view_utils.h"

#include <algorithm>
#include <string>

#include "base/i18n/number_formatting.h"
#include "base/i18n/time_formatting.h"
#include "base/i18n/unicodestring.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "third_party/icu/source/i18n/unicode/measfmt.h"
#include "third_party/icu/source/i18n/unicode/measunit.h"
#include "third_party/icu/source/i18n/unicode/measure.h"

namespace global_media_controls {

namespace {

// Returns a singleton `icu::MeasureFormat` object, so that it doesn't have to
// be recreated each time when formatting a time duration.
const icu::MeasureFormat& GetMeasureFormat() {
  static const base::NoDestructor<icu::MeasureFormat> measure_format([] {
    UErrorCode status = U_ZERO_ERROR;
    icu::MeasureFormat measure_format(
        icu::Locale::getDefault(), UMeasureFormatWidth::UMEASFMT_WIDTH_NUMERIC,
        status);
    CHECK(U_SUCCESS(status));
    return measure_format;
  }());
  return *measure_format;
}

}  // namespace

gfx::Size ScaleImageSizeToFitView(const gfx::Size& image_size,
                                  const gfx::Size& view_size) {
  const float scale =
      std::max(view_size.width() / static_cast<float>(image_size.width()),
               view_size.height() / static_cast<float>(image_size.height()));
  return gfx::ScaleToFlooredSize(image_size, scale);
}

std::u16string GetFormattedDuration(const base::TimeDelta& duration) {
  UErrorCode status = U_ZERO_ERROR;

  std::vector<icu::Measure> measures;

  const int64_t total_seconds =
      base::ClampRound<int64_t>(duration.InSecondsF());
  const int64_t hours = total_seconds / base::Time::kSecondsPerHour;
  if (hours != 0) {
    measures.emplace_back(hours, icu::MeasureUnit::createHour(status), status);
  }

  const int64_t minutes =
      (total_seconds - hours * base::Time::kSecondsPerHour) /
      base::Time::kSecondsPerMinute;
  measures.emplace_back(minutes, icu::MeasureUnit::createMinute(status),
                        status);

  const int64_t seconds = total_seconds % base::Time::kSecondsPerMinute;
  measures.emplace_back(seconds, icu::MeasureUnit::createSecond(status),
                        status);

  icu::UnicodeString formatted;
  icu::FieldPosition ignore(icu::FieldPosition::DONT_CARE);
  GetMeasureFormat().formatMeasures(&measures[0], measures.size(), formatted,
                                    ignore, status);

  if (U_SUCCESS(status)) {
    return base::i18n::UnicodeStringToString16(formatted);
  }

  // If any step has error, returns the duration in seconds.
  return base::NumberToString16(std::ceil(duration.InSecondsF()));
}

}  // namespace global_media_controls
