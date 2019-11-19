// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/card_unmask_prompt_view_tester_views.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/autofill/payments/card_unmask_prompt_views.h"
#include "ui/views/controls/textfield/textfield.h"

namespace autofill {

// static
std::unique_ptr<CardUnmaskPromptViewTester> CardUnmaskPromptViewTester::For(
    CardUnmaskPromptView* view) {
  return std::make_unique<CardUnmaskPromptViewTesterViews>(
      static_cast<CardUnmaskPromptViews*>(view));
}

// Class that facilitates testing.
CardUnmaskPromptViewTesterViews::CardUnmaskPromptViewTesterViews(
    CardUnmaskPromptViews* view)
    : view_(view) {}

CardUnmaskPromptViewTesterViews::~CardUnmaskPromptViewTesterViews() {}

void CardUnmaskPromptViewTesterViews::Close() {
  view_->ClosePrompt();
}

void CardUnmaskPromptViewTesterViews::EnterCVCAndAccept() {
  view_->cvc_input_->SetText(base::ASCIIToUTF16("123"));
  view_->AcceptDialog();
}

}  // namespace autofill
