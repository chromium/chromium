// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_toolbar_desktop.h"

#include <algorithm>
#include <string>

#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/user_action_tester.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_action_runner.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/extension_ui_util.h"
#include "chrome/browser/ui/extensions/extensions_toolbar_view_model.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_model.h"
#include "chrome/browser/ui/views/extensions/browser_action_drag_data.h"
#include "chrome/browser/ui/views/extensions/extension_view_utils.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_browsertest.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/permissions/chip/permission_chip_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/host_access_request_helper.h"
#include "extensions/browser/pref_names.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_id.h"
#include "extensions/test/permissions_manager_waiter.h"
#include "extensions/test/test_extension_dir.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/views_switches.h"

namespace {

using SitePermissionsHelper = extensions::SitePermissionsHelper;
using PermissionsManager = extensions::PermissionsManager;

}  // namespace

class ExtensionsToolbarDesktopBrowserTest
    : public ExtensionsToolbarBrowserTest {
 public:
  ExtensionsToolbarDesktopBrowserTest()
      : ExtensionsToolbarDesktopBrowserTest({}, {}) {}
  ExtensionsToolbarDesktopBrowserTest(
      const std::vector<base::test::FeatureRef>& enabled_features,
      const std::vector<base::test::FeatureRef>& disabled_features)
      : ExtensionsToolbarBrowserTest(
            [enabled_features] {
              std::vector<base::test::FeatureRef> actual = enabled_features;
              actual.push_back(
                  extensions_features::kExtensionsMenuAccessControl);
              return actual;
            }(),
            disabled_features) {}
  ~ExtensionsToolbarDesktopBrowserTest() override = default;
  ExtensionsToolbarDesktopBrowserTest(
      const ExtensionsToolbarDesktopBrowserTest&) = delete;
  ExtensionsToolbarDesktopBrowserTest& operator=(
      const ExtensionsToolbarDesktopBrowserTest&) = delete;

  // Returns the view of the given `extension_id` if the extension is currently
  // pinned.
  ToolbarActionView* GetPinnedExtensionView(
      const extensions::ExtensionId& extension_id) {
    std::vector<ToolbarActionView*> actions = GetPinnedExtensionViews();
    auto it =
        std::ranges::find(actions, extension_id, [](ToolbarActionView* action) {
          return action->view_model()->GetId();
        });
    if (it == actions.end()) {
      return nullptr;
    }
    return *it;
  }

  // Returns whether the request access button is visible or not.
  bool IsRequestAccessButtonVisible() {
    return request_access_button()->GetVisible();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionsToolbarBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(
        views::switches::kDisableInputEventActivationProtectionForTesting);
  }

  void SetUpInProcessBrowserTestFixture() override {
    ExtensionsToolbarBrowserTest::SetUpInProcessBrowserTestFixture();
    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
  }

 protected:
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;

 protected:
  ExtensionsToolbarViewModel::ExtensionsToolbarButtonState
  GetCurrentButtonState() {
    content::WebContents* active_web_contents = web_contents();
    CHECK(active_web_contents);
    return extensions_container()->GetToolbarViewModel()->GetButtonState(
        *active_web_contents);
  }
};

IN_PROC_BROWSER_TEST_F(ExtensionsToolbarDesktopBrowserTest,
                       BrowserActionDragDataPickleRoundTrip) {
  BrowserActionDragData source_data("extension-id", 7);
  ui::OSExchangeData exchange_data;
  source_data.Write(profile(), &exchange_data);

  EXPECT_TRUE(BrowserActionDragData::CanDrop(exchange_data, profile()));

  BrowserActionDragData restored_data;
  ASSERT_TRUE(restored_data.Read(exchange_data));
  EXPECT_EQ("extension-id", restored_data.id());
  EXPECT_EQ(7u, restored_data.index());
  EXPECT_TRUE(restored_data.IsFromProfile(profile()));
}

IN_PROC_BROWSER_TEST_F(ExtensionsToolbarDesktopBrowserTest,
                       ReorderPinnedExtensions) {
  constexpr char kExtensionAName[] = "A Extension";
  auto extensionA = InstallExtension(kExtensionAName);
  constexpr char kExtensionBName[] = "B Extension";
  auto extensionB = InstallExtension(kExtensionBName);
  constexpr char kExtensionCName[] = "C Extension";
  auto extensionC = InstallExtension(kExtensionCName);

  auto* toolbar_model = ToolbarActionsModel::Get(profile());
  ASSERT_TRUE(toolbar_model);

  toolbar_model->SetActionVisibility(extensionA->id(), true);
  toolbar_model->SetActionVisibility(extensionB->id(), true);
  toolbar_model->SetActionVisibility(extensionC->id(), true);
  WaitForAnimation();

  // Verify the order is A, B, C.
  EXPECT_THAT(
      GetPinnedExtensionNames(),
      testing::ElementsAre(kExtensionAName, kExtensionBName, kExtensionCName));

  // Simulate dragging extension C to the first slot.
  ToolbarActionView* drag_view = GetPinnedExtensionView(extensionC->id());
  EXPECT_TRUE(extensions_container()->CanStartDragForView(
      drag_view, gfx::Point(), gfx::Point()));
  ui::OSExchangeData drag_data;
  extensions_container()->WriteDragDataForView(drag_view, gfx::Point(),
                                               &drag_data);
  gfx::PointF drop_point(GetPinnedExtensionView(extensionA->id())->origin());
  ui::DropTargetEvent drop_event(drag_data, drop_point, drop_point,
                                 ui::DragDropTypes::DRAG_MOVE);
  extensions_container()->OnDragUpdated(drop_event);
  auto drop_cb = extensions_container()->GetDropCallback(drop_event);
  ui::mojom::DragOperation output_drag_op = ui::mojom::DragOperation::kNone;
  std::move(drop_cb).Run(drop_event, output_drag_op,
                         /*drag_image_layer_owner=*/nullptr);
  WaitForAnimation();

  // Verify the new order is C, A, B.
  EXPECT_THAT(
      GetPinnedExtensionNames(),
      testing::ElementsAre(kExtensionCName, kExtensionAName, kExtensionBName));
}

IN_PROC_BROWSER_TEST_F(ExtensionsToolbarDesktopBrowserTest,
                       ForcePinnedExtensionsCannotReorder) {
  constexpr char kExtensionAName[] = "A Extension";
  auto extensionA = InstallExtension(kExtensionAName);
  constexpr char kExtensionBName[] = "B Extension";
  auto extensionB = InstallExtension(kExtensionBName);
  constexpr char kExtensionCName[] = "C Extension";
  auto extensionC = InstallExtension(kExtensionCName);

  auto* toolbar_model = ToolbarActionsModel::Get(profile());
  ASSERT_TRUE(toolbar_model);

  toolbar_model->SetActionVisibility(extensionA->id(), true);
  toolbar_model->SetActionVisibility(extensionB->id(), true);
  toolbar_model->SetActionVisibility(extensionC->id(), true);
  WaitForAnimation();

  // Make Extension C force-pinned, as if it was controlled by the
  // ExtensionSettings policy.
  std::string json = base::StringPrintf(
      R"({
        "%s": {
          "toolbar_pin": "force_pinned"
        }
      })",
      extensionC->id().c_str());
  std::optional<base::Value> settings =
      base::JSONReader::Read(json, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(settings.has_value());
  policy::PolicyMap policy_map;
  policy_map.Set(policy::key::kExtensionSettings,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, std::move(settings.value()),
                 /*external_data_fetcher=*/nullptr);
  policy_provider_.UpdateChromePolicy(policy_map);
  base::RunLoop().RunUntilIdle();
  WaitForAnimation();

  // Verify the order is A, B, C.
  EXPECT_THAT(
      GetPinnedExtensionNames(),
      testing::ElementsAre(kExtensionAName, kExtensionBName, kExtensionCName));
  EXPECT_TRUE(toolbar_model->IsActionForcePinned(extensionC->id()));

  // Force-pinned extension should not be draggable.
  ToolbarActionView* drag_view = GetPinnedExtensionView(extensionC->id());
  EXPECT_FALSE(extensions_container()->CanStartDragForView(
      drag_view, gfx::Point(), gfx::Point()));
}

