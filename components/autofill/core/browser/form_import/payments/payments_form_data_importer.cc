// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_import/payments/payments_form_data_importer.h"

#include "base/check_deref.h"

namespace autofill::payments {

PaymentsFormDataImporter::PaymentsFormDataImporter(AutofillClient* client)
    : client_(CHECK_DEREF(client)) {}

PaymentsFormDataImporter::~PaymentsFormDataImporter() = default;

}  // namespace autofill::payments
