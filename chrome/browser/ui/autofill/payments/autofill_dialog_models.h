// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_AUTOFILL_DIALOG_MODELS_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_AUTOFILL_DIALOG_MODELS_H_

#include <string>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "ui/base/models/combobox_model.h"
#include "ui/base/models/simple_combobox_model.h"

namespace autofill {

// A model for possible months in the Gregorian calendar.
class MonthComboboxModel : public ui::ComboboxModel {
 public:
  MonthComboboxModel();
  ~MonthComboboxModel() override;

  // Set |default_index_| to the given |month| before user interaction. There is
  // no effect if the |month| is out of the range.
  void SetDefaultIndexByMonth(int month);

  // ui::Combobox implementation:
  int GetItemCount() const override;
  base::string16 GetItemAt(int index) override;
  int GetDefaultIndex() const override;

 private:
  // The index of the item that is selected by default (before user
  // interaction).
  int default_index_ = 0;

  DISALLOW_COPY_AND_ASSIGN(MonthComboboxModel);
};

// A model for years between now and a decade hence.
class YearComboboxModel : public ui::SimpleComboboxModel {
 public:
  // If |additional_year| is not in the year range initially in this model
  // [current year, current year + 9], this will add |additional_year| to the
  // model. Passing 0 has no effect.
  explicit YearComboboxModel(int additional_year = 0);
  ~YearComboboxModel() override;

  // Set |default_index_| to the given |year| before user interaction. There is
  // no effect if the |year| is out of the range.
  void SetDefaultIndexByYear(int year);

  // ui::Combobox implementation:
  int GetDefaultIndex() const override;

 private:
  // The index of the item that is selected by default (before user
  // interaction).
  int default_index_ = 0;

  DISALLOW_COPY_AND_ASSIGN(YearComboboxModel);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_AUTOFILL_DIALOG_MODELS_H_
