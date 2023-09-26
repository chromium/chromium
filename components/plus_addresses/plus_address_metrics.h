// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_METRICS_H_
#define COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_METRICS_H_

namespace plus_addresses {
// A small, stateless utility class for logging metrics. It will handle autofill
// metrics, plus_address_service metrics, and user interaction metrics.
class PlusAddressMetrics {
 public:
  enum class PlusAddressModalEvent {
    kModalShown = 0,
    kModalCanceled = 1,
    kModalConfirmed = 2,
    kMaxValue = kModalConfirmed,
  };

  // As of now, the class is intended to be stateless and static; do not allow
  // construction.
  PlusAddressMetrics() = delete;
  PlusAddressMetrics(const PlusAddressMetrics&) = delete;
  PlusAddressMetrics& operator=(const PlusAddressMetrics&) = delete;

  // Log plus address creation modal events.
  static void RecordModalEvent(PlusAddressModalEvent plus_address_modal_event);
};
}  // namespace plus_addresses

#endif  // COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_METRICS_H_
