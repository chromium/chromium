// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_PAYMENTS_SERVER_CVC_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_PAYMENTS_SERVER_CVC_H_

#include <stddef.h>

#include <string>

#include "base/time/time.h"

namespace autofill {

// Helper struct to better group server cvc related variables for better
// passing last_updated_timestamp, which is needed for sync bridge. Limited
// scope in autofill table & sync bridge.
struct ServerCvc {
  bool operator==(const ServerCvc&) const = default;

  // A server generated id to identify the corresponding credit card.
  int64_t instrument_id;
  // CVC value of the card.
  std::u16string cvc;
  // The timestamp of the most recent update to the data entry.
  base::Time last_updated_timestamp;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_PAYMENTS_SERVER_CVC_H_
