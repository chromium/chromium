// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"

#include "base/json/json_reader.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_unittest.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"

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
