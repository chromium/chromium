// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_CONTEXTS_SPEAKER_SELECTION_PERMISSION_CONTEXT_H_
#define COMPONENTS_PERMISSIONS_CONTEXTS_SPEAKER_SELECTION_PERMISSION_CONTEXT_H_

#include "components/permissions/permission_context_base.h"

namespace permissions {

class SpeakerSelectionPermissionContext
    : public permissions::PermissionContextBase {
 public:
  explicit SpeakerSelectionPermissionContext(
      content::BrowserContext* browser_context);
  ~SpeakerSelectionPermissionContext() override = default;

  SpeakerSelectionPermissionContext(const SpeakerSelectionPermissionContext&) =
      delete;
  SpeakerSelectionPermissionContext& operator=(
      const SpeakerSelectionPermissionContext&) = delete;

 protected:
  ContentSetting GetPermissionStatusInternal(
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      const GURL& embedding_origin) const override;

  void DecidePermission(
      permissions::PermissionRequestData request_data,
      permissions::BrowserPermissionCallback callback) override;

  void UpdateContentSetting(const GURL& requesting_origin,
                            const GURL& embedding_origin,
                            ContentSetting content_setting,
                            bool is_one_time) override;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_CONTEXTS_SPEAKER_SELECTION_PERMISSION_CONTEXT_H_
