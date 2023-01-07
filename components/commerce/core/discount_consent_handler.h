// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_DISCOUNT_CONSENT_HANDLER_H_
#define COMPONENTS_COMMERCE_CORE_DISCOUNT_CONSENT_HANDLER_H_

namespace commerce {
class DiscountConsentHandler {
 public:
  virtual void AcknowledgeDiscountConsent(bool is_accepted) = 0;
  virtual void DismissedDiscountConsent() = 0;
  virtual void InterestedInDiscountConsent() = 0;
};
}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_DISCOUNT_CONSENT_HANDLER_H_
