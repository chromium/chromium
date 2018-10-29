// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_METADATA_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_METADATA_H_

#include <string>

#include "base/time/time.h"

namespace autofill {

// This struct contains the metadata to an autofill data model. It is used to
// abstract the data from the metadata.
struct AutofillMetadata {
 public:
  AutofillMetadata(){};
  ~AutofillMetadata(){};

  bool operator==(const AutofillMetadata&) const;

  // The ID for the model. This should be the guid for local data and server_id
  // for the server data.
  std::string id;

  // The number of times the model has been used.
  size_t use_count;

  // The last time the model was used.
  base::Time use_date;

  // Only useful for SERVER_PROFILEs. Whether the server profile has been
  // converted to a local profile.
  bool has_converted = false;

  // Only useful for SERVER_CARDs. The identifier of the billing address for the
  // card.
  std::string billing_address_id;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_METADATA_H_
