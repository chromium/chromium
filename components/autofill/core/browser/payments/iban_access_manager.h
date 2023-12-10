// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_IBAN_ACCESS_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_IBAN_ACCESS_MANAGER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/autofill_client.h"

namespace autofill {

struct Suggestion;

// This class provides functionality to return a full (non-masked) IBAN value
// when the user clicks on an IBAN suggestion.
//
// It is able to handle both server-saved IBANs (which require a network
// call to Payments server to retrieve the full value) as well as local-saved
// IBANs.
class IbanAccessManager {
 public:
  class Accessor {
   public:
    virtual ~Accessor() = default;
    virtual void OnIbanFetched(const std::u16string& value) = 0;
  };

  explicit IbanAccessManager(AutofillClient* client);
  IbanAccessManager(const IbanAccessManager&) = delete;
  IbanAccessManager& operator=(const IbanAccessManager&) = delete;
  ~IbanAccessManager();

  // Returns the full IBAN value corresponding to the input `suggestion`.
  // As this may require a network round-trip for server IBANs, the value
  // is returned via a call to `Accessor::OnIbanFetched` which may occur
  // asynchronously to this method.
  // If the IBAN value cannot be extracted, the accessor will never be called.
  void FetchValue(const Suggestion& suggestion,
                  base::WeakPtr<Accessor> accessor);

 private:
  // Called when an UnmaskIban call is completed. The full IBAN value will be
  // returned via `value`.
  void OnUnmaskResponseReceived(base::WeakPtr<Accessor> accessor,
                                AutofillClient::PaymentsRpcResult result,
                                const std::u16string& value);

  // The associated autofill client.
  const raw_ptr<AutofillClient> client_;

  base::WeakPtrFactory<IbanAccessManager> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_IBAN_ACCESS_MANAGER_H_
