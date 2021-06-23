// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_credential_factory.h"

#include "components/keyed_service/core/service_access_type.h"
#include "components/payments/content/payment_credential.h"
#include "components/payments/content/payment_credential_manager.h"
#include "components/payments/content/payment_manifest_web_data_service.h"
#include "components/webdata_services/web_data_service_wrapper_factory.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"

namespace payments {

void CreatePaymentCredential(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<mojom::PaymentCredential> receiver) {
  if (!PaymentCredential::IsFrameAllowedToUseSecurePaymentConfirmation(
          render_frame_host))
    return;

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  if (!web_contents)
    return;

  content::GlobalRenderFrameHostId initiator_frame_routing_id =
      render_frame_host->GetProcess()
          ? content::GlobalRenderFrameHostId(
                render_frame_host->GetProcess()->GetID(),
                render_frame_host->GetRoutingID())
          : content::GlobalRenderFrameHostId();
  PaymentCredentialManager::GetOrCreateForWebContents(web_contents)
      ->CreatePaymentCredential(
          initiator_frame_routing_id,
          webdata_services::WebDataServiceWrapperFactory::
              GetPaymentManifestWebDataServiceForBrowserContext(
                  web_contents->GetBrowserContext(),
                  ServiceAccessType::EXPLICIT_ACCESS),
          std::move(receiver));
}

}  // namespace payments