// Tests that when an extension is reloaded it remains visible in the toolbar.
IN_PROC_BROWSER_TEST_F(ExtensionsToolbarDesktopBrowserTest,
                       ReloadExtensionKeepsPinnedState) {
  // The extension must have a manifest to be reloaded.
  extensions::TestExtensionDir extension_directory;
  constexpr char kManifest[] = R"({
        "name": "Test Extension",
        "version": "1",
        "manifest_version": 3
      })";
  extension_directory.WriteManifest(kManifest);
  extensions::ChromeTestExtensionLoader loader(profile());
  scoped_refptr<const extensions::Extension> extension =
      loader.LoadExtension(extension_directory.UnpackedPath());

  // Pin extension and verify it is visible on the toolbar.
  auto* toolbar_model = ToolbarActionsModel::Get(profile());
  ASSERT_TRUE(toolbar_model);
  if (!toolbar_model->IsActionPinned(extension->id())) {
    toolbar_model->SetActionVisibility(extension->id(), true);
  }
  EXPECT_TRUE(
      extensions_container()->IsActionVisibleOnToolbar(extension->id()));

  // Reload the extension.
  extensions::TestExtensionRegistryObserver registry_observer(
      extensions::ExtensionRegistry::Get(profile()), extension->id());
  ReloadExtension(extension->id());
  ASSERT_TRUE(registry_observer.WaitForExtensionLoaded());
  content::RunAllTasksUntilIdle();
  WaitForAnimation();

  // Verify the extension is visible on the toolbar.
  EXPECT_TRUE(
      extensions_container()->IsActionVisibleOnToolbar(extension->id()));
}

// Tests that a when an extension is reloaded with manifest errors, and
// therefore fails to be loaded into Chrome, it's removed from the toolbar.
IN_PROC_BROWSER_TEST_F(ExtensionsToolbarDesktopBrowserTest,
                       ReloadExtensionFailed) {
  extensions::TestExtensionDir extension_directory;
  constexpr char kManifest[] = R"({
        "name": "Test Extension",
        "version": "1",
        "manifest_version": 3
      })";
  extension_directory.WriteManifest(kManifest);
  extensions::ChromeTestExtensionLoader loader(profile());
  scoped_refptr<const extensions::Extension> extension =
      loader.LoadExtension(extension_directory.UnpackedPath());

  // Pin extension and verify it is visible on the toolbar.
  auto* toolbar_model = ToolbarActionsModel::Get(profile());
  ASSERT_TRUE(toolbar_model);
  if (!toolbar_model->IsActionPinned(extension->id())) {
    toolbar_model->SetActionVisibility(extension->id(), true);
  }
  EXPECT_TRUE(
      extensions_container()->IsActionVisibleOnToolbar(extension->id()));

  // Replace the extension's valid manifest with one containing errors. In this
  // case, 'version' keys is missing.
  constexpr char kManifestWithErrors[] = R"({
        "name": "Test",
        "manifest_version": 3,
      })";
  extension_directory.WriteManifest(kManifestWithErrors);

  // Reload the extension. It should fail due to the manifest errors.
  extension_registrar()->ReloadExtensionWithQuietFailure(extension->id());
  content::RunAllTasksUntilIdle();
  WaitForAnimation();

  // Verify the extension is no longer visible on the toolbar.
  EXPECT_FALSE(
      extensions_container()->IsActionVisibleOnToolbar(extension->id()));
}

IN_PROC_BROWSER_TEST_F(ExtensionsToolbarDesktopBrowserTest,
                       PinnedExtensionAppearsInAnotherWindow) {
  const std::string& extension_id = InstallExtension("Extension")->id();
  const auto is_action_visible_on_toolbar = [&extension_id](Browser* browser) {
    return BrowserView::GetBrowserViewForBrowser(browser)
        ->toolbar()
        ->extensions_container()
        ->IsActionVisibleOnToolbar(extension_id);
  };

  Browser* browser2 = CreateBrowser(profile());

  // Verify extension is unpinned in both windows.
  EXPECT_FALSE(is_action_visible_on_toolbar(browser()));
  EXPECT_FALSE(is_action_visible_on_toolbar(browser2));

  // Pin extension in one window.
  auto* toolbar_model = ToolbarActionsModel::Get(profile());
  ASSERT_TRUE(toolbar_model);
  toolbar_model->SetActionVisibility(extension_id, true);

  // Both windows open get the pinned extension.
  EXPECT_TRUE(is_action_visible_on_toolbar(browser()));
  EXPECT_TRUE(is_action_visible_on_toolbar(browser2));

  Browser* browser3 = CreateBrowser(profile());

  // Brand-new window also gets the pinned extension.
  EXPECT_TRUE(is_action_visible_on_toolbar(browser3));
}

IN_PROC_BROWSER_TEST_F(ExtensionsToolbarDesktopBrowserTest,
                       PinnedExtensionsReorderOnPrefChange) {
  constexpr char kExtensionAName[] = "A Extension";
  auto extensionA = InstallExtension(kExtensionAName);
  constexpr char kExtensionBName[] = "B Extension";
  auto extensionB = InstallExtension(kExtensionBName);
  constexpr char kExtensionCName[] = "C Extension";
  auto extensionC = InstallExtension(kExtensionCName);

  auto* toolbar_model = ToolbarActionsModel::Get(profile());
  ASSERT_TRUE(toolbar_model);

  toolbar_model->SetActionVisibility(extensionA->id(), true);
  toolbar_model->SetActionVisibility(extensionB->id(), true);
  toolbar_model->SetActionVisibility(extensionC->id(), true);
  WaitForAnimation();

  // Verify the order is A, B, C.
  EXPECT_THAT(
      GetPinnedExtensionNames(),
      testing::ElementsAre(kExtensionAName, kExtensionBName, kExtensionCName));

  // Set the order using prefs.
  extensions::ExtensionPrefs::Get(profile())->SetPinnedExtensions(
      {extensionB->id(), extensionC->id(), extensionA->id()});
  WaitForAnimation();

  // Verify the new order is B, C, A.
  EXPECT_THAT(
      GetPinnedExtensionNames(),
      testing::ElementsAre(kExtensionBName, kExtensionCName, kExtensionAName));
}

