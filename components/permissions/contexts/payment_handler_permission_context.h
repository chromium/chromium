// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_CONTEXTS_PAYMENT_HANDLER_PERMISSION_CONTEXT_H_
#define COMPONENTS_PERMISSIONS_CONTEXTS_PAYMENT_HANDLER_PERMISSION_CONTEXT_H_

#include "components/permissions/permission_context_base.h"

namespace content {
class WebContents;
}  // namespace content

namespace payments {

class PaymentHandlerPermissionContext
    : public permissions::PermissionContextBase {
 public:
  explicit PaymentHandlerPermissionContext(
      content::BrowserContext* browser_context);

  PaymentHandlerPermissionContext(const PaymentHandlerPermissionContext&) =
      delete;
  PaymentHandlerPermissionContext& operator=(
      const PaymentHandlerPermissionContext&) = delete;

  ~PaymentHandlerPermissionContext() override;

 private:
  // PermissionContextBase
  void DecidePermission(
      permissions::PermissionRequestData request_data,
      permissions::BrowserPermissionCallback callback) override;
};

}  // namespace payments

#endif  // COMPONENTS_PERMISSIONS_CONTEXTS_PAYMENT_HANDLER_PERMISSION_CONTEXT_H_
