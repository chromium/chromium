// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_IMPORT_FORM_DATA_IMPORTER_TEST_API_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_IMPORT_FORM_DATA_IMPORTER_TEST_API_H_

#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_import/addresses/address_form_data_importer.h"
#include "components/autofill/core/browser/form_import/form_data_importer.h"

namespace autofill {

class FormStructure;

class FormDataImporterTestApi {
 public:
  using ExtractedAddressProfile =
      AddressFormDataImporter::ExtractedAddressProfile;
  using ExtractedFormData = FormDataImporter::ExtractedFormData;

  explicit FormDataImporterTestApi(FormDataImporter* fdi) : fdi_(*fdi) {}

  ExtractedFormData ExtractFormData(const FormStructure& form,
                                    bool profile_autofill_enabled,
                                    bool payment_methods_autofill_enabled) {
    return fdi_->ExtractFormData(form, profile_autofill_enabled,
                                 payment_methods_autofill_enabled);
  }

  void ImportAndProcessFormData(const FormStructure& submitted_form,
                                bool profile_autofill_enabled,
                                bool payment_methods_autofill_enabled,
                                ukm::SourceId ukm_source_id) {
    fdi_->ImportAndProcessFormData(submitted_form, profile_autofill_enabled,
                                   payment_methods_autofill_enabled,
                                   ukm_source_id);
  }

 private:
  const raw_ref<FormDataImporter> fdi_;
};

inline FormDataImporterTestApi test_api(FormDataImporter& fdi) {
  return FormDataImporterTestApi(&fdi);
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_IMPORT_FORM_DATA_IMPORTER_TEST_API_H_
