// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"

#include "base/json/json_reader.h"
#include "base/ranges/algorithm.h"
#include "base/test/metrics/user_action_tester.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_action_runner.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_unittest.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "extensions/browser/pref_names.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_id.h"
#include "extensions/test/permissions_manager_waiter.h"
#include "extensions/test/test_extension_dir.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"

namespace {

using SitePermissionsHelper = extensions::SitePermissionsHelper;
using PermissionsManager = extensions::PermissionsManager;

// TODO(crbug.com/40916158): Same as permission's ChipController. Pull out to a
// shared location.
base::TimeDelta kConfirmationDisplayDuration = base::Seconds(4);

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
  raw_ptr<BrowserView, DanglingUntriaged> browser_view_;
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

  // Navigates to `url`.
  void NavigateAndCommit(const GURL& URL);

  // Returns the view of the given `extension_id` if the extension is currently
  // pinned.
  ToolbarActionView* GetPinnedExtensionView(
      const extensions::ExtensionId& extension_id);

  // Returns whether the request access button is visible or not.
  bool IsRequestAccessButtonVisible();

  // ExtensionsToolbarUnitTest:
  void SetUp() override;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<content::WebContentsTester, DanglingUntriaged> web_contents_tester_;
};

ExtensionsToolbarContainerUnitTest::ExtensionsToolbarContainerUnitTest()
    : ExtensionsToolbarUnitTest(
          base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
  scoped_feature_list_.InitAndEnableFeature(
      extensions_features::kExtensionsMenuAccessControl);
}

