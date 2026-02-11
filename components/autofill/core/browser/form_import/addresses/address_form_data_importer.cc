// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_import/addresses/address_form_data_importer.h"

#include "base/check_deref.h"

namespace autofill {

AddressFormDataImporter::AddressFormDataImporter(AutofillClient* client)
    : client_(CHECK_DEREF(client)) {}

AddressFormDataImporter::~AddressFormDataImporter() = default;

}  // namespace autofill
