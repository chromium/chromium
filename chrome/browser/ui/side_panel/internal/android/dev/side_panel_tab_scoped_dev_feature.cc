// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/side_panel/internal/android/dev/side_panel_tab_scoped_dev_feature.h"

#include <memory>

#include "base/functional/bind.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/ui/side_panel/android/side_panel_native_view_android.h"
#include "chrome/browser/ui/side_panel/internal/android/side_panel_coordinator_android.h"
#include "chrome/browser/ui/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_key.h"
#include "chrome/browser/ui/side_panel/side_panel_native_view.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "third_party/jni_zero/jni_zero.h"

// Must come after headers that provide symbols used by @JniType.
#include "chrome/browser/ui/side_panel/internal/android/jni_headers/SidePanelTabScopedDevFeatureImpl_jni.h"

namespace {
SidePanelNativeView CreateSidePanelDevView(tabs::TabInterface* tab,
                                           SidePanelEntryScope& scope) {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  TabAndroid* tab_android = static_cast<TabAndroid*>(tab);
  if (!tab_android) {
    return nullptr;
  }

  auto view = Java_SidePanelTabScopedDevFeatureImpl_createTabScopedView(
      env, tab_android->GetJavaObject());
  return std::make_unique<SidePanelNativeViewAndroid>(view);
}
}  // namespace

SidePanelTabScopedDevFeature::SidePanelTabScopedDevFeature(
    tabs::TabInterface* tab,
    SidePanelRegistry* registry)
    : tab_(tab), registry_(registry) {
  if (registry_) {
    auto entry = std::make_unique<SidePanelEntry>(
        SidePanelType::kToolbar,
        SidePanelEntry::Key(SidePanelEntry::Id::kSidePanelDev),
        base::BindRepeating(&CreateSidePanelDevView, tab_),
        base::BindRepeating([]() { return 0; }));
    entry->SetProperty(kSidePanelTitleKey, std::u16string(u"Developer Panel"));
    registry_->Register(std::move(entry));
  }
}

SidePanelTabScopedDevFeature::~SidePanelTabScopedDevFeature() = default;

void JNI_SidePanelTabScopedDevFeatureImpl_ToggleTabScopedDevFeature(
    JNIEnv* env,
    TabAndroid* tab_android) {
  CHECK(tab_android);

  tabs::TabInterface* tab = tab_android;
  auto* side_panel_coordinator =
      SidePanelCoordinatorAndroid::From(tab->GetBrowserWindowInterface());
  if (side_panel_coordinator) {
    side_panel_coordinator->Toggle(
        SidePanelEntryKey(SidePanelEntry::Id::kSidePanelDev),
        SidePanelOpenTrigger::kToolbarButton);
  }
}

DEFINE_JNI(SidePanelTabScopedDevFeatureImpl)
