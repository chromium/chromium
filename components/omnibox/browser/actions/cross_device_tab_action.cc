// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/actions/cross_device_tab_action.h"

#include "components/omnibox/browser/actions/omnibox_action.h"
#include "components/omnibox/browser/actions/omnibox_action_concepts.h"
#include "components/strings/grit/components_strings.h"
#include "url/gurl.h"

#if defined(SUPPORT_PEDALS_VECTOR_ICONS)
#include "components/omnibox/browser/vector_icons.h"  // nogncheck
#include "components/vector_icons/vector_icons.h"     // nogncheck
#endif  // defined(SUPPORT_PEDALS_VECTOR_ICONS)

CrossDeviceTabAction::CrossDeviceTabAction()
    : OmniboxAction(
          LabelStrings(IDS_OMNIBOX_ACTION_CROSS_DEVICE_TAB_HINT,
                       IDS_OMNIBOX_ACTION_CROSS_DEVICE_TAB_SUGGESTION_CONTENTS,
                       IDS_ACC_OMNIBOX_ACTION_CROSS_DEVICE_TAB_SUFFIX,
                       IDS_ACC_OMNIBOX_ACTION_CROSS_DEVICE_TAB),
          GURL("chrome://history/syncedTabs")) {}

CrossDeviceTabAction::~CrossDeviceTabAction() = default;

void CrossDeviceTabAction::RecordActionShown(size_t position,
                                             bool executed) const {
  // TODO(crbug.com/508162292): Record metrics.
}

OmniboxActionId CrossDeviceTabAction::ActionId() const {
  return OmniboxActionId::CROSS_DEVICE_TAB;
}

#if defined(SUPPORT_PEDALS_VECTOR_ICONS)
const gfx::VectorIcon& CrossDeviceTabAction::GetVectorIcon() const {
  return vector_icons::kDevicesIcon;
}
#endif  // defined(SUPPORT_PEDALS_VECTOR_ICONS)
