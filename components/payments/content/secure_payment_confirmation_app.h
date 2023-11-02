// Copyright 2020 The Chromium Authors
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
#include "components/payments/content/secure_payment_confirmation_controller.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom-forward.h"
#include "url/origin.h"

class SkBitmap;

namespace webauthn {
class InternalAuthenticator;
}  // namespace webauthn

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace payments {

class PaymentRequestSpec;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Keep in sync with
// src/tools/metrics/histograms/enums.xml.
enum class SecurePaymentConfirmationSystemPromptResult {
  kCanceled = 0,
  kAccepted = 1,
  kMaxValue = kAccepted,
};

class SecurePaymentConfirmationApp : public PaymentApp,
                                     public content::WebContentsObserver {
 public:
  // Please use `std::move()` for the `credential_id` parameter to avoid extra
  // copies.
  SecurePaymentConfirmationApp(
      content::WebContents* web_contents_to_observe,
      const std::string& effective_relying_party_identity,
      std::unique_ptr<SkBitmap> icon,
      const std::u16string& label,
      std::vector<uint8_t> credential_id,
      const url::Origin& merchant_origin,
      base::WeakPtr<PaymentRequestSpec> spec,
      mojom::SecurePaymentConfirmationRequestPtr request,
      std::unique_ptr<webauthn::InternalAuthenticator> authenticator);
  ~SecurePaymentConfirmationApp() override;

  SecurePaymentConfirmationApp(const SecurePaymentConfirmationApp& other) =
      delete;
  SecurePaymentConfirmationApp& operator=(
      const SecurePaymentConfirmationApp& other) = delete;

  // PaymentApp implementation.
  void InvokePaymentApp(base::WeakPtr<Delegate> delegate) override;
  bool IsCompleteForPayment() const override;
  bool CanPreselect() const override;
  std::u16string GetMissingInfoLabel() const override;
  bool HasEnrolledInstrument() const override;
  void RecordUse() override;
  bool NeedsInstallation() const override;
  std::string GetId() const override;
  std::u16string GetLabel() const override;
  std::u16string GetSublabel() const override;
  const SkBitmap* icon_bitmap() const override;
  bool IsValidForModifier(const std::string& method) const override;
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
  mojom::PaymentResponsePtr SetAppSpecificResponseFields(
      mojom::PaymentResponsePtr response) const override;

  // WebContentsObserver implementation.
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;

 private:
  void OnGetAssertion(
      base::WeakPtr<Delegate> delegate,
      blink::mojom::AuthenticatorStatus status,
      blink::mojom::GetAssertionAuthenticatorResponsePtr response,
      blink::mojom::WebAuthnDOMExceptionDetailsPtr dom_exception_details);

  // Used only for comparison with the RenderFrameHost pointer in
  // RenderFrameDeleted() method.
  content::GlobalRenderFrameHostId authenticator_frame_routing_id_;

  const std::string effective_relying_party_identity_;
  const std::unique_ptr<SkBitmap> icon_;
  const std::u16string label_;
  const std::vector<uint8_t> credential_id_;
  const url::Origin merchant_origin_;
  const base::WeakPtr<PaymentRequestSpec> spec_;
  const mojom::SecurePaymentConfirmationRequestPtr request_;
  std::unique_ptr<webauthn::InternalAuthenticator> authenticator_;
  std::string challenge_;
  blink::mojom::GetAssertionAuthenticatorResponsePtr response_;

  base::WeakPtrFactory<SecurePaymentConfirmationApp> weak_ptr_factory_{this};
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_SECURE_PAYMENT_CONFIRMATION_APP_H_
