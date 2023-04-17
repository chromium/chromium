// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_COUNTRY_COMBOBOX_MODEL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_COUNTRY_COMBOBOX_MODEL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "ui/base/models/combobox_model.h"

namespace autofill {

class AutofillCountry;
class PersonalDataManager;

// A model for countries to be used to enter addresses.
class CountryComboboxModel : public ui::ComboboxModel {
 public:
  using CountryVector = std::vector<std::unique_ptr<AutofillCountry>>;

  CountryComboboxModel();

  CountryComboboxModel(const CountryComboboxModel&) = delete;
  CountryComboboxModel& operator=(const CountryComboboxModel&) = delete;

  ~CountryComboboxModel() override;

  // |filter| is passed each known country's country code. If |filter| returns
  // true, an item for that country is added to the model (else it's omitted).
  // Empty callback can be used to retain all countries.
  // |manager| determines the default choice.
  void SetCountries(
      const PersonalDataManager& manager,
      const base::RepeatingCallback<bool(const std::string&)>& filter,
      const std::string& app_locale);

  // ui::ComboboxModel implementation:
  size_t GetItemCount() const override;
  std::u16string GetItemAt(size_t index) const override;
  bool IsItemSeparatorAt(size_t index) const override;

  // The list of countries always has the default country at the top as well as
  // within the sorted vector.
  const CountryVector& countries() const { return countries_; }

  // Returns the default country code for this model.
  std::string GetDefaultCountryCode() const;

 private:
  // The countries to show in the model, including NULL for entries that are
  // not countries (the separator entry).
  CountryVector countries_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_COUNTRY_COMBOBOX_MODEL_H_
