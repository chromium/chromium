// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_BNPL_TOS_CONTROLLER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_BNPL_TOS_CONTROLLER_H_

#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"

struct AccountInfo;

namespace autofill {

// Contains a string of text and the location of a substring for a link as well
// as the url for where the link should lead to.
struct TextWithLink {
  std::u16string text;
  gfx::Range offset;
  GURL url;
};

// Interface that exposes controller functionality to BnplTosView.
class BnplTosController {
 public:
  // Callback received when the BNPL ToS view is to be closed.
  // `user_accepted` is a boolean that is true if the user accepts the dialog,
  // and false if the user cancels the dialog.
  virtual void OnViewClosing(bool user_accepted) = 0;

  // Strings used for the view.
  virtual std::u16string GetOkButtonLabel() const = 0;
  virtual std::u16string GetCancelButtonLabel() const = 0;
  virtual std::u16string GetTitle() const = 0;
  virtual std::u16string GetReviewText() const = 0;
  virtual std::u16string GetApproveText() const = 0;
  virtual TextWithLink GetLinkText() const = 0;
  virtual const LegalMessageLines& GetLegalMessageLines() const = 0;

  // Returns the account info of the signed-in user.
  virtual AccountInfo GetAccountInfo() const = 0;

  virtual base::WeakPtr<BnplTosController> GetWeakPtr() = 0;

 protected:
  virtual ~BnplTosController() = default;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_BNPL_TOS_CONTROLLER_H_