IN_PROC_BROWSER_TEST_F(ExtensionsToolbarDesktopBrowserTest, RunDropCallback) {
  constexpr char kExtensionAName[] = "A Extension";
  auto extensionA = InstallExtension(kExtensionAName);
  constexpr char kExtensionBName[] = "B Extension";
  auto extensionB = InstallExtension(kExtensionBName);
  constexpr char kExtensionCName[] = "C Extension";
  auto extensionC = InstallExtension(kExtensionCName);

  auto* toolbar_model = ToolbarActionsModel::Get(profile());
  ASSERT_TRUE(toolbar_model);

  toolbar_model->SetActionVisibility(extensionA->id(), true);
  toolbar_model->SetActionVisibility(extensionB->id(), true);
  toolbar_model->SetActionVisibility(extensionC->id(), true);
  WaitForAnimation();

  EXPECT_THAT(
      GetPinnedExtensionNames(),
      testing::ElementsAre(kExtensionAName, kExtensionBName, kExtensionCName));

  // Simulate dragging extension C to the first slot.
  ToolbarActionView* drag_view = GetPinnedExtensionView(extensionC->id());
  EXPECT_TRUE(extensions_container()->CanStartDragForView(
      drag_view, gfx::Point(), gfx::Point()));
  ui::OSExchangeData drag_data;
  extensions_container()->WriteDragDataForView(drag_view, gfx::Point(),
                                               &drag_data);
  gfx::PointF drop_point(GetPinnedExtensionView(extensionA->id())->origin());
  ui::DropTargetEvent drop_event(drag_data, drop_point, drop_point,
                                 ui::DragDropTypes::DRAG_MOVE);
  extensions_container()->OnDragUpdated(drop_event);
  auto cb = extensions_container()->GetDropCallback(drop_event);
  ui::mojom::DragOperation output_drag_op = ui::mojom::DragOperation::kNone;
  std::move(cb).Run(drop_event, output_drag_op,
                    /*drag_image_layer_owner=*/nullptr);
  WaitForAnimation();

  EXPECT_THAT(
      GetPinnedExtensionNames(),
      testing::ElementsAre(kExtensionCName, kExtensionAName, kExtensionBName));
  EXPECT_EQ(output_drag_op, ui::mojom::DragOperation::kMove);
}

IN_PROC_BROWSER_TEST_F(ExtensionsToolbarDesktopBrowserTest, ResetDropCallback) {
  constexpr char kExtensionAName[] = "A Extension";
  auto extensionA = InstallExtension(kExtensionAName);
  constexpr char kExtensionBName[] = "B Extension";
  auto extensionB = InstallExtension(kExtensionBName);
  constexpr char kExtensionCName[] = "C Extension";
  auto extensionC = InstallExtension(kExtensionCName);

  auto* toolbar_model = ToolbarActionsModel::Get(profile());
  ASSERT_TRUE(toolbar_model);

  toolbar_model->SetActionVisibility(extensionA->id(), true);
  toolbar_model->SetActionVisibility(extensionB->id(), true);
  toolbar_model->SetActionVisibility(extensionC->id(), true);
  WaitForAnimation();

  EXPECT_THAT(
      GetPinnedExtensionNames(),
      testing::ElementsAre(kExtensionAName, kExtensionBName, kExtensionCName));

  // Simulate dragging "C Extension" to the first slot.
  ToolbarActionView* drag_view = GetPinnedExtensionView(extensionC->id());
  EXPECT_TRUE(extensions_container()->CanStartDragForView(
      drag_view, gfx::Point(), gfx::Point()));
  ui::OSExchangeData drag_data;
  extensions_container()->WriteDragDataForView(drag_view, gfx::Point(),
                                               &drag_data);
  gfx::PointF drop_point(GetPinnedExtensionView(extensionA->id())->origin());
  ui::DropTargetEvent drop_event(drag_data, drop_point, drop_point,
                                 ui::DragDropTypes::DRAG_MOVE);
  extensions_container()->OnDragUpdated(drop_event);
  auto cb = extensions_container()->GetDropCallback(drop_event);
  WaitForAnimation();

  EXPECT_THAT(
      GetPinnedExtensionNames(),
      testing::ElementsAre(kExtensionCName, kExtensionAName, kExtensionBName));

  // If the drop callback is reset (and never invoked), the drag should be
  // aborted, and items should be back in their original order.
  cb.Reset();
  WaitForAnimation();

  EXPECT_THAT(
      GetPinnedExtensionNames(),
      testing::ElementsAre(kExtensionAName, kExtensionBName, kExtensionCName));
}

IN_PROC_BROWSER_TEST_F(ExtensionsToolbarDesktopBrowserTest,
                       InvalidateDropCallbackOnActionAdded) {
  constexpr char kExtensionAName[] = "A Extension";
  auto extensionA = InstallExtension(kExtensionAName);
  constexpr char kExtensionBName[] = "B Extension";
  auto extensionB = InstallExtension(kExtensionBName);

  auto* toolbar_model = ToolbarActionsModel::Get(profile());
  ASSERT_TRUE(toolbar_model);

  toolbar_model->SetActionVisibility(extensionA->id(), true);
  toolbar_model->SetActionVisibility(extensionB->id(), true);
  WaitForAnimation();

  EXPECT_THAT(GetPinnedExtensionNames(),
              testing::ElementsAre(kExtensionAName, kExtensionBName));

  // Simulate dragging extension B to the first slot.
  ToolbarActionView* drag_view = GetPinnedExtensionView(extensionB->id());
  EXPECT_TRUE(extensions_container()->CanStartDragForView(
      drag_view, gfx::Point(), gfx::Point()));
  ui::OSExchangeData drag_data;
  extensions_container()->WriteDragDataForView(drag_view, gfx::Point(),
                                               &drag_data);
  gfx::PointF drop_point(GetPinnedExtensionView(extensionA->id())->origin());
  ui::DropTargetEvent drop_event(drag_data, drop_point, drop_point,
                                 ui::DragDropTypes::DRAG_MOVE);
  extensions_container()->OnDragUpdated(drop_event);
  auto cb = extensions_container()->GetDropCallback(drop_event);
  WaitForAnimation();

  EXPECT_THAT(GetPinnedExtensionNames(),
              testing::ElementsAre(kExtensionBName, kExtensionAName));

  constexpr char kExtensionCName[] = "C Extension";
  auto extensionC = InstallExtension(kExtensionCName);
  toolbar_model->SetActionVisibility(extensionC->id(), true);
  WaitForAnimation();

  // The drop callback should be invalidated, and items should be back in their
  // original order.
  ui::mojom::DragOperation output_drag_op = ui::mojom::DragOperation::kNone;
  std::move(cb).Run(drop_event, output_drag_op,
                    /*drag_image_layer_owner=*/nullptr);
  WaitForAnimation();

  EXPECT_THAT(
      GetPinnedExtensionNames(),
      testing::ElementsAre(kExtensionAName, kExtensionBName, kExtensionCName));
}

