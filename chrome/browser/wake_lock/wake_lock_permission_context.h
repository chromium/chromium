// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WAKE_LOCK_WAKE_LOCK_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_WAKE_LOCK_WAKE_LOCK_PERMISSION_CONTEXT_H_

#include "base/macros.h"
#include "chrome/browser/permissions/permission_context_base.h"
#include "components/content_settings/core/common/content_settings_types.h"

class WakeLockPermissionContext : public PermissionContextBase {
 public:
  WakeLockPermissionContext(Profile* profile,
                            ContentSettingsType content_settings_type);

  ~WakeLockPermissionContext() override;

 private:
  // PermissionContextBase:
  ContentSetting GetPermissionStatusInternal(
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      const GURL& embedding_origin) const override;
  bool IsRestrictedToSecureOrigins() const override;

  ContentSettingsType content_settings_type_;

  DISALLOW_COPY_AND_ASSIGN(WakeLockPermissionContext);
};

#endif  // CHROME_BROWSER_WAKE_LOCK_WAKE_LOCK_PERMISSION_CONTEXT_H_
