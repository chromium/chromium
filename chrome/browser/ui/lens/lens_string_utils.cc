// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_string_utils.h"

#include "chrome/browser/ui/ui_features.h"
#include "chrome/grit/branded_strings.h"

namespace lens {

int GetLensOverlayEntrypointLabelAltIds() {
  if (::features::IsMenuSimplificationEnabled()) {
    return IDS_LENS_OVERLAY_TAB_ENTRYPOINT_LABEL_V2;
  }
  return IDS_LENS_OVERLAY_TAB_ENTRYPOINT_LABEL;
}

}  // namespace lens