// Tests reordering pinned actions with MovePinnedActionsBy().
IN_PROC_BROWSER_TEST_F(ExtensionsToolbarDesktopBrowserTest,
                       TestMovePinnedActionBy) {
  constexpr char kExtensionAName[] = "A Extension";
  auto extensionA = InstallExtension(kExtensionAName);
  constexpr char kExtensionBName[] = "B Extension";
  auto extensionB = InstallExtension(kExtensionBName);
  constexpr char kExtensionCName[] = "C Extension";
  auto extensionC = InstallExtension(kExtensionCName);

  auto* toolbar_model = ToolbarActionsModel::Get(profile());
  ASSERT_TRUE(toolbar_model);

  toolbar_model->SetActionVisibility(extensionA->id(), true);
  toolbar_model->SetActionVisibility(extensionB->id(), true);
  toolbar_model->SetActionVisibility(extensionC->id(), true);
  WaitForAnimation();

  EXPECT_THAT(
      GetPinnedExtensionNames(),
      testing::ElementsAre(kExtensionAName, kExtensionBName, kExtensionCName));

  // Simulate moving "A" by -1. Nothing should change.
  extensions_container()->MovePinnedActionBy(extensionA->id(), -1);
  EXPECT_THAT(
      GetPinnedExtensionNames(),
      testing::ElementsAre(kExtensionAName, kExtensionBName, kExtensionCName));

  // Simulate moving "C" by 1. Nothing should change.
  extensions_container()->MovePinnedActionBy(extensionC->id(), 1);
  EXPECT_THAT(
      GetPinnedExtensionNames(),
      testing::ElementsAre(kExtensionAName, kExtensionBName, kExtensionCName));

  // Now, move A by 1. The order should be B, A, C.
  extensions_container()->MovePinnedActionBy(extensionA->id(), 1);
  EXPECT_THAT(
      GetPinnedExtensionNames(),
      testing::ElementsAre(kExtensionBName, kExtensionAName, kExtensionCName));

  // Move A by 1 more. The order should be B, C, A.
  extensions_container()->MovePinnedActionBy(extensionA->id(), 1);
  EXPECT_THAT(
      GetPinnedExtensionNames(),
      testing::ElementsAre(kExtensionBName, kExtensionCName, kExtensionAName));

  // Move A back by one. Now, B, A, C.
  extensions_container()->MovePinnedActionBy(extensionA->id(), -1);
  EXPECT_THAT(
      GetPinnedExtensionNames(),
      testing::ElementsAre(kExtensionBName, kExtensionAName, kExtensionCName));

  // And back once more. Back to A, B, C.
  extensions_container()->MovePinnedActionBy(extensionA->id(), -1);
  EXPECT_THAT(
      GetPinnedExtensionNames(),
      testing::ElementsAre(kExtensionAName, kExtensionBName, kExtensionCName));
}

// ToolbarActionsModel::MovePinnedAction crashes if pinned extensions changes
// while the drop callback isn't invalidated. This test makes sure this doesn't
// happen anymore. https://crbug.com/40803390.
IN_PROC_BROWSER_TEST_F(ExtensionsToolbarDesktopBrowserTest,
                       InvalidateDropCallbackOnPrefChange) {
  constexpr char kExtensionAName[] = "A Extension";
  auto extensionA = InstallExtension(kExtensionAName);
  constexpr char kExtensionBName[] = "B Extension";
  auto extensionB = InstallExtension(kExtensionBName);

  auto* toolbar_model = ToolbarActionsModel::Get(profile());
  ASSERT_TRUE(toolbar_model);

  toolbar_model->SetActionVisibility(extensionA->id(), true);
  toolbar_model->SetActionVisibility(extensionB->id(), true);
  WaitForAnimation();

  EXPECT_THAT(GetPinnedExtensionNames(),
              testing::ElementsAre(kExtensionAName, kExtensionBName));

  // Simulate dragging extension B to the first slot.
  ToolbarActionView* drag_view = GetPinnedExtensionView(extensionB->id());
  EXPECT_TRUE(extensions_container()->CanStartDragForView(
      drag_view, gfx::Point(), gfx::Point()));
  ui::OSExchangeData drag_data;
  extensions_container()->WriteDragDataForView(drag_view, gfx::Point(),
                                               &drag_data);
  gfx::PointF drop_point(GetPinnedExtensionView(extensionA->id())->origin());
  ui::DropTargetEvent drop_event(drag_data, drop_point, drop_point,
                                 ui::DragDropTypes::DRAG_MOVE);
  extensions_container()->OnDragUpdated(drop_event);
  auto cb = extensions_container()->GetDropCallback(drop_event);
  WaitForAnimation();

  EXPECT_THAT(GetPinnedExtensionNames(),
              testing::ElementsAre(kExtensionBName, kExtensionAName));

  extensions::ExtensionPrefs::Get(profile())->SetPinnedExtensions({});
  WaitForAnimation();

  // The drop callback should be invalidated, and items should be back in their
  // original order.
  ui::mojom::DragOperation output_drag_op = ui::mojom::DragOperation::kNone;
  std::move(cb).Run(drop_event, output_drag_op,
                    /*drag_image_layer_owner=*/nullptr);
  WaitForAnimation();

  EXPECT_THAT(GetPinnedExtensionNames(), testing::ElementsAre());
}

IN_PROC_BROWSER_TEST_F(ExtensionsToolbarDesktopBrowserTest,
                       ExtensionsToolbarButtonIconAndText) {
  // Test default state.
  EXPECT_EQ(
      ExtensionsToolbarViewModel::GetToolbarButtonIcon(
          ExtensionsToolbarViewModel::ExtensionsToolbarButtonState::kDefault)
          .name,
      features::IsRoundedIconsEnabled()
          ? vector_icons::kChromeExtensionIcon.name
          : vector_icons::kExtensionChromeRefreshOldIcon.name);
  EXPECT_EQ(
      ExtensionsToolbarViewModel::GetToolbarButtonAccessibleText(
          ExtensionsToolbarViewModel::ExtensionsToolbarButtonState::kDefault),
      l10n_util::GetStringUTF16(IDS_ACC_NAME_EXTENSIONS_BUTTON));
  EXPECT_EQ(
      ExtensionsToolbarViewModel::GetToolbarButtonTooltipText(
          ExtensionsToolbarViewModel::ExtensionsToolbarButtonState::kDefault),
      l10n_util::GetStringUTF16(IDS_TOOLTIP_EXTENSIONS_BUTTON));

  // Test all extensions are blocked state.
  EXPECT_EQ(ExtensionsToolbarViewModel::GetToolbarButtonIcon(
                ExtensionsToolbarViewModel::ExtensionsToolbarButtonState::
                    kAllExtensionsBlocked)
                .name,
            features::IsRoundedIconsEnabled()
                ? vector_icons::kChromeExtensionOffIcon.name
                : vector_icons::kExtensionOffOldIcon.name);
  EXPECT_EQ(ExtensionsToolbarViewModel::GetToolbarButtonAccessibleText(
                ExtensionsToolbarViewModel::ExtensionsToolbarButtonState::
                    kAllExtensionsBlocked),
            l10n_util::GetStringUTF16(
                IDS_ACC_NAME_EXTENSIONS_BUTTON_ALL_EXTENSIONS_BLOCKED));
  EXPECT_EQ(ExtensionsToolbarViewModel::GetToolbarButtonTooltipText(
                ExtensionsToolbarViewModel::ExtensionsToolbarButtonState::
                    kAllExtensionsBlocked),
            l10n_util::GetStringUTF16(
                IDS_TOOLTIP_EXTENSIONS_BUTTON_ALL_EXTENSIONS_BLOCKED));

  // Test any extension has access state.
  EXPECT_EQ(ExtensionsToolbarViewModel::GetToolbarButtonIcon(
                ExtensionsToolbarViewModel::ExtensionsToolbarButtonState::
                    kAnyExtensionHasAccess)
                .name,
            features::IsRoundedIconsEnabled()
                ? vector_icons::kChromeExtensionCheckIcon.name
                : vector_icons::kExtensionOnOldIcon.name);
  EXPECT_EQ(ExtensionsToolbarViewModel::GetToolbarButtonAccessibleText(
                ExtensionsToolbarViewModel::ExtensionsToolbarButtonState::
                    kAnyExtensionHasAccess),
            l10n_util::GetStringUTF16(
                IDS_ACC_NAME_EXTENSIONS_BUTTON_ANY_EXTENSION_HAS_ACCESS));
  EXPECT_EQ(ExtensionsToolbarViewModel::GetToolbarButtonTooltipText(
                ExtensionsToolbarViewModel::ExtensionsToolbarButtonState::
                    kAnyExtensionHasAccess),
            l10n_util::GetStringUTF16(
                IDS_TOOLTIP_EXTENSIONS_BUTTON_ANY_EXTENSION_HAS_ACCESS));
}

