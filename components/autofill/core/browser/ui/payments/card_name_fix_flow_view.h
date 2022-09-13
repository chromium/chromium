// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_CARD_NAME_FIX_FLOW_VIEW_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_CARD_NAME_FIX_FLOW_VIEW_H_

namespace autofill {

// The cross-platform UI interface which prompts the user to confirm their name.
// This object is responsible for its own lifetime.
class CardNameFixFlowView {
 public:
  virtual void Show() = 0;
  virtual void ControllerGone() = 0;

 protected:
  virtual ~CardNameFixFlowView() = default;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_CARD_NAME_FIX_FLOW_VIEW_H_
