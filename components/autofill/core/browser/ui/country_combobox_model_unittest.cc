// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/country_combobox_model.h"

#include <memory>

#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_ui.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_ui_component.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/localization.h"

namespace autofill {

class CountryComboboxModelTest : public testing::Test {
 public:
  CountryComboboxModelTest() {
    model_ = std::make_unique<CountryComboboxModel>();
    model_->SetCountries(GeoIpCountryCode("DE"), "en-US");
  }

  CountryComboboxModel* model() { return model_.get(); }

 private:
  std::unique_ptr<CountryComboboxModel> model_;
};

TEST_F(CountryComboboxModelTest, DefaultCountryCode) {
  std::string default_country = model()->GetDefaultCountryCode();
  EXPECT_EQ("DE", default_country);

  AutofillCountry country(default_country, "en-US");
  EXPECT_EQ(country.name(), model()->GetItemAt(0));
}

TEST_F(CountryComboboxModelTest, AllCountriesHaveComponents) {
  ::i18n::addressinput::Localization localization;
  std::string unused;
  for (size_t i = 0; i < model()->GetItemCount(); ++i) {
    if (model()->IsItemSeparatorAt(i))
      continue;

    std::string country_code = model()->countries()[i]->country_code();
    std::vector<::i18n::addressinput::AddressUiComponent> components =
        ::i18n::addressinput::BuildComponents(country_code, localization,
                                              std::string(), &unused);
    EXPECT_FALSE(components.empty()) << " for country " << country_code;
  }
}

}  // namespace autofill
