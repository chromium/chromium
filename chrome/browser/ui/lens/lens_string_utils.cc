// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_string_utils.h"

#include "chrome/grit/branded_strings.h"
#include "components/lens/lens_features.h"

namespace lens {

int GetLensOverlayEntrypointLabelAltIds(int default_value) {
  if (!base::FeatureList::IsEnabled(
          lens::features::kLensOverlayEntrypointLabelAlt)) {
    return default_value;
  }
  switch (lens::features::kLensOverlayEntrypointLabelAltId.Get()) {
    case 1:
      return IDS_LENS_OVERLAY_ENTRYPOINT_LABEL_ALT1;
    case 2:
      return IDS_LENS_OVERLAY_ENTRYPOINT_LABEL_ALT2;
    case 3:
      return IDS_LENS_OVERLAY_ENTRYPOINT_LABEL_ALT3;
    case 4:
      return IDS_LENS_OVERLAY_TAB_ENTRYPOINT_LABEL;
    default:
      return default_value;
  }
}

int GetLensOverlayImageEntrypointLabelAltIds(int default_value) {
  if (!base::FeatureList::IsEnabled(
          lens::features::kLensOverlayEntrypointLabelAlt)) {
    return default_value;
  }
  switch (lens::features::kLensOverlayEntrypointLabelAltId.Get()) {
    case 1:
      return IDS_LENS_OVERLAY_IMAGE_ENTRYPOINT_LABEL_ALT1;
    case 2:
      return IDS_LENS_OVERLAY_IMAGE_ENTRYPOINT_LABEL_ALT2;
    case 3:
    case 4:
      return IDS_LENS_OVERLAY_IMAGE_ENTRYPOINT_LABEL_ALT3;
    default:
      return default_value;
  }
}

int GetLensOverlayVideoEntrypointLabelAltIds(int default_value) {
  if (!base::FeatureList::IsEnabled(
          lens::features::kLensOverlayEntrypointLabelAlt)) {
    return default_value;
  }
  switch (lens::features::kLensOverlayEntrypointLabelAltId.Get()) {
    case 1:
      return IDS_LENS_OVERLAY_VIDEO_ENTRYPOINT_LABEL_ALT1;
    case 2:
      return IDS_LENS_OVERLAY_VIDEO_ENTRYPOINT_LABEL_ALT2;
    case 3:
    case 4:
      return IDS_LENS_OVERLAY_VIDEO_ENTRYPOINT_LABEL_ALT3;
    default:
      return default_value;
  }
}

}  // namespace lens