void ExtensionsToolbarContainerUnitTest::NavigateAndCommit(const GURL& url) {
  web_contents_tester_->NavigateAndCommit(url);
  WaitForAnimation();
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

bool ExtensionsToolbarContainerUnitTest::IsRequestAccessButtonVisible() {
  return request_access_button()->GetVisible();
}

void ExtensionsToolbarContainerUnitTest::SetUp() {
  ExtensionsToolbarUnitTest::SetUp();
  web_contents_tester_ = AddWebContentsAndGetTester();
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
  std::optional<base::Value> settings = base::JSONReader::Read(json);
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

// Test that the extension button state changes after site permissions updates.
TEST_F(ExtensionsToolbarContainerUnitTest,
       ExtensionsButton_SitePermissionsUpdates) {
  // Install an extension that requests host permissions.
  auto extension =
      InstallExtensionWithHostPermissions("Extension", {"<all_urls>"});

  const GURL url("http://www.url.com");
  auto url_origin = url::Origin::Create(url);
  NavigateAndCommit(url);

  auto* manager = extensions::PermissionsManager::Get(profile());
  {
    // Extensions button has "all extensions blocked" icon type when it's
    // an user restricted site.
    extensions::PermissionsManagerWaiter manager_waiter(manager);
    manager->AddUserRestrictedSite(url_origin);
    manager_waiter.WaitForUserPermissionsSettingsChange();
    WaitForAnimation();
    EXPECT_EQ(extensions_button()->state(),
              ExtensionsToolbarButton::State::kAllExtensionsBlocked);
  }

  {
    // Extensions button has "any extension has access" icon type when it's not
    // an user restricted site and 1+ extensions have
    // site access granted. Note that by default extensions have granted access.
    extensions::PermissionsManagerWaiter manager_waiter(manager);
    manager->RemoveUserRestrictedSite(url_origin);
    manager_waiter.WaitForUserPermissionsSettingsChange();
    WaitForAnimation();
    EXPECT_EQ(extensions_button()->state(),
              ExtensionsToolbarButton::State::kAnyExtensionHasAccess);
  }

  {
    // Extension button has "default" icon type when it's not an user restricted
    // site and no extensions have site access granted.
    // To achieve this, we withhold host permissions in the only extension
    // installed.
    WithholdHostPermissions(extension.get());
    WaitForAnimation();
    EXPECT_EQ(extensions_button()->state(),
              ExtensionsToolbarButton::State::kDefault);
  }
}

// Test that the extension button state takes into account chrome restricted
// sites.
TEST_F(ExtensionsToolbarContainerUnitTest,
       ExtensionsButton_ChromeRestrictedSite) {
  InstallExtensionWithHostPermissions("Extension", {"<all_urls>"});

  const GURL restricted_url("chrome://extensions");
  NavigateAndCommit(restricted_url);

  // Extensions button has "all extensions blocked" icon type for chrome
  // restricted sites.
  EXPECT_EQ(extensions_button()->state(),
            ExtensionsToolbarButton::State::kAllExtensionsBlocked);
}

// Tests that extensions appear in the request access button iff they have a
// site access request.
TEST_F(ExtensionsToolbarContainerUnitTest, RequestAccessButton_Extensions) {
  auto extension_A =
      InstallExtensionWithPermissions("Extension A", {"activeTab"});
  auto extension_B =
      InstallExtensionWithHostPermissions("Extension B", {"*://www.b.com/*"});
  auto extension_C =
      InstallExtensionWithHostPermissions("Extension C", {"<all_urls>"});
  WithholdHostPermissions(extension_B.get());
  WithholdHostPermissions(extension_C.get());

  // Navigate to a site only explicitly requested by extension C. Verify
  // request access button is not visible, since no extension has added a
  // request.
  NavigateAndCommit(GURL("http://www.other.com"));
  EXPECT_FALSE(IsRequestAccessButtonVisible());

  // Add a site access request for extension A and verify it's not visible on
  // the request access button since extensions with only activeTab can't add a
  // request.
  AddSiteAccessRequest(*extension_A,
                       browser()->tab_strip_model()->GetActiveWebContents());
  EXPECT_FALSE(IsRequestAccessButtonVisible());

  // Add a site access request for extension B and verify it's not visible on
  // the request access button since extension didn't request access for the
  // current site.
  AddSiteAccessRequest(*extension_B,
                       browser()->tab_strip_model()->GetActiveWebContents());
  EXPECT_FALSE(IsRequestAccessButtonVisible());

  // Add a site access request for extension C and verify it's visible on the
  // request access button.
  AddSiteAccessRequest(*extension_C,
                       browser()->tab_strip_model()->GetActiveWebContents());
  EXPECT_TRUE(IsRequestAccessButtonVisible());
  EXPECT_THAT(request_access_button()->GetExtensionIdsForTesting(),
              testing::ElementsAre(extension_C->id()));

  // Navigate to a site only explicitly requested by extension B and C. Verify
  // request access button is not visible, since requests are reset on
  // cross-origin navigations.
  NavigateAndCommit(GURL("http://www.b.com"));
  EXPECT_FALSE(IsRequestAccessButtonVisible());

  // Add a site access request for extension B and verify it's visible on
  // the request access button.
  AddSiteAccessRequest(*extension_B,
                       browser()->tab_strip_model()->GetActiveWebContents());
  EXPECT_TRUE(IsRequestAccessButtonVisible());
  EXPECT_THAT(request_access_button()->GetExtensionIdsForTesting(),
              testing::ElementsAre(extension_B->id()));

  // Add a site access request for extension C and verify it's visible on the
  // request access button.
  AddSiteAccessRequest(*extension_C,
                       browser()->tab_strip_model()->GetActiveWebContents());
  EXPECT_TRUE(IsRequestAccessButtonVisible());
  EXPECT_THAT(request_access_button()->GetExtensionIdsForTesting(),
              testing::ElementsAre(extension_B->id(), extension_C->id()));

  // Remove the site access request for extension B and verify only extension
  // C is visible on the request access button.
  RemoveSiteAccessRequest(*extension_B,
                          browser()->tab_strip_model()->GetActiveWebContents());
  EXPECT_TRUE(IsRequestAccessButtonVisible());
  EXPECT_THAT(request_access_button()->GetExtensionIdsForTesting(),
              testing::ElementsAre(extension_C->id()));

  // Remove the site access request for extension C and verify request access
  // button is not visible.
  RemoveSiteAccessRequest(*extension_C,
                          browser()->tab_strip_model()->GetActiveWebContents());
  EXPECT_FALSE(IsRequestAccessButtonVisible());
}

// Tests that an extension appears in the request access button iff it has a
// site access request that matches the given pattern filter.
TEST_F(ExtensionsToolbarContainerUnitTest,
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
                    "http://www.other.com/");
  AddSiteAccessRequest(
      *extension, browser()->tab_strip_model()->GetActiveWebContents(), filter);
  EXPECT_FALSE(IsRequestAccessButtonVisible());

  // Add a site access request with filter that matches the current web
  // contents. Verify extension is visible on the request access button.
  filter = URLPattern(extensions::Extension::kValidHostPermissionSchemes,
                      "http://www.example.com/");
  AddSiteAccessRequest(
      *extension, browser()->tab_strip_model()->GetActiveWebContents(), filter);
  EXPECT_TRUE(IsRequestAccessButtonVisible());
  EXPECT_THAT(request_access_button()->GetExtensionIdsForTesting(),
              testing::ElementsAre(extension->id()));

  // Add a site access request with filter that does not match the current web
  // contents. Verify request access button is hidden (previous request was
  // removed).
  filter = URLPattern(extensions::Extension::kValidHostPermissionSchemes,
                      "http://www.other.com/");
  AddSiteAccessRequest(
      *extension, browser()->tab_strip_model()->GetActiveWebContents(), filter);
  EXPECT_FALSE(IsRequestAccessButtonVisible());
}

// Tests that an extension's site access request is removed when the extension
// is granted site access.
TEST_F(ExtensionsToolbarContainerUnitTest,
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
  AddSiteAccessRequest(*extension_A, web_contents);
  AddSiteAccessRequest(*extension_B, web_contents);
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
TEST_F(ExtensionsToolbarContainerUnitTest,
       RequestAccessButtonVisibility_NavigationBetweenPages) {
  auto extension =
      InstallExtensionWithHostPermissions("Extension", {"<all_urls>"});
  WithholdHostPermissions(extension.get());

  NavigateAndCommit(GURL("http://www.a.com"));
  AddSiteAccessRequest(*extension,
                       browser()->tab_strip_model()->GetActiveWebContents());

  EXPECT_TRUE(IsRequestAccessButtonVisible());
  EXPECT_THAT(request_access_button()->GetExtensionIdsForTesting(),
              testing::ElementsAre(extension->id()));

  // Navigate to a same-origin site and verify request access button has
  // extension.
  NavigateAndCommit(GURL("http://www.a.com/path"));
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
TEST_F(ExtensionsToolbarContainerUnitTest,
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
                    "*://*/path");
  AddSiteAccessRequest(
      *extension, browser()->tab_strip_model()->GetActiveWebContents(), filter);
  EXPECT_FALSE(IsRequestAccessButtonVisible());

  // Navigate to a same-origin site that matches the filter. Verify extension is
  // visible on the request access button.
  NavigateAndCommit(GURL("http://www.example.com/path"));
  EXPECT_TRUE(IsRequestAccessButtonVisible());
  EXPECT_THAT(request_access_button()->GetExtensionIdsForTesting(),
              testing::ElementsAre(extension->id()));

  // Add site access request for extension with a filter that doesn't have the
  // same origin as the current web contents. Verify request access button is
  // hidden.
  filter = URLPattern(extensions::Extension::kValidHostPermissionSchemes,
                      "http://www.other.com/path");
  AddSiteAccessRequest(
      *extension, browser()->tab_strip_model()->GetActiveWebContents(), filter);
  EXPECT_FALSE(IsRequestAccessButtonVisible());

  // Navigate to a cross-origin site that matches the filters. Since it's a
  // cross-origin navigation, requests are reset. Therefore, verify request
  // access button is hidden.
  NavigateAndCommit(GURL("http://www.other.com/path"));
  EXPECT_FALSE(IsRequestAccessButtonVisible());
}

// Test that request access button is visible based on the user site setting
// selected.
TEST_F(ExtensionsToolbarContainerUnitTest,
       RequestAccessButton_UserSiteSetting) {
  const GURL url("http://www.url.com");
  auto url_origin = url::Origin::Create(url);

  // Install an extension and withhold permissions so request access button can
  // be visible.
  auto extension =
      InstallExtensionWithHostPermissions("Extension", {"<all_urls>"});
  WithholdHostPermissions(extension.get());

  // Navigate to url and add a site request for the extension.
  NavigateAndCommit(url);
  AddSiteAccessRequest(*extension,
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
TEST_F(ExtensionsToolbarContainerUnitTest,
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
  SitePermissionsHelper permissions_helper(browser()->profile());
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
  AddSiteAccessRequest(*extension_a, web_contents);
  AddSiteAccessRequest(*extension_b, web_contents);

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
TEST_F(ExtensionsToolbarContainerUnitTest,
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
  SitePermissionsHelper permissions_helper(browser()->profile());
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
  AddSiteAccessRequest(*extension_a, web_contents);
  AddSiteAccessRequest(*extension_b, web_contents);

  // Verify request access button has both extensions.
  EXPECT_TRUE(IsRequestAccessButtonVisible());
  EXPECT_EQ(
      request_access_button()->GetText(),
      l10n_util::GetStringFUTF16Int(IDS_EXTENSIONS_REQUEST_ACCESS_BUTTON, 2));

  int tab_id = extensions::ExtensionTabUtil::GetTabId(web_contents);
  auto* permissions_manager = extensions::PermissionsManager::Get(profile());

  // Dismiss extension A's requests. Verify only extension B is visible in the
  // button.
  permissions_manager->UserDismissedSiteAccessRequest(web_contents, tab_id,
                                                      extension_a->id());
  EXPECT_TRUE(IsRequestAccessButtonVisible());
  EXPECT_EQ(
      request_access_button()->GetText(),
      l10n_util::GetStringFUTF16Int(IDS_EXTENSIONS_REQUEST_ACCESS_BUTTON, 1));

  // Dismiss extension B's requests. Verify button is not visible anymore.
  permissions_manager->UserDismissedSiteAccessRequest(web_contents, tab_id,
                                                      extension_b->id());
  EXPECT_FALSE(IsRequestAccessButtonVisible());
}

TEST_F(ExtensionsToolbarContainerUnitTest,
       RequestAccessButton_OnPressedExecuteAction) {
  auto extension =
      InstallExtensionWithHostPermissions("Extension", {"<all_urls>"});
  WithholdHostPermissions(extension.get());

  // Navigate to url and add a site access request for extension.
  const GURL url("http://www.example.com");
  NavigateAndCommit(url);
  AddSiteAccessRequest(*extension,
                       browser()->tab_strip_model()->GetActiveWebContents());
  LayoutContainerIfNecessary();

  constexpr char kActivatedUserAction[] =
      "Extensions.Toolbar.ExtensionsActivatedFromRequestAccessButton";
  base::UserActionTester user_action_tester;
  auto* permissions = PermissionsManager::Get(profile());

  // Request access button is visible because the extension is requesting
  // access.
  ASSERT_TRUE(request_access_button()->GetVisible());
  EXPECT_EQ(user_action_tester.GetActionCount(kActivatedUserAction), 0);
  EXPECT_EQ(permissions->GetUserSiteAccess(*extension, url),
            PermissionsManager::UserSiteAccess::kOnClick);

  // Extension menu button has default state since extensions are not blocked,
  // and there is no extension with access to the site.
  EXPECT_EQ(extensions_button()->state(),
            ExtensionsToolbarButton::State::kDefault);

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
  EXPECT_EQ(extensions_button()->state(),
            ExtensionsToolbarButton::State::kAnyExtensionHasAccess);
  EXPECT_EQ(permissions->GetUserSiteAccess(*extension, url),
            PermissionsManager::UserSiteAccess::kOnSite);

  // Verify confirmation message appears on the request access button.
  EXPECT_TRUE(request_access_button()->GetVisible());
  EXPECT_EQ(request_access_button()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_EXTENSIONS_REQUEST_ACCESS_BUTTON_DISMISSED_TEXT));

  // Force the confirmation to be collapsed.
  task_environment()->AdvanceClock(kConfirmationDisplayDuration);
  base::RunLoop().RunUntilIdle();
  WaitForAnimation();

  // Verify the request access button is hidden.
  ASSERT_FALSE(request_access_button()->GetVisible());
}

// Tests that if an update comes in between the request access button is clicked
// and the confirmation is collapsed, the button is updated afterwards with the
// correct information.
TEST_F(ExtensionsToolbarContainerUnitTest,
       RequestAccessButton_UpdateInBetweenClickAndConfirmationCollapse) {
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
  AddSiteAccessRequest(*extension_A, web_contents);
  AddSiteAccessRequest(*extension_B, web_contents);
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
  AddSiteAccessRequest(*extension_C, web_contents);

  // Confirmation is still showing since collapse time hasn't elapsed.
  EXPECT_TRUE(request_access_button()->GetVisible());
  EXPECT_EQ(request_access_button()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_EXTENSIONS_REQUEST_ACCESS_BUTTON_DISMISSED_TEXT));

  // Force the confirmation to be collapsed.
  task_environment()->AdvanceClock(kConfirmationDisplayDuration);
  base::RunLoop().RunUntilIdle();

  // Verify the request access button is visible since extension C is now
  // requesting access.
  EXPECT_TRUE(request_access_button()->GetVisible());
  EXPECT_THAT(request_access_button()->GetExtensionIdsForTesting(),
              testing::ElementsAre(extension_C->id()));
}

class ExtensionsToolbarContainerWithPermittedSitesUnitTest
    : public ExtensionsToolbarContainerUnitTest {
 public:
  ExtensionsToolbarContainerWithPermittedSitesUnitTest() {
    std::vector<base::test::FeatureRef> enabled_features = {
        extensions_features::kExtensionsMenuAccessControl,
        extensions_features::kExtensionsMenuAccessControlWithPermittedSites};
    std::vector<base::test::FeatureRef> disabled_features;
    feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }
  ExtensionsToolbarContainerWithPermittedSitesUnitTest(
      const ExtensionsToolbarContainerWithPermittedSitesUnitTest&) = delete;
  const ExtensionsToolbarContainerWithPermittedSitesUnitTest& operator=(
      const ExtensionsToolbarContainerWithPermittedSitesUnitTest&) = delete;
  ~ExtensionsToolbarContainerWithPermittedSitesUnitTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Test that request access button is visible based on the user site setting
// selected.
TEST_F(ExtensionsToolbarContainerWithPermittedSitesUnitTest,
       RequestAccessButtonVisibilityOnPermittedSites) {
  const GURL url("http://www.url.com");
  auto url_origin = url::Origin::Create(url);

  // Install an extension and withhold permissions so request access button can
  // be visible.
  auto extension =
      InstallExtensionWithHostPermissions("Extension", {"<all_urls>"});
  WithholdHostPermissions(extension.get());

  // Navigate to a site and add a site access request for the extension.
  NavigateAndCommit(url);
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  AddSiteAccessRequest(*extension, web_contents);

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
