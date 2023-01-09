// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_WALLET_USAGE_DATA_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_WALLET_USAGE_DATA_H_

#include <string>
#include <vector>

#include "base/types/strong_alias.h"
#include "url/origin.h"

namespace autofill {

// Usage data information related to a virtual card.
struct VirtualCardUsageData {
 public:
  // Represents the unique identifier for the actual card hat the virtual card
  // is linked to. Used to access more information regarding the actual and
  // virtual card from Payments server. Originally generated in the Payments
  // server.
  using InstrumentId = base::StrongAlias<class InstrumentIdTag, int64_t>;

  // Represents the last four digits of the virtual card.
  using VirtualCardLastFour =
      base::StrongAlias<class VirtualCardLastFourTag, std::string>;

  VirtualCardUsageData();
  VirtualCardUsageData(const VirtualCardUsageData&);
  VirtualCardUsageData& operator=(const VirtualCardUsageData&);
  ~VirtualCardUsageData();

  InstrumentId instrument_id = InstrumentId(0);

  VirtualCardLastFour virtual_card_last_four;

  // The origin of the merchant url the virtual card was retrieved on. May not
  // be set if accessed from an Android application. Example:
  // https://www.walmart.com.
  url::Origin merchant_origin;

  // The app package on Android OS the virtual card was retrieved on. May not be
  // set if accessed from Chrome browser. Example: com.walmart.android.
  std::string merchant_app_package;
};

// Contains various information related to the usages of a specific payment
// method on a individual merchant website or app. Wallet highlights that this
// class is only relevant to payment data.
class AutofillWalletUsageData {
 public:
  enum class UsageDataType {
    // Default value, should not be used.
    kUnknown = 0,
    // Usage data is linked to a virtual card.
    kVirtualCard = 1,
  };

  static AutofillWalletUsageData ForVirtualCard(
      const VirtualCardUsageData& virtual_card_usage_data);

  AutofillWalletUsageData(const AutofillWalletUsageData&);
  AutofillWalletUsageData& operator=(const AutofillWalletUsageData&);
  ~AutofillWalletUsageData();

  const VirtualCardUsageData& virtual_card_usage_data() const {
    return virtual_card_usage_data_;
  }

  UsageDataType usage_data_type() const { return usage_data_type_; }

 private:
  AutofillWalletUsageData(UsageDataType usage_data_type,
                          const VirtualCardUsageData& virtual_card_usage_data);

  // The type of payment that the usage data is linked to.
  UsageDataType usage_data_type_ = UsageDataType::kUnknown;

  // Contains additional information about the virtual card. Only set if usage
  // data is originates from a virtual card.
  VirtualCardUsageData virtual_card_usage_data_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_WALLET_USAGE_DATA_H_