// TODO(crbug.com/475863910): Move the tests testing
// ExtensionsToolbarViewModel::GetButtonState() to
// extensions_toolbar_view_model_unittest.cc once it's created.

// Test that the extension button state changes after site permissions updates.
IN_PROC_BROWSER_TEST_F(ExtensionsToolbarDesktopBrowserTest,
                       ExtensionsButton_SitePermissionsUpdates) {
  // Install an extension that requests host permissions.
  auto extension =
      InstallExtensionWithHostPermissions("Extension", {"<all_urls>"});

  const GURL url("http://www.url.com");
  NavigateAndCommit(url);
  auto url_origin =
      web_contents()->GetPrimaryMainFrame()->GetLastCommittedOrigin();

  auto* manager = extensions::PermissionsManager::Get(profile());
  {
    // Extensions button has "all extensions blocked" icon type when it's
    // an user restricted site.
    extensions::PermissionsManagerWaiter manager_waiter(manager);
    manager->AddUserRestrictedSite(url_origin);
    manager_waiter.WaitForUserPermissionsSettingsChange();
    WaitForAnimation();
    EXPECT_EQ(GetCurrentButtonState(),
              ExtensionsToolbarViewModel::ExtensionsToolbarButtonState::
                  kAllExtensionsBlocked);
  }

  {
    // Extensions button has "any extension has access" icon type when it's not
    // an user restricted site and 1+ extensions have
    // site access granted. Note that by default extensions have granted access.
    extensions::PermissionsManagerWaiter manager_waiter(manager);
    manager->RemoveUserRestrictedSite(url_origin);
    manager_waiter.WaitForUserPermissionsSettingsChange();
    WaitForAnimation();
    EXPECT_EQ(GetCurrentButtonState(),
              ExtensionsToolbarViewModel::ExtensionsToolbarButtonState::
                  kAnyExtensionHasAccess);
  }

  {
    // Extension button has "default" icon type when it's not an user restricted
    // site and no extensions have site access granted.
    // To achieve this, we withhold host permissions in the only extension
    // installed.
    WithholdHostPermissions(extension.get());
    WaitForAnimation();
    EXPECT_EQ(
        GetCurrentButtonState(),
        ExtensionsToolbarViewModel::ExtensionsToolbarButtonState::kDefault);
  }
}

// Test that the extension button state takes into account chrome restricted
// sites.
IN_PROC_BROWSER_TEST_F(ExtensionsToolbarDesktopBrowserTest,
                       ExtensionsButton_ChromeRestrictedSite) {
  InstallExtensionWithHostPermissions("Extension", {"<all_urls>"});

  const GURL restricted_url("chrome://extensions");
  NavigateAndCommit(restricted_url);

  // Extensions button has "all extensions blocked" icon type for chrome
  // restricted sites.
  EXPECT_EQ(GetCurrentButtonState(),
            ExtensionsToolbarViewModel::ExtensionsToolbarButtonState::
                kAllExtensionsBlocked);
}

IN_PROC_BROWSER_TEST_F(ExtensionsToolbarDesktopBrowserTest,
                       RequestAccessButton_TooltipTextAccessibility) {
  auto extension_A = InstallExtensionWithHostPermissions(
      "Extension A", {"*://www.example.com/*"});
  auto extension_B =
      InstallExtensionWithHostPermissions("Extension B", {"<all_urls>"});
  WithholdHostPermissions(extension_A.get());
  WithholdHostPermissions(extension_B.get());

  // Navigate to a site and verify request access button is not visible, since
  // no extension has added a request.
  NavigateAndCommit(GURL("http://www.example.com"));
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_FALSE(IsRequestAccessButtonVisible());

  // Add site access requests for both extensions and verify they are visible
  // on the request access button.
  AddHostAccessRequest(*extension_A, web_contents);
  AddHostAccessRequest(*extension_B, web_contents);
  EXPECT_TRUE(IsRequestAccessButtonVisible());
  EXPECT_THAT(request_access_button()->GetExtensionIdsForTesting(),
              testing::ElementsAre(extension_A->id(), extension_B->id()));

  ui::AXNodeData data;
  request_access_button()->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_NE(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            request_access_button()->GetRenderedTooltipText(gfx::Point()));
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kDescription),
            request_access_button()->GetRenderedTooltipText(gfx::Point()));

  RemoveHostAccessRequest(*extension_B,
                          browser()->tab_strip_model()->GetActiveWebContents());

  data = ui::AXNodeData();
  request_access_button()->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_NE(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            request_access_button()->GetRenderedTooltipText(gfx::Point()));
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kDescription),
            request_access_button()->GetRenderedTooltipText(gfx::Point()));
}

// Tests that an extension appears in the request access button iff it has a
// site access request that matches the given pattern filter.
IN_PROC_BROWSER_TEST_F(ExtensionsToolbarDesktopBrowserTest,
                       RequestAccessButton_RequestWithPattern) {
  auto extension =
      InstallExtensionWithHostPermissions("Extension", {"<all_urls>"});
  WithholdHostPermissions(extension.get());

  // Navigate to a site and verify request access button is not visible, since
  // no extension has added a request.
  NavigateAndCommit(GURL("http://www.example.com"));
  EXPECT_FALSE(IsRequestAccessButtonVisible());

  // Add a site access request with filter that does not match the current web
  // contents. Verify request access button is hidden.
  URLPattern filter(extensions::Extension::kValidHostPermissionSchemes,
                    "http://www.other.com/*");
  AddHostAccessRequest(
      *extension, browser()->tab_strip_model()->GetActiveWebContents(), filter);
  EXPECT_FALSE(IsRequestAccessButtonVisible());

  // Add a site access request with filter that matches the current web
  // contents. Verify extension is visible on the request access button.
  filter = URLPattern(extensions::Extension::kValidHostPermissionSchemes,
                      "http://www.example.com/*");
  AddHostAccessRequest(
      *extension, browser()->tab_strip_model()->GetActiveWebContents(), filter);
  EXPECT_TRUE(IsRequestAccessButtonVisible());
  EXPECT_THAT(request_access_button()->GetExtensionIdsForTesting(),
              testing::ElementsAre(extension->id()));

  // Add a site access request with filter that does not match the current web
  // contents. Verify request access button is hidden (previous request was
  // removed).
  filter = URLPattern(extensions::Extension::kValidHostPermissionSchemes,
                      "http://www.other.com/*");
  AddHostAccessRequest(
      *extension, browser()->tab_strip_model()->GetActiveWebContents(), filter);
  EXPECT_FALSE(IsRequestAccessButtonVisible());
}

