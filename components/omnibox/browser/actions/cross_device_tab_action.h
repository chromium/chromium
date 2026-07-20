// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_ACTIONS_CROSS_DEVICE_TAB_ACTION_H_
#define COMPONENTS_OMNIBOX_BROWSER_ACTIONS_CROSS_DEVICE_TAB_ACTION_H_

#include "base/time/time.h"
#include "build/build_config.h"
#include "components/omnibox/browser/actions/omnibox_action.h"
#include "components/omnibox/browser/actions/omnibox_action_concepts.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/scoped_java_ref.h"
#endif

namespace gfx {
struct VectorIcon;
}  // namespace gfx

class CrossDeviceTabAction : public OmniboxAction {
 public:
  explicit CrossDeviceTabAction(base::Time tab_last_active_time);

  base::Time tab_last_active_time() const { return tab_last_active_time_; }

  // OmniboxAction:
  // Note: `RecordActionShown()` is not overridden here because we log more
  // detailed interaction metrics (including latency and age) in
  // `CrossDeviceTabProvider::RecordInteractionMetrics()`, which has access to
  // `OmniboxLog`.
  OmniboxActionId ActionId() const override;
#if BUILDFLAG(IS_ANDROID)
  base::android::ScopedJavaLocalRef<jobject> GetOrCreateJavaObject(
      JNIEnv* env) const override;
#endif
#if defined(SUPPORT_PEDALS_VECTOR_ICONS)
  const gfx::VectorIcon& GetVectorIcon() const override;
#endif  // defined(SUPPORT_PEDALS_VECTOR_ICONS)

 private:
  ~CrossDeviceTabAction() override;

  // The timestamp of the tab (when it was last active on the remote device).
  const base::Time tab_last_active_time_;

#if BUILDFLAG(IS_ANDROID)
  mutable base::android::ScopedJavaGlobalRef<jobject> j_omnibox_action_;
#endif
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_ACTIONS_CROSS_DEVICE_TAB_ACTION_H_
