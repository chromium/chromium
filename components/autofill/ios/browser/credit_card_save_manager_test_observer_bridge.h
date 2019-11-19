// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_BROWSER_CREDIT_CARD_SAVE_MANAGER_TEST_OBSERVER_BRIDGE_H_
#define COMPONENTS_AUTOFILL_IOS_BROWSER_CREDIT_CARD_SAVE_MANAGER_TEST_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#include "base/macros.h"
#include "components/autofill/core/browser/payments/credit_card_save_manager.h"

// A protocol to be adopted by EarlGrey tests to get notified of actions that
// occur in autofill::CreditCardSaveManager.
@protocol CreditCardSaveManagerTestObserver<NSObject>

- (void)offeredLocalSave;

- (void)decidedToRequestUploadSave;

- (void)receivedGetUploadDetailsResponse;

- (void)sentUploadCardRequest;

- (void)receivedUploadCardResponse;

- (void)strikeChangeComplete;

@end

namespace autofill {

// Forwards actions from autofill::CreditCardSaveManager to the Objective-C
// observer, CreditCardSaveManagerTestObserver.
class CreditCardSaveManagerTestObserverBridge
    : public CreditCardSaveManager::ObserverForTest {
 public:
  explicit CreditCardSaveManagerTestObserverBridge(
      CreditCardSaveManager* credit_card_save_manager,
      id<CreditCardSaveManagerTestObserver> observer);
  ~CreditCardSaveManagerTestObserverBridge() override = default;

  // CreditCardSaveManager::ObserverForTest:
  void OnOfferLocalSave() override;
  void OnDecideToRequestUploadSave() override;
  void OnReceivedGetUploadDetailsResponse() override;
  void OnSentUploadCardRequest() override;
  void OnReceivedUploadCardResponse() override;
  void OnStrikeChangeComplete() override;

 private:
  __weak id<CreditCardSaveManagerTestObserver> observer_ = nil;

  DISALLOW_COPY_AND_ASSIGN(CreditCardSaveManagerTestObserverBridge);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_IOS_BROWSER_CREDIT_CARD_SAVE_MANAGER_TEST_OBSERVER_BRIDGE_H_
