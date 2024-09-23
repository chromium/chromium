// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SHARING_MESSAGE_SHARING_DIALOG_DATA_H_
#define COMPONENTS_SHARING_MESSAGE_SHARING_DIALOG_DATA_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "components/sharing_message/sharing_app.h"
#include "components/sharing_message/sharing_metrics.h"
#include "components/sharing_message/sharing_target_device_info.h"
#include "url/origin.h"

class SharingDialog;

namespace gfx {
struct VectorIcon;
}  // namespace gfx

// All data required to display a SharingDialog.
struct SharingDialogData {
 public:
  // TODO(crbug.com/40102679): Merge both images using alpha blending so they
  // work on any background color.
  struct HeaderIcons {
    HeaderIcons(const gfx::VectorIcon* light, const gfx::VectorIcon* dark);
    raw_ptr<const gfx::VectorIcon> light;
    raw_ptr<const gfx::VectorIcon> dark;
  };
  SharingDialogData();
  ~SharingDialogData();
  SharingDialogData(SharingDialogData&& other);
  SharingDialogData& operator=(SharingDialogData&& other);

  SharingDialogType type = SharingDialogType::kErrorDialog;
  SharingFeatureName prefix = SharingFeatureName::kUnknown;

  std::vector<SharingTargetDeviceInfo> devices;
  std::vector<SharingApp> apps;

  std::u16string title;
  std::u16string error_text;
  int help_text_id = 0;
  int help_text_origin_id = 0;
  std::optional<HeaderIcons> header_icons;
  int origin_text_id = 0;
  std::optional<url::Origin> initiating_origin;

  base::OnceCallback<void(const SharingTargetDeviceInfo&)> device_callback;
  base::OnceCallback<void(const SharingApp&)> app_callback;
  base::OnceCallback<void(SharingDialog*)> close_callback;
};

#endif  // COMPONENTS_SHARING_MESSAGE_SHARING_DIALOG_DATA_H_