// Tests that an extension's site access request is removed when the extension
// is granted site access.
IN_PROC_BROWSER_TEST_F(ExtensionsToolbarDesktopBrowserTest,
                       RequestAccessButton_ExtensionGrantedSiteAccess) {
  auto extension_A = InstallExtensionWithHostPermissions(
      "Extension A", {"*://www.example.com/*"});
  auto extension_B =
      InstallExtensionWithHostPermissions("Extension B", {"<all_urls>"});
  WithholdHostPermissions(extension_A.get());
  WithholdHostPermissions(extension_B.get());

  // Navigate to a site and verify request access button is not visible, since
  // no extension has added a request.
  NavigateAndCommit(GURL("http://www.example.com"));
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_FALSE(IsRequestAccessButtonVisible());

  // Add site access requests for both extensions and verify they are visible
  // on the request access button.
  AddHostAccessRequest(*extension_A, web_contents);
  AddHostAccessRequest(*extension_B, web_contents);
  EXPECT_TRUE(IsRequestAccessButtonVisible());
  EXPECT_THAT(request_access_button()->GetExtensionIdsForTesting(),
              testing::ElementsAre(extension_A->id(), extension_B->id()));

  // Grant site access to extension B and verify request access button only has
  // extension A, since extension B's request was removed once the extension
  // gained access to the site.
  UpdateUserSiteAccess(*extension_B, web_contents,
                       PermissionsManager::UserSiteAccess::kOnSite);
  EXPECT_TRUE(IsRequestAccessButtonVisible());
  EXPECT_THAT(request_access_button()->GetExtensionIdsForTesting(),
              testing::ElementsAre(extension_A->id()));
}

// Tests that requests are reset on cross-origin navigations.
IN_PROC_BROWSER_TEST_F(ExtensionsToolbarDesktopBrowserTest,
                       RequestAccessButtonVisibility_NavigationBetweenPages) {
  auto extension =
      InstallExtensionWithHostPermissions("Extension", {"<all_urls>"});
  WithholdHostPermissions(extension.get());

  NavigateAndCommit(GURL("http://www.a.com"));
  AddHostAccessRequest(*extension,
                       browser()->tab_strip_model()->GetActiveWebContents());

  EXPECT_TRUE(IsRequestAccessButtonVisible());
  EXPECT_THAT(request_access_button()->GetExtensionIdsForTesting(),
              testing::ElementsAre(extension->id()));

  // Navigate to a same-origin site and verify request access button has
  // extension.
  NavigateAndCommit(GURL("http://www.a.com/title2.html"));
  EXPECT_TRUE(IsRequestAccessButtonVisible());
  EXPECT_THAT(request_access_button()->GetExtensionIdsForTesting(),
              testing::ElementsAre(extension->id()));

  // Navigate to a cross-origin site and verify request access button is hidden.
  NavigateAndCommit(GURL("http://www.b.com"));
  EXPECT_FALSE(IsRequestAccessButtonVisible());

  // Navigate to the original site and verify request access button is hidden,
  // since requests are reset on cross-origin navigations.
  NavigateAndCommit(GURL("http://www.a.com"));
  EXPECT_FALSE(IsRequestAccessButtonVisible());
}

// Tests that the request access button is visible for matched patterns on
// same-origin navigations.
IN_PROC_BROWSER_TEST_F(
    ExtensionsToolbarDesktopBrowserTest,
    RequestAccessButton_NavigationBetweenPages_RequestWithPattern) {
  auto extension =
      InstallExtensionWithHostPermissions("Extension", {"<all_urls>"});
  WithholdHostPermissions(extension.get());

  // Navigate to a site and verify request access button is hidden, since
  // no extension has added a request.
  NavigateAndCommit(GURL("http://www.example.com"));
  EXPECT_FALSE(IsRequestAccessButtonVisible());

  // Add site access request for extension with a filter that doesn't match the
  // current web contents. Verify request access button is hidden.
  URLPattern filter(extensions::Extension::kValidHostPermissionSchemes,
                    "*://*/title2.html*");
  AddHostAccessRequest(
      *extension, browser()->tab_strip_model()->GetActiveWebContents(), filter);
  EXPECT_FALSE(IsRequestAccessButtonVisible());

  // Navigate to a same-origin site that matches the filter. Verify extension is
  // visible on the request access button.
  NavigateAndCommit(GURL("http://www.example.com/title2.html"));
  EXPECT_TRUE(IsRequestAccessButtonVisible());
  EXPECT_THAT(request_access_button()->GetExtensionIdsForTesting(),
              testing::ElementsAre(extension->id()));

  // Add site access request for extension with a filter that doesn't have the
  // same origin as the current web contents. Verify request access button is
  // hidden.
  filter = URLPattern(extensions::Extension::kValidHostPermissionSchemes,
                      "http://www.other.com/title2.html");
  AddHostAccessRequest(
      *extension, browser()->tab_strip_model()->GetActiveWebContents(), filter);
  EXPECT_FALSE(IsRequestAccessButtonVisible());

  // Navigate to a cross-origin site that matches the filters. Since it's a
  // cross-origin navigation, requests are reset. Therefore, verify request
  // access button is hidden.
  NavigateAndCommit(GURL("http://www.other.com/title2.html"));
  EXPECT_FALSE(IsRequestAccessButtonVisible());
}

// Test that request access button is visible based on the user site setting
// selected.
IN_PROC_BROWSER_TEST_F(ExtensionsToolbarDesktopBrowserTest,
                       RequestAccessButton_UserSiteSetting) {
  const GURL url("http://www.url.com");

  // Install an extension and withhold permissions so request access button can
  // be visible.
  auto extension =
      InstallExtensionWithHostPermissions("Extension", {"<all_urls>"});
  WithholdHostPermissions(extension.get());

  // Navigate to url and add a site request for the extension.
  NavigateAndCommit(url);
  auto url_origin =
      web_contents()->GetPrimaryMainFrame()->GetLastCommittedOrigin();
  AddHostAccessRequest(*extension,
                       browser()->tab_strip_model()->GetActiveWebContents());

  // A site has "customize by extensions" site setting by default,
  ASSERT_EQ(GetUserSiteSetting(url),
            PermissionsManager::UserSiteSetting::kCustomizeByExtension);
  EXPECT_TRUE(IsRequestAccessButtonVisible());

  auto* manager = PermissionsManager::Get(profile());

  {
    // Request access button is not visible in restricted sites.
    extensions::PermissionsManagerWaiter manager_waiter(manager);
    manager->AddUserRestrictedSite(url_origin);
    manager_waiter.WaitForUserPermissionsSettingsChange();
    WaitForAnimation();
    EXPECT_FALSE(IsRequestAccessButtonVisible());
  }

  {
    // Request acesss button is visible if site is not restricted,
    // and at least one extension has a site access request.
    extensions::PermissionsManagerWaiter manager_waiter(manager);
    manager->RemoveUserRestrictedSite(url_origin);
    manager_waiter.WaitForUserPermissionsSettingsChange();
    WaitForAnimation();
    EXPECT_TRUE(IsRequestAccessButtonVisible());
  }
}

