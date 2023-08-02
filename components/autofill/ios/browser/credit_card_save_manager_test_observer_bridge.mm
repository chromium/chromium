// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/browser/credit_card_save_manager_test_observer_bridge.h"

#include "base/check.h"

namespace autofill {

CreditCardSaveManagerTestObserverBridge::
    CreditCardSaveManagerTestObserverBridge(
        CreditCardSaveManager* credit_card_save_manager,
        id<CreditCardSaveManagerTestObserver> observer)
    : observer_(observer) {
  DCHECK(observer_);
  DCHECK(credit_card_save_manager);
  credit_card_save_manager->SetEventObserverForTesting(this);
}

void CreditCardSaveManagerTestObserverBridge::OnOfferLocalSave() {
  [observer_ offeredLocalSave];
}

void CreditCardSaveManagerTestObserverBridge::OnDecideToRequestUploadSave() {
  [observer_ decidedToRequestUploadSave];
}

void CreditCardSaveManagerTestObserverBridge::
    OnReceivedGetUploadDetailsResponse() {
  [observer_ receivedGetUploadDetailsResponse];
}

void CreditCardSaveManagerTestObserverBridge::OnSentUploadCardRequest() {
  [observer_ sentUploadCardRequest];
}

void CreditCardSaveManagerTestObserverBridge::OnReceivedUploadCardResponse() {
  [observer_ receivedUploadCardResponse];
}

void CreditCardSaveManagerTestObserverBridge::OnStrikeChangeComplete() {
  [observer_ strikeChangeComplete];
}

}  // namespace autofill
