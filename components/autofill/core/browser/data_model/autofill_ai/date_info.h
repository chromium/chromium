// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_AI_DATE_INFO_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_AI_DATE_INFO_H_

#include <string>
#include <string_view>
#include <utility>

#include "components/autofill/core/browser/data_model/data_model_utils.h"

namespace autofill {

// Stores a year, month, day tuple.
class DateInfo {
 public:
  DateInfo();
  DateInfo(const DateInfo& info);
  DateInfo& operator=(const DateInfo& info);
  DateInfo(DateInfo&& info);
  DateInfo& operator=(DateInfo&& info);
  ~DateInfo();

  // Changes the date. Incremental formats only lead to incremental changes:
  //   SetDate(Date{.day = 16}, u"DD");
  //   SetDate(Date{.day = 12}, u"MM");
  //   SetDate(Date{.day = 2022}, u"YYYY");
  // leads to
  //   GetDate(u"DD/MM/YYYY") == u"16/12/2022"
  void SetDate(std::u16string_view date, std::u16string_view format);

  // For the grammar of format strings, see `data_util::IsValidDateFormat()`.
  std::u16string GetDate(std::u16string_view format) const;

  // Returns the date formatted according to `format` and `locale` using ICU.
  // Returns an empty string if the date is partially set (e.g. day is not set)
  // or in case of an error. For the grammar of format strings, see
  // https://unicode.org/reports/tr35/tr35-dates.html#Date_Format_Patterns.
  std::u16string GetIcuDate(std::u16string_view format,
                            std::string_view locale) const;

  friend bool operator==(const DateInfo&, const DateInfo&) = default;

 private:
  data_util::Date date_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_AI_DATE_INFO_H_
