// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_metadata.h"

namespace autofill {

bool AutofillMetadata::operator==(const AutofillMetadata& metadata) const {
  return id == metadata.id && use_count == metadata.use_count &&
         use_date == metadata.use_date &&
         has_converted == metadata.has_converted &&
         billing_address_id == metadata.billing_address_id;
}

}  // namespace autofill
