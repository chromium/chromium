// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/secure_payment_confirmation_service_factory.h"

#include "components/keyed_service/core/service_access_type.h"
#include "components/payments/content/payment_manifest_web_data_service.h"
#include "components/payments/content/secure_payment_confirmation_service.h"
#include "components/webauthn/core/browser/internal_authenticator.h"
#include "components/webdata_services/web_data_service_wrapper_factory.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/secure_payment_confirmation_utils.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/webauthn/android/internal_authenticator_android.h"
#else
#include "components/webauthn/content/browser/internal_authenticator_impl.h"
#endif

namespace payments {

void CreateSecurePaymentConfirmationService(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<mojom::SecurePaymentConfirmationService> receiver) {
  if (!content::IsFrameAllowedToUseSecurePaymentConfirmation(
          render_frame_host)) {
    return;
  }

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);

  // If the frame is allowed to use secure payment confirmation, the render
  // frame host should be non-null and valid. Consequently, the web contents
  // should also be non-null.
  CHECK(web_contents);
  CHECK(render_frame_host);

  std::unique_ptr<webauthn::InternalAuthenticator> maybe_authenticator;
  if (render_frame_host->IsActive() && render_frame_host->IsRenderFrameLive()) {
#if BUILDFLAG(IS_ANDROID)
    maybe_authenticator =
        std::make_unique<webauthn::InternalAuthenticatorAndroid>(
            render_frame_host);
#else
    maybe_authenticator =
        std::make_unique<content::InternalAuthenticatorImpl>(render_frame_host);
#endif
  }

  // The object is bound to the lifetime of |render_frame_host| and the mojo
  // connection. See DocumentService for details.
  new SecurePaymentConfirmationService(
      *render_frame_host, std::move(receiver),
      webdata_services::WebDataServiceWrapperFactory::
          GetPaymentManifestWebDataServiceForBrowserContext(
              web_contents->GetBrowserContext(),
              ServiceAccessType::EXPLICIT_ACCESS),
      std::move(maybe_authenticator));
}

}  // namespace payments
