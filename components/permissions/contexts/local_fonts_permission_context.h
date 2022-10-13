// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_CONTEXTS_LOCAL_FONTS_PERMISSION_CONTEXT_H_
#define COMPONENTS_PERMISSIONS_CONTEXTS_LOCAL_FONTS_PERMISSION_CONTEXT_H_

#include "components/permissions/permission_context_base.h"

class LocalFontsPermissionContext : public permissions::PermissionContextBase {
 public:
  explicit LocalFontsPermissionContext(
      content::BrowserContext* browser_context);
  ~LocalFontsPermissionContext() override;

  LocalFontsPermissionContext(const LocalFontsPermissionContext&) = delete;
  LocalFontsPermissionContext& operator=(const LocalFontsPermissionContext&) =
      delete;
};

#endif  // COMPONENTS_PERMISSIONS_CONTEXTS_LOCAL_FONTS_PERMISSION_CONTEXT_H_
