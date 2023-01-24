// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/card_unmask_prompt_view_tester_views.h"

#include <string>

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

void CardUnmaskPromptViewTesterViews::EnterCVCAndAccept(
    const std::u16string& cvc) {
  view_->cvc_input_->SetText(cvc);
  view_->AcceptDialog();
}

}  // namespace autofill
