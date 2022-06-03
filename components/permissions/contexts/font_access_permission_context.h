// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_CONTEXTS_FONT_ACCESS_PERMISSION_CONTEXT_H_
#define COMPONENTS_PERMISSIONS_CONTEXTS_FONT_ACCESS_PERMISSION_CONTEXT_H_

#include "components/permissions/permission_context_base.h"

class FontAccessPermissionContext : public permissions::PermissionContextBase {
 public:
  explicit FontAccessPermissionContext(
      content::BrowserContext* browser_context);
  ~FontAccessPermissionContext() override;

  FontAccessPermissionContext(const FontAccessPermissionContext&) = delete;
  FontAccessPermissionContext& operator=(const FontAccessPermissionContext&) =
      delete;

 protected:
  bool IsRestrictedToSecureOrigins() const override;
};

#endif  // COMPONENTS_PERMISSIONS_CONTEXTS_FONT_ACCESS_PERMISSION_CONTEXT_H_
