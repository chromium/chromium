// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_BNPL_TOS_CONTROLLER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_BNPL_TOS_CONTROLLER_H_

#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/data_model/payments/bnpl_issuer.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/signin/public/identity_manager/account_info.h"

namespace autofill {

namespace payments {
struct TextWithLink;
}  // namespace payments

// Interface that exposes controller functionality to BnplTosView.
class BnplTosController {
 public:
  // Callbacks used for when the user accepts and cancels the BNPL ToS dialog.
  virtual void OnUserAccepted() = 0;
  virtual void OnUserCancelled() = 0;

  // Strings used for the view.
  virtual std::u16string GetOkButtonLabel() const = 0;
  virtual std::u16string GetCancelButtonLabel() const = 0;
  virtual std::u16string GetTitle() const = 0;
  virtual std::u16string GetReviewText() const = 0;
  virtual std::u16string GetApproveText() const = 0;
  virtual payments::TextWithLink GetLinkText() const = 0;
  virtual const LegalMessageLines& GetLegalMessageLines() const = 0;
  // Returns the account info of the signed-in user.
  virtual AccountInfo GetAccountInfo() const = 0;
  // Return the BNPL issuer id.
  virtual BnplIssuer::IssuerId GetIssuerId() const = 0;

  virtual base::WeakPtr<BnplTosController> GetWeakPtr() = 0;

 protected:
  virtual ~BnplTosController() = default;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_BNPL_TOS_CONTROLLER_H_
