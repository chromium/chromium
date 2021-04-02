// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_CARD_UNMASK_PROMPT_VIEW_TESTER_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_CARD_UNMASK_PROMPT_VIEW_TESTER_VIEWS_H_

#include "base/macros.h"
#include "chrome/browser/ui/autofill/payments/card_unmask_prompt_view_tester.h"

namespace autofill {

class CardUnmaskPromptViews;

// Class that facilitates testing a CardUnmaskPromptViews.
class CardUnmaskPromptViewTesterViews : public CardUnmaskPromptViewTester {
 public:
  explicit CardUnmaskPromptViewTesterViews(CardUnmaskPromptViews* view);
  ~CardUnmaskPromptViewTesterViews() override;

  // CardUnmaskPromptViewTester:
  void Close() override;
  void EnterCVCAndAccept() override;

 private:
  CardUnmaskPromptViews* view_;

  DISALLOW_COPY_AND_ASSIGN(CardUnmaskPromptViewTesterViews);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_CARD_UNMASK_PROMPT_VIEW_TESTER_VIEWS_H_
