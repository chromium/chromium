// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_PAYMENT_INSTRUMENT_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_PAYMENT_INSTRUMENT_H_

#include <cstdint>
#include <set>

#include "base/types/strong_alias.h"
#include "url/gurl.h"

namespace autofill {

class AutofillTable;
class PaymentInstrument;

bool operator==(const PaymentInstrument& a, const PaymentInstrument& b);

// Base class for all payment instruments. A payment instrument is considered to
// be any form of payment stored in the GPay backend that can be used to
// facilitate a payment on a webpage. Examples of derived class: BankAccount,
// CreditCard etc.
class PaymentInstrument {
 public:
  // A payment rail can loosely represent the different ways in which Chrome can
  // intercept a user's payment journey and assist in completing it. For
  // example: Pix, UPI, Card number, IBAN etc.
  enum class PaymentRail {
    kUnknown = 0,
    // Payment Rail used in Brazil.
    kPix = 1
  };
  enum class InstrumentType { kUnknown = 0, kBankAccount = 1 };

  PaymentInstrument(int64_t instrument_id,
                    std::u16string_view nickname,
                    const GURL& display_icon_url);
  PaymentInstrument(const PaymentInstrument& other);
  PaymentInstrument& operator=(const PaymentInstrument& other);
  virtual ~PaymentInstrument();
  // Return the type of PaymentInstrument.
  virtual InstrumentType GetInstrumentType() const = 0;

  // Database operations to be implemented by derived class. The derived class
  // is expected to call the corresponding database method on the AutofillTable
  // object. This is required for callers to call these methods on the base
  // class without knowing the type of the derived class.
  virtual bool AddToDatabase(AutofillTable* database) const = 0;
  virtual bool UpdateInDatabase(AutofillTable* database) const = 0;
  virtual bool DeleteFromDatabase(AutofillTable* database) const = 0;

  int64_t instrument_id() const { return instrument_id_; }

  const std::set<PaymentRail>& supported_rails() const {
    return supported_rails_;
  }

  // Add a payment rail to the list of rails supported for this instrument.
  void AddPaymentRail(PaymentRail payment_rail);
  // Check whether the PaymentInstrument is supported for a particular rail.
  bool IsSupported(PaymentRail payment_rail) const;

  const std::u16string& nickname() const { return nickname_; }

  const GURL& display_icon_url() const { return display_icon_url_; }

 private:
  // This is the ID assigned by the payments backend to uniquely identify this
  // PaymentInstrument.
  int64_t instrument_id_;

  // All the payment rails that are supported by this instrument.
  std::set<PaymentRail> supported_rails_;

  // The nickname of the PaymentInstrument. May be empty.
  std::u16string nickname_;

  // The url to fetch the icon for the PaymentInstrument. May be empty.
  GURL display_icon_url_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_PAYMENT_INSTRUMENT_H_
