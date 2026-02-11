// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_IMPORT_ADDRESSES_ADDRESS_FORM_DATA_IMPORTER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_IMPORT_ADDRESSES_ADDRESS_FORM_DATA_IMPORTER_H_

#include "base/memory/raw_ref.h"

namespace autofill {

class AddressDataManager;
class AutofillClient;
class AutofillProfile;

// Owned by `FormDataImporter`. Responsible for address-related form data
// importing functionality, including form extraction and processing.
class AddressFormDataImporter {
 public:
  explicit AddressFormDataImporter(AutofillClient* client);
  AddressFormDataImporter(const AddressFormDataImporter&) = delete;
  AddressFormDataImporter& operator=(const AddressFormDataImporter&) = delete;
  virtual ~AddressFormDataImporter();

  // TODO(crbug.com/481379161): It may be possible to make some of these
  //    functions private once all other refactoring finishes. Reevaluate at
  //    that time.

  // Clears all setting-inaccessible values from `profile`.
  void RemoveInaccessibleProfileValues(AutofillProfile& profile);

  AddressDataManager& address_data_manager();

 private:
  friend class AddressFormDataImporterTestApi;

  const raw_ref<AutofillClient> client_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_IMPORT_ADDRESSES_ADDRESS_FORM_DATA_IMPORTER_H_
