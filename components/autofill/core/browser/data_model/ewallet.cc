// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/ewallet.h"

#include <cstdint>
#include <string>
#include <string_view>

#include "base/containers/flat_set.h"
#include "url/gurl.h"

namespace autofill {

Ewallet::Ewallet(int64_t instrument_id,
                 std::u16string nickname,
                 GURL display_icon_url,
                 std::u16string ewallet_name,
                 std::u16string account_display_name,
                 base::flat_set<std::u16string> supported_payment_link_uris,
                 bool is_fido_enrolled)
    : ewallet_name_(std::move(ewallet_name)),
      account_display_name_(std::move(account_display_name)),
      supported_payment_link_uris_(std::move(supported_payment_link_uris)),
      payment_instrument_(
          instrument_id,
          nickname,
          display_icon_url,
          DenseSet({PaymentInstrument::PaymentRail::kPaymentHyperlink}),
          is_fido_enrolled) {}
Ewallet::Ewallet(const Ewallet& other) = default;
Ewallet& Ewallet::operator=(const Ewallet& other) = default;
Ewallet::~Ewallet() = default;

std::strong_ordering operator<=>(const Ewallet& a, const Ewallet& b) = default;
bool operator==(const Ewallet& a, const Ewallet& b) = default;

}  // namespace autofill
