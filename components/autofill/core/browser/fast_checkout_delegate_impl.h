// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FAST_CHECKOUT_DELEGATE_IMPL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FAST_CHECKOUT_DELEGATE_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/fast_checkout_delegate.h"

namespace autofill {

class AutofillDriver;
class BrowserAutofillManager;

class FastCheckoutDelegateImpl : public FastCheckoutDelegate {
 public:
  explicit FastCheckoutDelegateImpl(BrowserAutofillManager* manager);
  FastCheckoutDelegateImpl(const FastCheckoutDelegateImpl&) = delete;
  FastCheckoutDelegateImpl& operator=(const FastCheckoutDelegateImpl&) = delete;
  ~FastCheckoutDelegateImpl() override;

  // FastCheckoutDelegate:
  bool TryToShowFastCheckout(const FormData& form,
                             const FormFieldData& field) override;
  bool IsShowingFastCheckoutUI() const override;
  void HideFastCheckoutUI() override;
  void OnFastCheckoutUIHidden() override;
  AutofillDriver* GetDriver() override;
  void Reset() override;

 private:
  enum class FastCheckoutState {
    kNotShownYet,
    kIsShowing,
    kWasShown,
  };

  base::WeakPtr<FastCheckoutDelegateImpl> GetWeakPtr();

  FastCheckoutState fast_checkout_state_ = FastCheckoutState::kNotShownYet;
  const raw_ptr<BrowserAutofillManager> manager_;
  base::WeakPtrFactory<FastCheckoutDelegateImpl> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FAST_CHECKOUT_DELEGATE_IMPL_H_
