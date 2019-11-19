// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/country_combobox_model.h"

#include <memory>

#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/prefs/pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_ui.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_ui_component.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/localization.h"

namespace autofill {

class CountryComboboxModelTest : public testing::Test {
 public:
  CountryComboboxModelTest()
      : pref_service_(autofill::test::PrefServiceForTesting()) {
    manager_.SetPrefService(pref_service_.get());
    manager_.set_timezone_country_code("KR");
    model_ = std::make_unique<CountryComboboxModel>();
    model_->SetCountries(
        manager_, base::RepeatingCallback<bool(const std::string&)>(), "en-US");
  }

  void TearDown() override { manager_.SetPrefService(nullptr); }

  TestPersonalDataManager* manager() { return &manager_; }
  CountryComboboxModel* model() { return model_.get(); }

 private:
  TestPersonalDataManager manager_;
  std::unique_ptr<PrefService> pref_service_;
  std::unique_ptr<CountryComboboxModel> model_;
};

TEST_F(CountryComboboxModelTest, DefaultCountryCode) {
  std::string default_country = model()->GetDefaultCountryCode();
  EXPECT_EQ(manager()->GetDefaultCountryCodeForNewAddress(), default_country);

  AutofillCountry country(default_country, "en-US");
  EXPECT_EQ(country.name(), model()->GetItemAt(0));
}

TEST_F(CountryComboboxModelTest, AllCountriesHaveComponents) {
  ::i18n::addressinput::Localization localization;
  std::string unused;
  for (int i = 0; i < model()->GetItemCount(); ++i) {
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
