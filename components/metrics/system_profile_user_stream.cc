// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/system_profile_user_stream.h"

#include <stddef.h>
#include <stdint.h>

#include <string_view>

#include "base/containers/span.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"

namespace metrics {

namespace {

// Records a histogram to monitor how often the system profile is too large to
// be written to the user stream for Crashpad. If this starts to exceed zero for
// a large number of clients, `kSystemProfileUserStreamCapacityBytes` should be
// increased. See metric `UMA.ProtoSize.SystemProfile` for the actual size of
// the system profile.
void RecordUserStreamOverflow(size_t bytes_over) {
  base::UmaHistogramCounts100000("UMA.SystemProfile.UserStreamOverflow",
                                 base::saturated_cast<int>(bytes_over));
}

}  // namespace

void WriteSystemProfileUserStream(std::string_view payload,
                                  UserStreamWriter writer) {
  if (payload.size() > kSystemProfileUserStreamCapacityBytes) {
    RecordUserStreamOverflow(payload.size() -
                             kSystemProfileUserStreamCapacityBytes);
    // Not written: partially written protobufs are discarded by the crash
    // server, so the stream keeps its previous payload.
    return;
  }
  RecordUserStreamOverflow(0);

  writer(kSystemProfileUserStreamType, base::as_byte_span(payload));
}

}  // namespace metrics
