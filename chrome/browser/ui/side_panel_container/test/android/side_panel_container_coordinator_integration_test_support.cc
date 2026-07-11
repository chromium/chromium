// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/check.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/ui/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/side_panel/side_panel_ui_provider.h"
#include "chrome/browser/ui/side_panel_container/test/test_support_jni_headers/SidePanelContainerCoordinatorIntegrationTestSupport_jni.h"

static void
JNI_SidePanelContainerCoordinatorIntegrationTestSupport_ShowSidePanel(
    JNIEnv* env,
    TabAndroid* tab,
    bool suppress_animations) {
  CHECK(tab);

  auto* side_panel_ui =
      SidePanelUIProvider::From(tab->GetBrowserWindowInterface());
  CHECK(side_panel_ui);

  side_panel_ui->Show(SidePanelEntry::Key(SidePanelEntry::Id::kSidePanelDev),
                      SidePanelOpenTrigger::kToolbarButton,
                      suppress_animations);
}

static void
JNI_SidePanelContainerCoordinatorIntegrationTestSupport_CloseSidePanel(
    JNIEnv* env,
    TabAndroid* tab,
    bool suppress_animations) {
  CHECK(tab);

  auto* side_panel_ui =
      SidePanelUIProvider::From(tab->GetBrowserWindowInterface());
  CHECK(side_panel_ui);

  side_panel_ui->Close(SidePanelEntryHideReason::kSidePanelClosed,
                       suppress_animations);
}

DEFINE_JNI(SidePanelContainerCoordinatorIntegrationTestSupport)
