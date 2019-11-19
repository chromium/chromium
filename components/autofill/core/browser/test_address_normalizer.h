// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_ADDRESS_NORMALIZER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_ADDRESS_NORMALIZER_H_

#include <string>

#include "base/callback.h"
#include "components/autofill/core/browser/address_normalizer.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"

namespace autofill {

// A simpler version of the address normalizer to be used in tests. Can be set
// to normalize instantaneously or to wait for a call.
class TestAddressNormalizer : public AddressNormalizer {
 public:
  TestAddressNormalizer();
  ~TestAddressNormalizer() override;

  void LoadRulesForRegion(const std::string& region_code) override {}

  void NormalizeAddressAsync(
      const AutofillProfile& profile,
      int timeout_seconds,
      AddressNormalizer::NormalizationCallback callback) override;
  bool NormalizeAddressSync(AutofillProfile* profile) override;

  void OnAddressValidationRulesLoaded(const std::string& region_code,
                                      bool success) override {}

  void DelayNormalization();

  void CompleteAddressNormalization();

 private:
  AutofillProfile profile_;
  AddressNormalizer::NormalizationCallback callback_;

  bool instantaneous_normalization_ = true;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_ADDRESS_NORMALIZER_H_
