// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"

#include "base/json/json_reader.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_unittest.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "extensions/browser/pref_names.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_id.h"
#include "extensions/test/test_extension_dir.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"

namespace {

// A scoper that manages a Browser instance created by BrowserWithTestWindowTest
// beyond the default instance it creates in SetUp.
class AdditionalBrowser {
 public:
  explicit AdditionalBrowser(std::unique_ptr<Browser> browser)
      : browser_(std::move(browser)),
        browser_view_(BrowserView::GetBrowserViewForBrowser(browser_.get())) {}

  ~AdditionalBrowser() {
    // Tear down `browser_`, similar to TestWithBrowserView::TearDown.
    browser_.release();
    browser_view_->GetWidget()->CloseNow();
  }

  ExtensionsToolbarContainer* extensions_container() {
    return browser_view_->toolbar()->extensions_container();
  }

 private:
  std::unique_ptr<Browser> browser_;
  raw_ptr<BrowserView> browser_view_;
};

}  // namespace

class ExtensionsToolbarContainerUnitTest : public ExtensionsToolbarUnitTest {
 public:
  ExtensionsToolbarContainerUnitTest();
  ~ExtensionsToolbarContainerUnitTest() override = default;
  ExtensionsToolbarContainerUnitTest(
      const ExtensionsToolbarContainerUnitTest&) = delete;
  ExtensionsToolbarContainerUnitTest& operator=(
      const ExtensionsToolbarContainerUnitTest&) = delete;

  // Returns the view of the given `extension_id` if the extension is currently
  // pinned.
  ToolbarActionView* GetPinnedExtensionView(
      const extensions::ExtensionId& extension_id);

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

ExtensionsToolbarContainerUnitTest::ExtensionsToolbarContainerUnitTest() {
  scoped_feature_list_.InitAndEnableFeature(
      extensions_features::kExtensionsMenuAccessControl);
}

ToolbarActionView* ExtensionsToolbarContainerUnitTest::GetPinnedExtensionView(
    const extensions::ExtensionId& extension_id) {
  std::vector<ToolbarActionView*> actions = GetPinnedExtensionViews();
  auto it =
      base::ranges::find(actions, extension_id, [](ToolbarActionView* action) {
        return action->view_controller()->GetId();
      });
  if (it == actions.end())
    return nullptr;
  return *it;
}

TEST_F(ExtensionsToolbarContainerUnitTest, ReorderPinnedExtensions) {
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

TEST_F(ExtensionsToolbarContainerUnitTest, ForcePinnedExtensionsCannotReorder) {
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
  absl::optional<base::Value> settings = base::JSONReader::Read(json);
  ASSERT_TRUE(settings.has_value());
  profile()->GetTestingPrefService()->SetManagedPref(
      extensions::pref_names::kExtensionManagement,
      base::Value::ToUniquePtrValue(std::move(settings.value())));

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
TEST_F(ExtensionsToolbarContainerUnitTest, ReloadExtensionKeepsPinnedState) {
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

  // By default, extension on installation is unpinned.
  EXPECT_FALSE(
      extensions_container()->IsActionVisibleOnToolbar(extension->id()));

  // Pin extension and verify it is visible on the toolbar.
  auto* toolbar_model = ToolbarActionsModel::Get(profile());
  ASSERT_TRUE(toolbar_model);
  toolbar_model->SetActionVisibility(extension->id(), true);
  EXPECT_TRUE(
      extensions_container()->IsActionVisibleOnToolbar(extension->id()));

  // Reload the extension.
  extensions::TestExtensionRegistryObserver registry_observer(
      extensions::ExtensionRegistry::Get(profile()));
  ReloadExtension(extension->id());
  ASSERT_TRUE(registry_observer.WaitForExtensionLoaded());
  WaitForAnimation();

  // Verify the extension is visible on the toolbar.
  EXPECT_TRUE(
      extensions_container()->IsActionVisibleOnToolbar(extension->id()));
}

// Tests that a when an extension is reloaded with manifest errors, and
// therefore fails to be loaded into Chrome, it's removed from the toolbar.
TEST_F(ExtensionsToolbarContainerUnitTest, ReloadExtensionFailed) {
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

  // By default, extension on installation is unpinned.
  EXPECT_FALSE(
      extensions_container()->IsActionVisibleOnToolbar(extension->id()));

  // Pin extension and verify it is visible on the toolbar.
  auto* toolbar_model = ToolbarActionsModel::Get(profile());
  ASSERT_TRUE(toolbar_model);
  toolbar_model->SetActionVisibility(extension->id(), true);
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
  extension_service()->ReloadExtensionWithQuietFailure(extension->id());
  base::RunLoop().RunUntilIdle();
  WaitForAnimation();

  // Verify the extension is no longer visible on the toolbar.
  EXPECT_FALSE(
      extensions_container()->IsActionVisibleOnToolbar(extension->id()));
}

TEST_F(ExtensionsToolbarContainerUnitTest,
       PinnedExtensionAppearsInAnotherWindow) {
  const std::string& extension_id = InstallExtension("Extension")->id();

  AdditionalBrowser browser2(
      CreateBrowser(browser()->profile(), browser()->type(),
                    /* hosted_app */ false, /* browser_window */ nullptr));

  // Verify extension is unpinned in both windows.
  EXPECT_FALSE(extensions_container()->IsActionVisibleOnToolbar(extension_id));
  EXPECT_FALSE(
      browser2.extensions_container()->IsActionVisibleOnToolbar(extension_id));

  // Pin extension in one window.
  auto* toolbar_model = ToolbarActionsModel::Get(profile());
  ASSERT_TRUE(toolbar_model);
  toolbar_model->SetActionVisibility(extension_id, true);

  // Both windows open get the pinned extension.
  EXPECT_TRUE(extensions_container()->IsActionVisibleOnToolbar(extension_id));
  EXPECT_TRUE(
      browser2.extensions_container()->IsActionVisibleOnToolbar(extension_id));

  AdditionalBrowser browser3(
      CreateBrowser(browser()->profile(), browser()->type(),
                    /* hosted_app */ false, /* browser_window */ nullptr));

  // Brand-new window also gets the pinned extension.
  EXPECT_TRUE(
      browser3.extensions_container()->IsActionVisibleOnToolbar(extension_id));
}

TEST_F(ExtensionsToolbarContainerUnitTest,
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

TEST_F(ExtensionsToolbarContainerUnitTest, RunDropCallback) {
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

TEST_F(ExtensionsToolbarContainerUnitTest, ResetDropCallback) {
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

TEST_F(ExtensionsToolbarContainerUnitTest,
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

// ToolbarActionsModel::MovePinnedAction crashes if pinned extensions changes
// while the drop callback isn't invalidated. This test makes sure this doesn't
// happen anymore. https://crbug.com/1268239.
TEST_F(ExtensionsToolbarContainerUnitTest, InvalidateDropCallbackOnPrefChange) {
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
