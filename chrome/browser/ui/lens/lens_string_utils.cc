// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_string_utils.h"

#include "base/feature_list.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/grit/branded_strings.h"
#include "components/contextual_tasks/public/features.h"

namespace lens {

int GetLensOverlayEntrypointLabelAltIds(bool is_context_menu) {
  if (::features::IsMenuSimplificationEnabled() ||
      base::FeatureList::IsEnabled(
          contextual_tasks::kContextualTasksUpdatedEntryPoints)) {
    return IDS_LENS_OVERLAY_TAB_ENTRYPOINT_LABEL_V2;
  }
  return is_context_menu ? IDS_LENS_OVERLAY_TAB_ENTRYPOINT_LABEL_CONTEXT_MENU
                         : IDS_LENS_OVERLAY_TAB_ENTRYPOINT_LABEL;
}

}  // namespace lens
