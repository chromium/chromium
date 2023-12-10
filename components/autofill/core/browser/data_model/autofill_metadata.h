// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_METADATA_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_METADATA_H_

#include <string>

#include "base/time/time.h"

namespace autofill {

// This struct contains the metadata to an autofill data model. It is used to
// abstract the data from the metadata.
struct AutofillMetadata {
 public:
  AutofillMetadata() = default;
  ~AutofillMetadata() = default;

  bool operator==(const AutofillMetadata&) const = default;

  // Returns whether the metadata is deletable: if it has not been used for
  // longer than |kDisusedAddressDeletionTimeDelta|.
  bool IsDeletable() const;

  // The ID for the model. This should be the guid for local data and server_id
  // for the server data.
  std::string id;

  // The number of times the model has been used.
  size_t use_count;

  // The last time the model was used.
  base::Time use_date;

  // Only useful for SERVER_CARDs. The identifier of the billing address for the
  // card.
  std::string billing_address_id;
};

// So we can compare AutofillMetadata with EXPECT_EQ().
std::ostream& operator<<(std::ostream& os, const AutofillMetadata& metadata);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_METADATA_H_
