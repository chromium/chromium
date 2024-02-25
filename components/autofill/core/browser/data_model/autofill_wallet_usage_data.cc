// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/autofill_wallet_usage_data.h"

namespace autofill {

VirtualCardUsageData::VirtualCardUsageData(
    UsageDataId usage_data_id,
    InstrumentId instrument_id,
    VirtualCardLastFour virtual_card_last_four,
    url::Origin merchant_origin)
    : usage_data_id_(usage_data_id),
      instrument_id_(instrument_id),
      virtual_card_last_four_(virtual_card_last_four),
      merchant_origin_(merchant_origin) {}

VirtualCardUsageData::VirtualCardUsageData(const VirtualCardUsageData&) =
    default;

VirtualCardUsageData& VirtualCardUsageData::operator=(
    const VirtualCardUsageData&) = default;

VirtualCardUsageData::~VirtualCardUsageData() = default;

bool operator==(const VirtualCardUsageData& a, const VirtualCardUsageData& b) {
  return *a.usage_data_id() == *b.usage_data_id();
}

// static
AutofillWalletUsageData AutofillWalletUsageData::ForVirtualCard(
    const VirtualCardUsageData& virtual_card_usage_data) {
  return AutofillWalletUsageData(UsageDataType::kVirtualCard,
                                 virtual_card_usage_data);
}

AutofillWalletUsageData::AutofillWalletUsageData(
    const AutofillWalletUsageData&) = default;

AutofillWalletUsageData& AutofillWalletUsageData::operator=(
    const AutofillWalletUsageData&) = default;

AutofillWalletUsageData::~AutofillWalletUsageData() = default;

AutofillWalletUsageData::AutofillWalletUsageData(
    UsageDataType usage_data_type,
    const VirtualCardUsageData& virtual_card_usage_data)
    : usage_data_type_(usage_data_type),
      virtual_card_usage_data_(virtual_card_usage_data) {}

}  // namespace autofill
