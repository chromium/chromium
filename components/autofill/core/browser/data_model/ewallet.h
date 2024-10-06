// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_EWALLET_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_EWALLET_H_

#include <cstdint>
#include <string>
#include <string_view>

#include "base/containers/flat_set.h"
#include "components/autofill/core/browser/data_model/payment_instrument.h"

class GURL;

namespace autofill {

// An ewallet is a form of payment that facilitates a push payment to the
// merchant rather than a pull payment from the merchant. In the case of a pull
// payment, the merchant collects payment information from the user and
// initiates the payment with the issuer. Example: credit cards. For a push
// payment, the payment is initiated from the issuer side, to a payment target
// indicating the recipient merchant. Ewallets fall into that category. Ewallets
// are typically already linked to a user's bank account. This class consists of
// the details for a user's ewallet, and this data is synced from the Google
// Payments server.
class Ewallet {
 public:
  Ewallet(int64_t instrument_id,
          std::u16string nickname,
          GURL display_icon_url,
          std::u16string ewallet_name,
          std::u16string account_display_name,
          base::flat_set<std::u16string> supported_payment_link_uris,
          bool is_fido_enrolled);
  Ewallet(const Ewallet& other);
  Ewallet& operator=(const Ewallet& other);
  ~Ewallet();

  friend std::strong_ordering operator<=>(const Ewallet&, const Ewallet&);
  friend bool operator==(const Ewallet&, const Ewallet&);

  const std::u16string& ewallet_name() const { return ewallet_name_; }

  const std::u16string& account_display_name() const {
    return account_display_name_;
  }

  const base::flat_set<std::u16string>& supported_payment_link_uris() const {
    return supported_payment_link_uris_;
  }

  const PaymentInstrument& payment_instrument() const {
    return payment_instrument_;
  }

 private:
  // Name of the ewallet provider.
  std::u16string ewallet_name_;

  // Display name of the ewallet account.
  std::u16string account_display_name_;

  // Chrome matches the payment links on web pages against the list of payment
  // link URI regexes. The regex matching approach makes it possible to launch
  // new payment methods without requiring any client side changes. More details
  // on payment links can be found at
  // https://github.com/aneeshali/paymentlink/blob/main/docs/explainer.md.
  base::flat_set<std::u16string> supported_payment_link_uris_;

  // Fields common for all types of payment instruments.
  PaymentInstrument payment_instrument_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_EWALLET_H_
