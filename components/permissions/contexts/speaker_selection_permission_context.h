// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_CONTEXTS_SPEAKER_SELECTION_PERMISSION_CONTEXT_H_
#define COMPONENTS_PERMISSIONS_CONTEXTS_SPEAKER_SELECTION_PERMISSION_CONTEXT_H_

#include "components/permissions/permission_context_base.h"

// TODO(https://crbug.com/41492674): speaker selection is not hooked with
// MediaStreamDevicesController yet, which could be when we have permission
// prompt for speaker selection.
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
};

#endif  // COMPONENTS_PERMISSIONS_CONTEXTS_SPEAKER_SELECTION_PERMISSION_CONTEXT_H_
