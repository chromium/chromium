// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/actions/cross_device_tab_action.h"

#include "base/time/time.h"
#include "components/omnibox/browser/actions/omnibox_action.h"
#include "components/omnibox/browser/actions/omnibox_action_concepts.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/ui_base_features.h"
#include "url/gurl.h"

#if defined(SUPPORT_PEDALS_VECTOR_ICONS)
#include "components/omnibox/browser/vector_icons.h"  // nogncheck
#include "components/vector_icons/vector_icons.h"     // nogncheck
#endif  // defined(SUPPORT_PEDALS_VECTOR_ICONS)

#if BUILDFLAG(IS_ANDROID)
#include "components/omnibox/browser/actions/omnibox_action_factory_android.h"
#endif

CrossDeviceTabAction::CrossDeviceTabAction(base::Time tab_last_active_time)
    : OmniboxAction(
          LabelStrings(IDS_OMNIBOX_ACTION_CROSS_DEVICE_TAB_HINT,
                       IDS_OMNIBOX_ACTION_CROSS_DEVICE_TAB_SUGGESTION_CONTENTS,
                       IDS_ACC_OMNIBOX_ACTION_CROSS_DEVICE_TAB_SUFFIX,
                       IDS_ACC_OMNIBOX_ACTION_CROSS_DEVICE_TAB),
          GURL("chrome://history/syncedTabs")),
      tab_last_active_time_(tab_last_active_time) {}

CrossDeviceTabAction::~CrossDeviceTabAction() = default;

OmniboxActionId CrossDeviceTabAction::ActionId() const {
  return OmniboxActionId::CROSS_DEVICE_TAB;
}

#if BUILDFLAG(IS_ANDROID)
base::android::ScopedJavaLocalRef<jobject>
CrossDeviceTabAction::GetOrCreateJavaObject(JNIEnv* env) const {
  if (!j_omnibox_action_) {
    j_omnibox_action_.Reset(
        BuildCrossDeviceTabAction(env, reinterpret_cast<intptr_t>(this),
                                  strings_.hint, strings_.accessibility_hint));
  }
  return base::android::ScopedJavaLocalRef<jobject>(j_omnibox_action_);
}
#endif

#if defined(SUPPORT_PEDALS_VECTOR_ICONS)
const gfx::VectorIcon& CrossDeviceTabAction::GetVectorIcon() const {
  return features::IsRoundedIconsEnabled() ? vector_icons::kDevicesIcon
                                           : vector_icons::kDevicesOldIcon;
}
#endif  // defined(SUPPORT_PEDALS_VECTOR_ICONS)
