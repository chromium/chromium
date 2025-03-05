// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/autofill_ai/date_info.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_structured_address_component.h"
#include "components/autofill/core/browser/data_model/data_model_utils.h"
#include "components/autofill/core/browser/data_quality/autofill_data_util.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/geo/country_names.h"

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
  if (!data_util::IsValidDateForFormat(date_, format)) {
    return {};
  }
  return data_util::FormatDate(date_, format);
}

}  // namespace autofill
