// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/contexts/payment_handler_permission_context.h"

#include "base/notreached.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permission_request_id.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom.h"
#include "url/gurl.h"

namespace payments {

PaymentHandlerPermissionContext::PaymentHandlerPermissionContext(
    content::BrowserContext* browser_context)
    : PermissionContextBase(browser_context,
                            ContentSettingsType::PAYMENT_HANDLER,
                            blink::mojom::PermissionsPolicyFeature::kNotFound) {
}

PaymentHandlerPermissionContext::~PaymentHandlerPermissionContext() = default;

void PaymentHandlerPermissionContext::DecidePermission(
    permissions::PermissionRequestData request_data,
    permissions::BrowserPermissionCallback callback) {
  // The user should never be prompted to authorize payment handler.
  NOTREACHED_IN_MIGRATION();
}

}  // namespace payments
