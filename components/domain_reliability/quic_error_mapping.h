// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOMAIN_RELIABILITY_QUIC_ERROR_MAPPING_H_
#define COMPONENTS_DOMAIN_RELIABILITY_QUIC_ERROR_MAPPING_H_

#include <string>

#include "net/third_party/quiche/src/quiche/quic/core/quic_packets.h"

// N.B. This file and the .cc are separate from util.h/.cc so that they can be
// independently updated by folks working on QUIC when new errors are added.

namespace domain_reliability {

// Attempts to convert a QUIC error into the quic_error string
// that should be recorded in a beacon. Returns true and parse the QUIC error
// code in |beacon_quic_error_out| if it could.
// Returns false and clear |beacon_quic_error_out| otherwise.
bool GetDomainReliabilityBeaconQuicError(quic::QuicErrorCode quic_error,
                                         std::string* beacon_quic_error_out);

}  // namespace domain_reliability

#endif  // COMPONENTS_DOMAIN_RELIABILITY_QUIC_ERROR_MAPPING_H_
