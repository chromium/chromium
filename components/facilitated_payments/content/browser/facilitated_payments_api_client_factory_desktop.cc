// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "components/facilitated_payments/content/browser/facilitated_payments_api_client_factory.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_api_client.h"
#include "content/public/browser/global_routing_id.h"

namespace payments::facilitated {

namespace {

// Desktop has no payments API client: it only runs payment QR code detection,
// which never calls the API.
FacilitatedPaymentsApiClientCreator CreateNullApiClientCreator() {
  return base::BindRepeating(
      []() -> std::unique_ptr<FacilitatedPaymentsApiClient> {
        return nullptr;
      });
}

}  // namespace

FacilitatedPaymentsApiClientCreator GetFacilitatedPaymentsApiClientCreator(
    content::GlobalRenderFrameHostId render_frame_host_id) {
  return CreateNullApiClientCreator();
}

FacilitatedPaymentsApiClientCreator GetFacilitatedPaymentsApiClientCreator(
    base::WeakPtr<content::WebContents> web_contents) {
  return CreateNullApiClientCreator();
}

}  // namespace payments::facilitated
