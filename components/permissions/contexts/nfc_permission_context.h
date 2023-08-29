// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_CONTEXTS_NFC_PERMISSION_CONTEXT_H_
#define COMPONENTS_PERMISSIONS_CONTEXTS_NFC_PERMISSION_CONTEXT_H_

#include "build/build_config.h"
#include "components/permissions/permission_context_base.h"

namespace permissions {

class NfcPermissionContext : public PermissionContextBase {
 public:
  // The delegate allows embedders to modify the permission context logic.
  class Delegate {
   public:
    virtual ~Delegate() = default;

#if BUILDFLAG(IS_ANDROID)
    // Returns whether or not this |web_contents| is interactable.
    virtual bool IsInteractable(content::WebContents* web_contents) = 0;
#endif
  };

  NfcPermissionContext(content::BrowserContext* browser_context,
                       std::unique_ptr<Delegate> delegate);

  NfcPermissionContext(const NfcPermissionContext&) = delete;
  NfcPermissionContext& operator=(const NfcPermissionContext&) = delete;

  ~NfcPermissionContext() override;

 protected:
  std::unique_ptr<Delegate> delegate_;

 private:
  // PermissionContextBase:
#if !BUILDFLAG(IS_ANDROID)
  ContentSetting GetPermissionStatusInternal(
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      const GURL& embedding_origin) const override;
#endif
  void DecidePermission(PermissionRequestData request_data,
                        BrowserPermissionCallback callback) override;
  void UpdateTabContext(const PermissionRequestID& id,
                        const GURL& requesting_frame,
                        bool allowed) override;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_CONTEXTS_NFC_PERMISSION_CONTEXT_H_
