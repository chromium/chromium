// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_FAST_CHECKOUT_FAST_CHECKOUT_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_FAST_CHECKOUT_FAST_CHECKOUT_CONTROLLER_IMPL_H_

#include "chrome/browser/ui/fast_checkout/fast_checkout_controller.h"

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/fast_checkout/fast_checkout_view.h"
#include "ui/gfx/native_widget_types.h"

namespace content {
class WebContents;
}  // namespace content

// The controller acts as C++ entry point to Fast Checkout UI. It provides
// clients all necessary directives to communicate back and forth with the
// bottom sheet.
class FastCheckoutControllerImpl : public FastCheckoutController {
 public:
  class Delegate {
   public:
    // Called when the bottom sheet options are accepted.
    virtual void OnOptionsSelected(
        std::unique_ptr<autofill::AutofillProfile> profile,
        std::unique_ptr<autofill::CreditCard> credit_card) = 0;

    // Called when the bottom sheet is dismissed with no selection.
    virtual void OnDismiss() = 0;

   protected:
    virtual ~Delegate() = default;
  };

  FastCheckoutControllerImpl(content::WebContents* web_contents,
                             Delegate* delegate);
  ~FastCheckoutControllerImpl() override;
  FastCheckoutControllerImpl(const FastCheckoutControllerImpl&) = delete;
  FastCheckoutControllerImpl& operator=(const FastCheckoutControllerImpl&) =
      delete;

  // FastCheckoutController:
  void Show(
      const std::vector<const autofill::AutofillProfile*>& autofill_profiles,
      const std::vector<autofill::CreditCard*>& credit_cards) override;
  void OnOptionsSelected(
      std::unique_ptr<autofill::AutofillProfile> profile,
      std::unique_ptr<autofill::CreditCard> credit_card) override;
  void OnDismiss() override;
  void OpenAutofillProfileSettings() override;
  void OpenCreditCardSettings() override;
  gfx::NativeView GetNativeView() override;

 protected:
  // Methods below are protected (rather than private) and virtual for
  // testing.

  // Gets or creates (if needed) the FastCheckoutView associated with this
  // controller.
  virtual FastCheckoutView* GetOrCreateView();

 private:
  // Weak pointer to the WebContents this class is tied to.
  const raw_ptr<content::WebContents> web_contents_;

  // View used to communicate with the Android frontend. It's non-null between
  // Show() and OnDismiss()/OnOptionsSelected().
  std::unique_ptr<FastCheckoutView> view_;

  // The delegate of UI events. It must outlive `this`.
  const raw_ptr<Delegate> delegate_;

  base::WeakPtrFactory<FastCheckoutController> weak_ptr_factory_{this};
};
#endif  // CHROME_BROWSER_UI_FAST_CHECKOUT_FAST_CHECKOUT_CONTROLLER_IMPL_H_
