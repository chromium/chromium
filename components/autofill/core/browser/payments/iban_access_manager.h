// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_IBAN_ACCESS_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_IBAN_ACCESS_MANAGER_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
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
  // Callback to notify the caller of the access manager when fetching the value
  // of an IBAN has finished.
  using OnIbanFetchedCallback =
      base::OnceCallback<void(const std::u16string& value)>;

  explicit IbanAccessManager(AutofillClient* client);
  IbanAccessManager(const IbanAccessManager&) = delete;
  IbanAccessManager& operator=(const IbanAccessManager&) = delete;
  virtual ~IbanAccessManager();

  // Returns the full IBAN value corresponding to the input `suggestion`.
  // As this may require a network round-trip for server IBANs,
  //`on_iban_fetched` is run once the value is fetched. For local IBANs, value
  // will be filled immediately.
  virtual void FetchValue(const Suggestion& suggestion,
                          OnIbanFetchedCallback on_iban_fetched);

 private:
  // Called when an UnmaskIban call is completed. The full IBAN value will be
  // returned via `value`.
  void OnUnmaskResponseReceived(OnIbanFetchedCallback on_iban_fetched,
                                base::TimeTicks unmask_request_timestamp,
                                AutofillClient::PaymentsRpcResult result,
                                const std::u16string& value);

  void OnServerIbanUnmaskCancelled();

  // The associated autofill client.
  const raw_ptr<AutofillClient> client_;

  base::WeakPtrFactory<IbanAccessManager> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_IBAN_ACCESS_MANAGER_H_
