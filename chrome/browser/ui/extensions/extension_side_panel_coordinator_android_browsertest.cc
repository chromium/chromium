// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extension_side_panel_coordinator.h"

#include <string>
#include <string_view>
#include <utility>

#include "base/android/device_info.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/api/side_panel/side_panel_service.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/extensions/extension_side_panel_manager.h"
#include "chrome/browser/ui/side_panel/android/android_side_panel_enabled_fn.h"
#include "chrome/browser/ui/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_key.h"
#include "chrome/browser/ui/side_panel/side_panel_registry.h"
#include "chrome/common/extensions/api/side_panel.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_icon_image.h"
#include "extensions/browser/test_image_loader.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/test/test_extension_dir.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace extensions {

class ExtensionSidePanelCoordinatorAndroidBrowserTest
    : public ExtensionBrowserTest {
 public:
  ExtensionSidePanelCoordinatorAndroidBrowserTest() {
    feature_list_.InitAndEnableFeature(
        chrome::android::kEnableAndroidSidePanel);
  }
  ~ExtensionSidePanelCoordinatorAndroidBrowserTest() override = default;

  void SetUp() override {
    if (!base::android::device_info::is_desktop() &&
        !base::android::device_info::is_tablet()) {
      GTEST_SKIP() << "Side panel is for large form factors; skipping the test "
                      "on others.";
    }
    if (!AndroidSidePanelEnabledFn::IsEnabled()) {
      GTEST_SKIP() << "Android Side Panel is disabled";
    }
    ExtensionBrowserTest::SetUp();
  }

 protected:
  SidePanelEntry* GetSidePanelEntry(const ExtensionId& extension_id) {
    SidePanelRegistry* registry =
        SidePanelRegistry::From(browser_window_interface());
    CHECK(registry);
    return registry->GetEntryForKey(
        SidePanelEntry::Key(SidePanelEntry::Id::kExtension, extension_id));
  }

  void WaitForExtensionIconLoaded(const ExtensionId& extension_id) {
    ExtensionSidePanelManager* manager =
        ExtensionSidePanelManager::From(browser_window_interface());
    ASSERT_TRUE(manager);
    ExtensionSidePanelCoordinator* coordinator =
        manager->GetExtensionCoordinatorForTesting(extension_id);
    ASSERT_TRUE(coordinator);
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return coordinator->extension_icon()->did_complete_initial_load();
    }));
  }

  void VerifyEntryTitle(const SidePanelEntry& entry,
                        std::u16string_view expected_title) {
    const std::u16string* title = entry.GetProperty(kSidePanelTitleKey);
    ASSERT_TRUE(title);
    EXPECT_EQ(*title, expected_title);
  }

  void VerifyEntryIcon(const SidePanelEntry& entry,
                       const Extension& extension) {
    const ui::ImageModel* icon = entry.GetProperty(kSidePanelIconKey);
    ASSERT_TRUE(icon);
    ASSERT_TRUE(icon->IsImage());

    SkBitmap expected_icon_bitmap = TestImageLoader::LoadAndGetExtensionBitmap(
        &extension, "icon.png", extension_misc::EXTENSION_ICON_SMALLISH);
    EXPECT_TRUE(gfx::test::AreBitmapsEqual(expected_icon_bitmap,
                                           *icon->GetImage().ToSkBitmap()));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Verifies that registering an extension's SidePanelEntry on Android sets its
// `kSidePanelTitleKey` and `kSidePanelIconKey` properties to the extension's
// short name and loaded icon.
IN_PROC_BROWSER_TEST_F(ExtensionSidePanelCoordinatorAndroidBrowserTest,
                       EntrySetsExtensionTitleAndIconOnRegistration) {
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("api_test/side_panel/simple_default"));
  ASSERT_TRUE(extension);

  ASSERT_NO_FATAL_FAILURE(WaitForExtensionIconLoaded(extension->id()));

  SidePanelEntry* entry = GetSidePanelEntry(extension->id());
  ASSERT_TRUE(entry);
  VerifyEntryTitle(*entry, base::UTF8ToUTF16(extension->short_name()));
  VerifyEntryIcon(*entry, *extension);
}

// Verifies that dynamically registering an extension's SidePanelEntry via
// `sidePanel.setOptions()` after initial load populates `kSidePanelTitleKey`
// (preferring `short_name` over `name`) and `kSidePanelIconKey`.
IN_PROC_BROWSER_TEST_F(ExtensionSidePanelCoordinatorAndroidBrowserTest,
                       EntrySetsExtensionTitleAndIconWhenEnabledViaSetOptions) {
  static constexpr char kManifest[] =
      R"({
          "name": "Extension with dynamic side panel",
          "short_name": "Dynamic panel",
          "version": "1.0",
          "manifest_version": 3,
          "icons": {"180": "icon.png"},
          "permissions": ["sidePanel"]
        })";
  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("default_path.html"),
                     "<html><body>Test</body></html>");
  test_dir.CopyFileTo(
      test_data_dir_.AppendASCII("api_test/side_panel/simple_default/icon.png"),
      FILE_PATH_LITERAL("icon.png"));

  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  EXPECT_FALSE(GetSidePanelEntry(extension->id()));

  ASSERT_NO_FATAL_FAILURE(WaitForExtensionIconLoaded(extension->id()));

  api::side_panel::PanelOptions options;
  options.enabled = true;
  options.path = "default_path.html";
  SidePanelService::Get(profile())->SetOptions(*extension, std::move(options));

  SidePanelEntry* entry = GetSidePanelEntry(extension->id());
  ASSERT_TRUE(entry);
  VerifyEntryTitle(*entry, u"Dynamic panel");
  VerifyEntryIcon(*entry, *extension);
}

}  // namespace extensions
