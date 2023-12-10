// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_AUTOFILL_DIALOG_MODELS_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_AUTOFILL_DIALOG_MODELS_H_

#include <string>

#include "ui/base/models/combobox_model.h"
#include "ui/base/models/simple_combobox_model.h"

namespace autofill {

// A model for possible months in the Gregorian calendar.
class MonthComboboxModel : public ui::ComboboxModel {
 public:
  MonthComboboxModel();
  MonthComboboxModel(const MonthComboboxModel&) = delete;
  MonthComboboxModel& operator=(const MonthComboboxModel&) = delete;
  ~MonthComboboxModel() override;

  // Set |default_index_| to the given |month| before user interaction. There is
  // no effect if the |month| is out of the range.
  void SetDefaultIndexByMonth(int month);

  // ui::Combobox implementation:
  size_t GetItemCount() const override;
  std::u16string GetItemAt(size_t index) const override;
  std::optional<size_t> GetDefaultIndex() const override;

 private:
  // The index of the item that is selected by default (before user
  // interaction).
  size_t default_index_ = 0;
};

// A model for years between now and a decade hence.
class YearComboboxModel : public ui::SimpleComboboxModel {
 public:
  // If |additional_year| is not in the year range initially in this model
  // [current year, current year + 9], this will add |additional_year| to the
  // model. Passing 0 has no effect.
  explicit YearComboboxModel(int additional_year = 0);
  YearComboboxModel(const YearComboboxModel&) = delete;
  YearComboboxModel& operator=(const YearComboboxModel&) = delete;
  ~YearComboboxModel() override;

  // Set |default_index_| to the given |year| before user interaction. There is
  // no effect if the |year| is out of the range.
  void SetDefaultIndexByYear(int year);

  // ui::Combobox implementation:
  std::optional<size_t> GetDefaultIndex() const override;

 private:
  // The index of the item that is selected by default (before user
  // interaction).
  size_t default_index_ = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_AUTOFILL_DIALOG_MODELS_H_
