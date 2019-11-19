// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/test_authentication_requester.h"

#include "base/strings/string16.h"
#include "components/autofill/core/browser/data_model/credit_card.h"

namespace autofill {

TestAuthenticationRequester::TestAuthenticationRequester() {}

TestAuthenticationRequester::~TestAuthenticationRequester() {}

base::WeakPtr<TestAuthenticationRequester>
TestAuthenticationRequester::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void TestAuthenticationRequester::OnCVCAuthenticationComplete(
    const CreditCardCVCAuthenticator::CVCAuthenticationResponse& response) {
  did_succeed_ = response.did_succeed;
  if (did_succeed_) {
    DCHECK(response.card);
    number_ = response.card->number();
  }
}

#if !defined(OS_IOS)
void TestAuthenticationRequester::OnFIDOAuthenticationComplete(
    bool did_succeed,
    const CreditCard* card) {
  did_succeed_ = did_succeed;
  if (did_succeed_) {
    DCHECK(card);
    number_ = card->number();
  }
}

void TestAuthenticationRequester::IsUserVerifiableCallback(
    bool is_user_verifiable) {
  is_user_verifiable_ = is_user_verifiable;
}
#endif

}  // namespace autofill