// Tests that an extension with a site access request but not allowed to show
// requests in the toolbar is not shown in the request access button.
IN_PROC_BROWSER_TEST_F(ExtensionsToolbarDesktopBrowserTest,
                       RequestAccessButton_ExtensionsNotAllowedInButton) {
  // Add two extensions that request access to all urls, and withhold their
  // site access.
  auto extension_a =
      InstallExtensionWithHostPermissions("Extension A", {"<all_urls>"});
  auto extension_b =
      InstallExtensionWithHostPermissions("Extension B", {"<all_urls>"});
  WithholdHostPermissions(extension_a.get());
  WithholdHostPermissions(extension_b.get());

  // By default, both extensions are allowed to show requests in requests access
  // button. However, request access button is not visible because we haven't
  // navigated to a site yet (and extensions haven't added any site access
  // requests).
  SitePermissionsHelper permissions_helper(browser()->GetProfile());
  EXPECT_TRUE(
      permissions_helper.ShowAccessRequestsInToolbar(extension_a->id()));
  EXPECT_TRUE(
      permissions_helper.ShowAccessRequestsInToolbar(extension_b->id()));
  EXPECT_FALSE(IsRequestAccessButtonVisible());

  // Navigate to an url that both extensions want access to, and add site access
  // requests for both.
  const GURL url("http://www.example.com");
  NavigateAndCommit(url);
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  AddHostAccessRequest(*extension_a, web_contents);
  AddHostAccessRequest(*extension_b, web_contents);

  // Verify request access button has both extensions.
  EXPECT_TRUE(IsRequestAccessButtonVisible());
  EXPECT_EQ(
      request_access_button()->GetText(),
      l10n_util::GetStringFUTF16Int(IDS_EXTENSIONS_REQUEST_ACCESS_BUTTON, 2));

  // Disallow extension A in the request access button. Verify only extension A
  // is visible in the button.
  permissions_helper.SetShowAccessRequestsInToolbar(extension_a->id(), false);
  EXPECT_TRUE(IsRequestAccessButtonVisible());
  EXPECT_EQ(
      request_access_button()->GetText(),
      l10n_util::GetStringFUTF16Int(IDS_EXTENSIONS_REQUEST_ACCESS_BUTTON, 1));

  // Disallow extension B in the request access button. Verify button is not
  // visible anymore.
  permissions_helper.SetShowAccessRequestsInToolbar(extension_b->id(), false);
  EXPECT_FALSE(IsRequestAccessButtonVisible());
}

// Test that an extension's request which is dismissed is not visible in the
// request access button.
IN_PROC_BROWSER_TEST_F(ExtensionsToolbarDesktopBrowserTest,
                       RequestAccessButton_RequestDismissed) {
  // Add two extensions that request access to all urls, and withhold their
  // site access.
  auto extension_a =
      InstallExtensionWithHostPermissions("Extension A", {"<all_urls>"});
  auto extension_b =
      InstallExtensionWithHostPermissions("Extension B", {"<all_urls>"});
  WithholdHostPermissions(extension_a.get());
  WithholdHostPermissions(extension_b.get());

  // By default, both extensions are allowed to show requests in requests access
  // button. However, request access button is not visible because we haven't
  // navigated to a site yet (and extensions haven't added any site access
  // requests).
  SitePermissionsHelper permissions_helper(browser()->GetProfile());
  EXPECT_TRUE(
      permissions_helper.ShowAccessRequestsInToolbar(extension_a->id()));
  EXPECT_TRUE(
      permissions_helper.ShowAccessRequestsInToolbar(extension_b->id()));
  EXPECT_FALSE(IsRequestAccessButtonVisible());

  // Navigate to an url that both extensions want access to, and add site access
  // requests for both.
  const GURL url("http://www.example.com");
  NavigateAndCommit(url);
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  AddHostAccessRequest(*extension_a, web_contents);
  AddHostAccessRequest(*extension_b, web_contents);

  // Verify request access button has both extensions.
  EXPECT_TRUE(IsRequestAccessButtonVisible());
  EXPECT_EQ(
      request_access_button()->GetText(),
      l10n_util::GetStringFUTF16Int(IDS_EXTENSIONS_REQUEST_ACCESS_BUTTON, 2));

  int tab_id = extensions::ExtensionTabUtil::GetTabId(web_contents);
  auto* permissions_manager = extensions::PermissionsManager::Get(profile());

  // Dismiss extension A's requests. Verify only extension B is visible in the
  // button.
  permissions_manager->UserDismissedHostAccessRequest(web_contents, tab_id,
                                                      extension_a->id());
  EXPECT_TRUE(IsRequestAccessButtonVisible());
  EXPECT_EQ(
      request_access_button()->GetText(),
      l10n_util::GetStringFUTF16Int(IDS_EXTENSIONS_REQUEST_ACCESS_BUTTON, 1));

  // Dismiss extension B's requests. Verify button is not visible anymore.
  permissions_manager->UserDismissedHostAccessRequest(web_contents, tab_id,
                                                      extension_b->id());
  EXPECT_FALSE(IsRequestAccessButtonVisible());
}

IN_PROC_BROWSER_TEST_F(ExtensionsToolbarDesktopBrowserTest,
                       RequestAccessButton_OnPressedExecuteAction) {
  auto extension =
      InstallExtensionWithHostPermissions("Extension", {"<all_urls>"});
  WithholdHostPermissions(extension.get());

  // Navigate to url and add a site access request for extension.
  const GURL url =
      embedded_test_server()->GetURL("example.com", "/title1.html");
  NavigateAndCommit(url);
  AddHostAccessRequest(*extension,
                       browser()->tab_strip_model()->GetActiveWebContents());
  LayoutContainerIfNecessary();

  constexpr char kActivatedUserAction[] =
      "Extensions.Toolbar.ExtensionsActivatedFromRequestAccessButton";
  base::UserActionTester user_action_tester;

  // Request access button is visible because the extension is requesting
  // access.
  ASSERT_TRUE(request_access_button()->GetVisible());
  EXPECT_EQ(user_action_tester.GetActionCount(kActivatedUserAction), 0);
  EXPECT_EQ(GetUserSiteAccess(*extension, url),
            PermissionsManager::UserSiteAccess::kOnClick);

  // Extension menu button has default state since extensions are not blocked,
  // and there is no extension with access to the site.
  EXPECT_EQ(GetCurrentButtonState(),
            ExtensionsToolbarViewModel::ExtensionsToolbarButtonState::kDefault);

  extensions::PermissionsManagerWaiter waiter(
      PermissionsManager::Get(profile()));
  ClickButton(request_access_button());
  waiter.WaitForExtensionPermissionsUpdate();
  WaitForAnimation();
  LayoutContainerIfNecessary();

  // Verify extension was executed and extensions menu button has "any
  // extension has access" state. Extension's site access should be "on site",
  // since clicking the button grants always access to that site.
  EXPECT_EQ(user_action_tester.GetActionCount(kActivatedUserAction), 1);
  EXPECT_EQ(GetCurrentButtonState(),
            ExtensionsToolbarViewModel::ExtensionsToolbarButtonState::
                kAnyExtensionHasAccess);
  EXPECT_EQ(GetUserSiteAccess(*extension, url),
            PermissionsManager::UserSiteAccess::kOnSite);

  // Verify confirmation message appears on the request access button.
  EXPECT_TRUE(request_access_button()->GetVisible());
  EXPECT_EQ(request_access_button()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_EXTENSIONS_REQUEST_ACCESS_BUTTON_DISMISSED_TEXT));

  // Force the confirmation to be collapsed.
  extensions_container()->CollapseConfirmation();
  WaitForAnimation();

  // Verify the request access button is hidden.
  ASSERT_FALSE(request_access_button()->GetVisible());
}

