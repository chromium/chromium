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

  void SetDate(data_util::Date date) { date_ = std::move(date); }
  const data_util::Date& GetDate() const { return date_; }

  std::u16string GetDate(std::u16string_view format) const;

  friend bool operator==(const DateInfo&, const DateInfo&) = default;

 private:
  data_util::Date date_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_AI_DATE_INFO_H_
