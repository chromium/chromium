// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/android/jni_android.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/side_panel/android/side_panel_native_view_android.h"
#include "chrome/browser/ui/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

// Must come after headers that provide symbols used by @JniType.
#include "chrome/browser/ui/side_panel/test/android/integration_test_support_jni_headers/SidePanelContainerCoordinatorIntegrationTestSupport_jni.h"

namespace {

std::unique_ptr<SidePanelNativeViewAndroid> CreateTestEntryView(
    TabAndroid* tab,
    SidePanelEntryScope& scope) {
  return std::make_unique<SidePanelNativeViewAndroid>(
      Java_SidePanelContainerCoordinatorIntegrationTestSupport_createTestView(
          jni_zero::AttachCurrentThread(), tab->GetJavaObject()));
}

}  // namespace

static void
JNI_SidePanelContainerCoordinatorIntegrationTestSupport_ShowDevSidePanel(
    JNIEnv* env,
    TabAndroid* tab,
    bool suppress_animations) {
  CHECK(tab);

  auto* side_panel_ui = SidePanelUI::From(tab->GetBrowserWindowInterface());
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

  auto* side_panel_ui = SidePanelUI::From(tab->GetBrowserWindowInterface());
  CHECK(side_panel_ui);

  side_panel_ui->Close(SidePanelEntryHideReason::kSidePanelClosed,
                       suppress_animations);
}

static void
JNI_SidePanelContainerCoordinatorIntegrationTestSupport_ShowTestEntrySidePanel(
    JNIEnv* env,
    TabAndroid* tab,
    bool suppress_animations) {
  CHECK(tab);

  // The entry is tab-scoped, so the tab's registry owns it, and destroys it
  // before the tab.
  SidePanelRegistry* registry = SidePanelRegistry::From(tab);
  CHECK(registry);
  // Use an ID different from the dev feature's entry ID.
  SidePanelEntry::Key key(SidePanelEntryId::kTestTabScopedEntry);
  if (!registry->GetEntryForKey(key)) {
    auto entry = std::make_unique<SidePanelEntry>(
        SidePanelType::kToolbar, key,
        base::BindRepeating(&CreateTestEntryView, base::Unretained(tab)),
        base::BindRepeating([]() { return 0; }));
    entry->SetProperty(kSidePanelTitleKey, std::u16string(u"Test Entry Panel"));
    registry->Register(std::move(entry));
  }

  auto* side_panel_ui = SidePanelUI::From(tab->GetBrowserWindowInterface());
  CHECK(side_panel_ui);
  side_panel_ui->Show(key, SidePanelOpenTrigger::kToolbarButton,
                      suppress_animations);
}

static void
JNI_SidePanelContainerCoordinatorIntegrationTestSupport_ClearPersistedPanelWidths(
    JNIEnv* env,
    Profile* profile) {
  CHECK(profile);
  profile->GetPrefs()->ClearPref(prefs::kSidePanelIdToWidth);
}

static void
JNI_SidePanelContainerCoordinatorIntegrationTestSupport_SetPersistedDevPanelWidth(
    JNIEnv* env,
    Profile* profile,
    int32_t width_dp) {
  CHECK(profile);
  ScopedDictPrefUpdate update(profile->GetPrefs(), prefs::kSidePanelIdToWidth);
  update->Set(SidePanelEntryIdToString(SidePanelEntryId::kSidePanelDev),
              width_dp);
}

DEFINE_JNI(SidePanelContainerCoordinatorIntegrationTestSupport)
