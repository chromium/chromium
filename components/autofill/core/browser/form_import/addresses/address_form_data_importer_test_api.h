// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_IMPORT_ADDRESSES_ADDRESS_FORM_DATA_IMPORTER_TEST_API_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_IMPORT_ADDRESSES_ADDRESS_FORM_DATA_IMPORTER_TEST_API_H_

#include "components/autofill/core/browser/form_import/addresses/address_form_data_importer.h"

namespace autofill {

class AddressFormDataImporterTestApi {
 public:
  explicit AddressFormDataImporterTestApi(AddressFormDataImporter* address_fdi)
      : address_fdi_(*address_fdi) {}

 private:
  const raw_ref<AddressFormDataImporter> address_fdi_;
};

inline AddressFormDataImporterTestApi test_api(
    AddressFormDataImporter& address_fdi) {
  return AddressFormDataImporterTestApi(&address_fdi);
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_IMPORT_ADDRESSES_ADDRESS_FORM_DATA_IMPORTER_TEST_API_H_
