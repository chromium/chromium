// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_SECURE_PAYMENT_CONFIRMATION_APP_H_
#define COMPONENTS_PAYMENTS_CONTENT_SECURE_PAYMENT_CONFIRMATION_APP_H_

#include "components/payments/content/payment_app.h"

#include <stdint.h>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "components/payments/content/secure_payment_confirmation_controller.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom.h"
#include "url/origin.h"

class SkBitmap;

namespace autofill {
class InternalAuthenticator;
}  // namespace autofill

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace payments {

class PaymentRequestSpec;

class SecurePaymentConfirmationApp : public PaymentApp,
                                     public content::WebContentsObserver {
 public:
  // Please use `std::move()` for the `credential_id` parameter to avoid extra
  // copies.
  SecurePaymentConfirmationApp(
      content::WebContents* web_contents_to_observe,
      const std::string& effective_relying_party_identity,
      std::unique_ptr<SkBitmap> icon,
      const base::string16& label,
      std::vector<uint8_t> credential_id,
      const url::Origin& merchant_origin,
      base::WeakPtr<PaymentRequestSpec> spec,
      mojom::SecurePaymentConfirmationRequestPtr request,
      std::unique_ptr<autofill::InternalAuthenticator> authenticator);
  ~SecurePaymentConfirmationApp() override;

  SecurePaymentConfirmationApp(const SecurePaymentConfirmationApp& other) =
      delete;
  SecurePaymentConfirmationApp& operator=(
      const SecurePaymentConfirmationApp& other) = delete;

  // PaymentApp implementation.
  void InvokePaymentApp(Delegate* delegate) override;
  bool IsCompleteForPayment() const override;
  uint32_t GetCompletenessScore() const override;
  bool CanPreselect() const override;
  base::string16 GetMissingInfoLabel() const override;
  bool HasEnrolledInstrument() const override;
  void RecordUse() override;
  bool NeedsInstallation() const override;
  std::string GetId() const override;
  base::string16 GetLabel() const override;
  base::string16 GetSublabel() const override;
  const SkBitmap* icon_bitmap() const override;
  bool IsValidForModifier(
      const std::string& method,
      bool supported_networks_specified,
      const std::set<std::string>& supported_networks) const override;
  base::WeakPtr<PaymentApp> AsWeakPtr() override;
  bool HandlesShippingAddress() const override;
  bool HandlesPayerName() const override;
  bool HandlesPayerEmail() const override;
  bool HandlesPayerPhone() const override;
  bool IsWaitingForPaymentDetailsUpdate() const override;
  void UpdateWith(
      mojom::PaymentRequestDetailsUpdatePtr details_update) override;
  void OnPaymentDetailsNotUpdated() override;
  void AbortPaymentApp(base::OnceCallback<void(bool)> abort_callback) override;

  // WebContentsObserver implementation.
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;

 private:
  void OnGetAssertion(
      Delegate* delegate,
      blink::mojom::AuthenticatorStatus status,
      blink::mojom::GetAssertionAuthenticatorResponsePtr response);

  // Used only for comparison with the RenderFrameHost pointer in
  // RenderFrameDeleted() method.
  const content::RenderFrameHost* const
      authenticator_render_frame_host_pointer_do_not_dereference_;

  const std::string effective_relying_party_identity_;
  const std::unique_ptr<SkBitmap> icon_;
  const base::string16 label_;
  const std::vector<uint8_t> credential_id_;
  const std::string encoded_credential_id_;
  const url::Origin merchant_origin_;
  const base::WeakPtr<PaymentRequestSpec> spec_;
  const mojom::SecurePaymentConfirmationRequestPtr request_;
  std::unique_ptr<autofill::InternalAuthenticator> authenticator_;
  std::string challenge_;

  base::WeakPtrFactory<SecurePaymentConfirmationApp> weak_ptr_factory_{this};
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_SECURE_PAYMENT_CONFIRMATION_APP_H_
