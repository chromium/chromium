// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_QUOTA_PERMISSION_CONTEXT_IMPL_H_
#define COMPONENTS_PERMISSIONS_QUOTA_PERMISSION_CONTEXT_IMPL_H_

#include "content/public/browser/quota_permission_context.h"

namespace permissions {

class QuotaPermissionContextImpl : public content::QuotaPermissionContext {
 public:
  QuotaPermissionContextImpl();

  // The callback will be dispatched on the IO thread.
  void RequestQuotaPermission(const content::StorageQuotaParams& params,
                              int render_process_id,
                              PermissionCallback callback) override;

  void DispatchCallbackOnIOThread(PermissionCallback callback,
                                  QuotaPermissionResponse response);

 private:
  ~QuotaPermissionContextImpl() override;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_QUOTA_PERMISSION_CONTEXT_IMPL_H_
