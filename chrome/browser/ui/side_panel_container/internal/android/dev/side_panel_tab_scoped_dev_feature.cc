// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/side_panel_container/internal/android/dev/side_panel_tab_scoped_dev_feature.h"

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/bind.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/ui/side_panel/android/side_panel_native_view_android.h"
#include "chrome/browser/ui/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_key.h"
#include "chrome/browser/ui/side_panel/side_panel_native_view.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/side_panel/side_panel_ui_provider.h"
#include "chrome/browser/ui/side_panel_container/internal/jni_headers/SidePanelTabScopedDevFeatureImpl_jni.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"

namespace {
SidePanelNativeView CreateSidePanelDevView(tabs::TabInterface* tab,
                                           SidePanelEntryScope& scope) {
  JNIEnv* env = base::android::AttachCurrentThread();
  TabAndroid* tab_android = static_cast<TabAndroid*>(tab);
  if (!tab_android) {
    return nullptr;
  }

  auto view = Java_SidePanelTabScopedDevFeatureImpl_createTabScopedView(
      env, tab_android->GetJavaObject());
  return std::make_unique<SidePanelNativeViewAndroid>(
      base::android::ScopedJavaGlobalRef<jobject>(env, view));
}
}  // namespace

SidePanelTabScopedDevFeature::SidePanelTabScopedDevFeature(
    tabs::TabInterface* tab,
    SidePanelRegistry* registry)
    : tab_(tab), registry_(registry) {
  if (registry_) {
    registry_->Register(std::make_unique<SidePanelEntry>(
        SidePanelType::kToolbar,
        SidePanelEntry::Key(SidePanelEntry::Id::kSidePanelDev),
        base::BindRepeating(&CreateSidePanelDevView, tab_),
        base::BindRepeating([]() { return 0; })));
  }
}

SidePanelTabScopedDevFeature::~SidePanelTabScopedDevFeature() = default;

void JNI_SidePanelTabScopedDevFeatureImpl_ToggleTabScopedDevFeature(
    JNIEnv* env,
    TabAndroid* tab_android) {
  CHECK(tab_android);

  tabs::TabInterface* tab = tab_android;
  auto* side_panel_ui =
      SidePanelUIProvider::From(tab->GetBrowserWindowInterface());
  if (side_panel_ui) {
    side_panel_ui->Toggle(SidePanelEntryKey(SidePanelEntry::Id::kSidePanelDev),
                          SidePanelOpenTrigger::kToolbarButton);
  }
}

DEFINE_JNI(SidePanelTabScopedDevFeatureImpl)