// Tests that if an update comes in between the request access button is clicked
// and the confirmation is collapsed, the button is updated afterwards with the
// correct information.
// TODO(crbug.com/547559833): Re-enable once fixed.
IN_PROC_BROWSER_TEST_F(
    ExtensionsToolbarDesktopBrowserTest,
    DISABLED_RequestAccessButton_UpdateInBetweenClickAndConfirmationCollapse) {
  auto extension_A =
      InstallExtensionWithHostPermissions("Extension A", {"<all_urls>"});
  auto extension_B =
      InstallExtensionWithHostPermissions("Extension B", {"<all_urls>"});
  auto extension_C =
      InstallExtensionWithHostPermissions("Extension C", {"<all_urls>"});
  WithholdHostPermissions(extension_A.get());
  WithholdHostPermissions(extension_B.get());
  WithholdHostPermissions(extension_C.get());

  const GURL url("http://www.example.com");
  NavigateAndCommit(url);
  LayoutContainerIfNecessary();

  // Add site access requests for extension A and B.
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  AddHostAccessRequest(*extension_A, web_contents);
  AddHostAccessRequest(*extension_B, web_contents);
  LayoutContainerIfNecessary();

  // Request access button is visible because extension A and B have site access
  // requests.
  EXPECT_TRUE(request_access_button()->GetVisible());
  EXPECT_THAT(request_access_button()->GetExtensionIdsForTesting(),
              testing::ElementsAre(extension_A->id(), extension_B->id()));

  ClickButton(request_access_button());
  WaitForAnimation();
  LayoutContainerIfNecessary();

  // Verify confirmation message appears on the request access button after
  // clicking on it
  EXPECT_TRUE(request_access_button()->GetVisible());
  EXPECT_EQ(request_access_button()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_EXTENSIONS_REQUEST_ACCESS_BUTTON_DISMISSED_TEXT));

  // Add a site access request for extension C before the confirmation is
  // collapsed.
  AddHostAccessRequest(*extension_C, web_contents);

  // Confirmation is still showing since collapse time hasn't elapsed.
  EXPECT_TRUE(request_access_button()->GetVisible());
  EXPECT_EQ(request_access_button()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_EXTENSIONS_REQUEST_ACCESS_BUTTON_DISMISSED_TEXT));

  // Force the confirmation to be collapsed.
  extensions_container()->CollapseConfirmation();
  WaitForAnimation();

  // Verify the request access button is visible since extension C is now
  // requesting access.
  EXPECT_TRUE(request_access_button()->GetVisible());
  EXPECT_THAT(request_access_button()->GetExtensionIdsForTesting(),
              testing::ElementsAre(extension_C->id()));
}

class ExtensionsToolbarDesktopWithPermittedSitesBrowserTest
    : public ExtensionsToolbarDesktopBrowserTest {
 public:
  ExtensionsToolbarDesktopWithPermittedSitesBrowserTest()
      : ExtensionsToolbarDesktopBrowserTest(
            {extensions_features::
                 kExtensionsMenuAccessControlWithPermittedSites},
            {}) {}
  ExtensionsToolbarDesktopWithPermittedSitesBrowserTest(
      const ExtensionsToolbarDesktopWithPermittedSitesBrowserTest&) = delete;
  const ExtensionsToolbarDesktopWithPermittedSitesBrowserTest& operator=(
      const ExtensionsToolbarDesktopWithPermittedSitesBrowserTest&) = delete;
  ~ExtensionsToolbarDesktopWithPermittedSitesBrowserTest() override = default;
};

// Test that request access button is visible based on the user site setting
// selected.
IN_PROC_BROWSER_TEST_F(ExtensionsToolbarDesktopWithPermittedSitesBrowserTest,
                       RequestAccessButtonVisibilityOnPermittedSites) {
  const GURL url("http://www.url.com");

  // Install an extension and withhold permissions so request access button can
  // be visible.
  auto extension =
      InstallExtensionWithHostPermissions("Extension", {"<all_urls>"});
  WithholdHostPermissions(extension.get());

  // Navigate to a site and add a site access request for the extension.
  NavigateAndCommit(url);
  auto url_origin =
      web_contents()->GetPrimaryMainFrame()->GetLastCommittedOrigin();
  AddHostAccessRequest(*extension, web_contents());

  // A site has "customize by extensions" site setting by default,
  ASSERT_EQ(GetUserSiteSetting(url),
            PermissionsManager::UserSiteSetting::kCustomizeByExtension);
  EXPECT_TRUE(IsRequestAccessButtonVisible());

  // Request access button is not visible in permitted sites.
  auto* manager = PermissionsManager::Get(profile());
  extensions::PermissionsManagerWaiter waiter(manager);
  manager->AddUserPermittedSite(url_origin);
  waiter.WaitForUserPermissionsSettingsChange();
  WaitForAnimation();

  // Request access button visibility is the same for other site settings, which
  // is already tested, regardless of whether permitted sites are supported or
  // not.
}

// Tests that when ToolbarActionsModel shuts down, extensions_container detaches
// and destroys all ToolbarActionView instances before the action view models
// are freed, preventing Use-After-Free crashes.
IN_PROC_BROWSER_TEST_F(ExtensionsToolbarDesktopBrowserTest,
                       OnToolbarActionsModelShutdown_DetachesActionViews) {
  auto extension = InstallExtension("Extension");
  auto* toolbar_model = ToolbarActionsModel::Get(profile());
  toolbar_model->SetActionVisibility(extension->id(), true);
  WaitForAnimation();

  ToolbarActionView* action_view =
      extensions_container()->GetViewForId(extension->id());
  ASSERT_TRUE(action_view);

  extensions_container()
      ->GetToolbarViewModel()
      ->OnToolbarActionsModelShutdown();

  // Verify that action_view was safely removed from the extensions container.
  EXPECT_EQ(nullptr, extensions_container()->GetViewForId(extension->id()));
}

class ExtensionsToolbarDesktopAccessControlDisabledBrowserTest
    : public ExtensionsToolbarBrowserTest {
 public:
  ExtensionsToolbarDesktopAccessControlDisabledBrowserTest()
      : ExtensionsToolbarBrowserTest(
            {},
            {extensions_features::kExtensionsMenuAccessControl}) {}
  ExtensionsToolbarDesktopAccessControlDisabledBrowserTest(
      const ExtensionsToolbarDesktopAccessControlDisabledBrowserTest&) = delete;
  ExtensionsToolbarDesktopAccessControlDisabledBrowserTest& operator=(
      const ExtensionsToolbarDesktopAccessControlDisabledBrowserTest&) = delete;
  ~ExtensionsToolbarDesktopAccessControlDisabledBrowserTest() override =
      default;
};

// Tests that when #extensions-menu-access-control is disabled, hovering over a
// pinned extension highlights the container and unhovering clears the container
// highlight.
IN_PROC_BROWSER_TEST_F(ExtensionsToolbarDesktopAccessControlDisabledBrowserTest,
                       HighlightClearedOnUnhover) {
  auto extension = InstallExtension("Extension");
  auto* toolbar_model = ToolbarActionsModel::Get(profile());
  toolbar_model->SetActionVisibility(extension->id(), true);
  WaitForAnimation();

  ToolbarActionView* action_view =
      extensions_container()->GetViewForId(extension->id());
  ASSERT_TRUE(action_view);

  // Activate the widget.
  views::Widget* widget = extensions_container()->GetWidget();
  widget->OnNativeWidgetActivationChanged(true);
  EXPECT_TRUE(widget->ShouldPaintAsActive());

  // Hover over the pinned extension action.
  action_view->SetState(views::Button::ButtonState::STATE_HOVERED);
  EXPECT_EQ(action_view->GetState(), views::Button::ButtonState::STATE_HOVERED);
  EXPECT_TRUE(extensions_container()->GetHighlighted());

  // Unhover the pinned extension action.
  action_view->SetState(views::Button::ButtonState::STATE_NORMAL);

  // Verify container highlight is cleared.
  EXPECT_FALSE(extensions_container()->GetHighlighted());
}
