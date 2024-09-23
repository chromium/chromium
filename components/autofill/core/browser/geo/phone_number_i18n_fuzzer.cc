// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "base/at_exit.h"
#include "base/i18n/icu_util.h"
#include "components/autofill/core/browser/geo/phone_number_i18n.h"
#include "third_party/libphonenumber/phonenumber_api.h"

namespace autofill {

struct IcuEnvironment {
  IcuEnvironment() { CHECK(base::i18n::InitializeICU()); }
  // Used by ICU integration.
  base::AtExitManager at_exit_manager;
};

IcuEnvironment* env = new IcuEnvironment();

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // At least 2 bytes are needed for |default_region|, due to the
  // ParsePhoneNumber contract.
  if (size < 2)
    return 0;

  // SAFETY: Size is at least 2.
  UNSAFE_BUFFERS(
      std::string default_region(reinterpret_cast<const char*>(data), 2));
  UNSAFE_BUFFERS(std::u16string value(
      reinterpret_cast<const char16_t*>(data + 2), (size - 2) / 2));
  std::u16string dummy_country_code;
  std::u16string dummy_city_code;
  std::u16string dummy_number;
  std::string dummy_inferred_region;
  ::i18n::phonenumbers::PhoneNumber dummy_i18n_number;

  bool dummy_result = i18n::ParsePhoneNumber(
      value, default_region, &dummy_country_code, &dummy_city_code,
      &dummy_number, &dummy_inferred_region, &dummy_i18n_number);
  if (dummy_result)
    return 0;
  return 0;
}

}  // namespace autofill
