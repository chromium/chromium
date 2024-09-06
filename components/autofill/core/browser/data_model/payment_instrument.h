// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_PAYMENT_INSTRUMENT_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_PAYMENT_INSTRUMENT_H_

#include <cstdint>
#include <set>

#include "base/types/strong_alias.h"
#include "components/autofill/core/common/dense_set.h"
#include "url/gurl.h"

namespace autofill {

class PaymentInstrument;

// Base class for all payment instruments. A payment instrument is considered to
// be any form of payment stored in the GPay backend that can be used to
// facilitate a payment on a webpage. Examples of derived class: BankAccount,
// CreditCard etc.
class PaymentInstrument final {
 public:
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.autofill.payments
  // A payment rail can loosely represent the different ways in which Chrome can
  // intercept a user's payment journey and assist in completing it. For
  // example: Pix, UPI, Card number, IBAN etc.
  enum class PaymentRail {
    kUnknown = 0,
    // Payment Rail used in Brazil.
    kPix = 1,
    // Payment Rail used for Ewallet.
    kPaymentHyperlink = 2,
    kMaxValue = kPaymentHyperlink,
  };

  PaymentInstrument(int64_t instrument_id,
                    std::u16string nickname,
                    GURL display_icon_url,
                    DenseSet<PaymentInstrument::PaymentRail> supported_rails,
                    bool is_fido_enrolled = false);
  PaymentInstrument(const PaymentInstrument& other);
  PaymentInstrument& operator=(const PaymentInstrument& other);
  ~PaymentInstrument();

  friend std::strong_ordering operator<=>(const PaymentInstrument&,
                                          const PaymentInstrument&);
  friend bool operator==(const PaymentInstrument&, const PaymentInstrument&);

  int64_t instrument_id() const { return instrument_id_; }

  DenseSet<PaymentRail> supported_rails() const { return supported_rails_; }

  // Check whether the PaymentInstrument is supported for a particular rail.
  bool IsSupported(PaymentRail payment_rail) const;

  const std::u16string& nickname() const { return nickname_; }

  const GURL& display_icon_url() const { return display_icon_url_; }

  bool is_fido_enrolled() const { return is_fido_enrolled_; }

 private:
  // This is the ID assigned by the payments backend to uniquely identify this
  // PaymentInstrument.
  int64_t instrument_id_;

  // The nickname of the PaymentInstrument. May be empty.
  std::u16string nickname_;

  // The url to fetch the icon for the PaymentInstrument. May be empty.
  GURL display_icon_url_;

  // All the payment rails that are supported by this instrument.
  DenseSet<PaymentRail> supported_rails_;

  // Whether the device is enrolled in FIDO for this instrument.
  bool is_fido_enrolled_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_PAYMENT_INSTRUMENT_H_
