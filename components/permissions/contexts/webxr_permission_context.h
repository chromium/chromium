// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_CONTEXTS_WEBXR_PERMISSION_CONTEXT_H_
#define COMPONENTS_PERMISSIONS_CONTEXTS_WEBXR_PERMISSION_CONTEXT_H_

#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permission_context_base.h"

namespace permissions {
class WebXrPermissionContext : public PermissionContextBase {
 public:
  WebXrPermissionContext(content::BrowserContext* browser_context,
                         ContentSettingsType content_settings_type);
  ~WebXrPermissionContext() override;
  WebXrPermissionContext(const WebXrPermissionContext&) = delete;
  WebXrPermissionContext& operator=(const WebXrPermissionContext&) = delete;

 private:
  // PermissionContextBase:
#if BUILDFLAG(IS_ANDROID)
  // On Android we need to do some additional checking for OS level permissions,
  // which do not need to happen on Desktop. Note that NotifyPermissionSet is
  // only called after a "RequestPermission" call (and not if we are just
  // checking the state of the permission), however, the requestSession flow
  // requires checking the permission as one of it's steps: (5.6 as of 03/10/20)
  // https://immersive-web.github.io/webxr/#dom-xrsystem-requestsession
  // When implementing navigator.xr.permission methods, we should ensure that
  // GetPermissionStatus is also updated to check these permissions.
  void NotifyPermissionSet(const PermissionRequestID& id,
                           const GURL& requesting_origin,
                           const GURL& embedding_origin,
                           BrowserPermissionCallback callback,
                           bool persist,
                           ContentSetting content_setting,
                           bool is_one_time,
                           bool is_final_decision) override;

  void UpdateTabContext(const permissions::PermissionRequestID& id,
                        const GURL& requesting_origin,
                        bool allowed) override;

  void OnAndroidPermissionDecided(const PermissionRequestID& id,
                                  const GURL& requesting_origin,
                                  const GURL& embedding_origin,
                                  BrowserPermissionCallback callback,
                                  bool permission_granted);
#endif

  ContentSettingsType content_settings_type_;

  // Must be the last member, to ensure that it will be
  // destroyed first, which will invalidate weak pointers
  base::WeakPtrFactory<WebXrPermissionContext> weak_ptr_factory_{this};
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_CONTEXTS_WEBXR_PERMISSION_CONTEXT_H_
