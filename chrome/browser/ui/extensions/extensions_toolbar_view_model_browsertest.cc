// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extensions_toolbar_view_model.h"

#include "base/memory/raw_ptr.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/extension_view_host.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/extensions/extension_action_delegate.h"
#include "chrome/browser/ui/extensions/extension_action_view_model.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "components/crx_file/id_util.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/permissions_manager.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_builder.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/origin.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

using testing::_;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;

namespace {

// Minimal stub for ExtensionActionDelegate.
class FakeExtensionActionDelegate : public ExtensionActionDelegate {
 public:
  void AttachToModel(ExtensionActionViewModel* model) override {}
  void DetachFromModel() override {}
  void RegisterCommand() override {}
  void UnregisterCommand() override {}
  bool IsShowingPopup() const override { return false; }
  void HidePopup() override {}
  gfx::NativeView GetPopupNativeView() override { return gfx::NativeView(); }
  void TriggerPopup(std::unique_ptr<extensions::ExtensionViewHost> host,
                    PopupShowAction show_action,
                    bool by_user,
                    ShowPopupCallback callback) override {}
  void ShowContextMenuAsFallback() override {}
  bool CloseOverflowMenuIfOpen() override { return false; }
};

// The test delegate that acts as the factory for Action ViewModels.
class TestExtensionsToolbarDelegate
    : public ExtensionsToolbarViewModel::Delegate {
 public:
  explicit TestExtensionsToolbarDelegate(BrowserWindowInterface* browser)
      : browser_(browser) {}
  ~TestExtensionsToolbarDelegate() override = default;

  std::unique_ptr<ExtensionActionViewModel> CreateActionViewModel(
      const ToolbarActionsModel::ActionId& action_id,
      ExtensionsContainer* extensions_container) override {
    return ExtensionActionViewModel::Create(
        action_id, browser_, std::make_unique<FakeExtensionActionDelegate>());
  }

  void HideActivePopup() override {}
  bool CloseOverflowMenuIfOpen() override { return false; }
  void ToggleExtensionsMenu() override {}
  bool CanShowToolbarActionPopupForAPICall(
      const std::string& action_id) override {
    return true;
  }

 private:
  raw_ptr<BrowserWindowInterface> browser_;
};

class MockExtensionsToolbarObserver
    : public ExtensionsToolbarViewModel::Observer {
 public:
  MockExtensionsToolbarObserver() = default;
  ~MockExtensionsToolbarObserver() override = default;

  MOCK_METHOD(void, OnActionsInitialized, (), (override));
  MOCK_METHOD(void,
              OnActionAdded,
              (const ToolbarActionsModel::ActionId&),
              (override));
  MOCK_METHOD(void,
              OnActionRemoved,
              (const ToolbarActionsModel::ActionId&),
              (override));
  MOCK_METHOD(void,
              OnActionUpdated,
              (const ToolbarActionsModel::ActionId&),
              (override));
  MOCK_METHOD(void, OnPinnedActionsChanged, (), (override));
  MOCK_METHOD(void, OnActiveWebContentsChanged, (bool), (override));
};

}  // namespace

class ExtensionsToolbarViewModelBrowserTest
    : public extensions::ExtensionBrowserTest {
 public:
  ExtensionsToolbarViewModelBrowserTest() = default;
  ~ExtensionsToolbarViewModelBrowserTest() override = default;

  // Adds a basic extension.
  scoped_refptr<const extensions::Extension> AddExtension(
      const std::string& name);

  // Adds an `extension` with the given `host_permissions`,
  // `permissions` and `location`.
  scoped_refptr<const extensions::Extension> AddExtension(
      const std::string& name,
      const std::vector<std::string>& permissions,
      const std::vector<std::string>& host_permissions,
      bool withhold_permissions = false);

  // Navigates the active web contents to a URL on `host_name`.
  void NavigateTo(std::string_view host_name);

  // ExtensionBrowserTest:
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

  ExtensionsToolbarViewModel* toolbar_model() { return toolbar_model_.get(); }
  MockExtensionsToolbarObserver& mock_observer() { return mock_observer_; }

 private:
  std::unique_ptr<TestExtensionsToolbarDelegate> toolbar_delegate_;
  std::unique_ptr<ExtensionsToolbarViewModel> toolbar_model_;

  testing::NiceMock<MockExtensionsToolbarObserver> mock_observer_;
};

scoped_refptr<const extensions::Extension>
ExtensionsToolbarViewModelBrowserTest::AddExtension(const std::string& name) {
  return AddExtension(name, /*permissions=*/{}, /*host_permissions=*/{});
}

scoped_refptr<const extensions::Extension>
ExtensionsToolbarViewModelBrowserTest::AddExtension(
    const std::string& name,
    const std::vector<std::string>& permissions,
    const std::vector<std::string>& host_permissions,
    bool withhold_permissions) {
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder(name)
          .SetManifestVersion(3)
          .AddAPIPermissions(permissions)
          .AddHostPermissions(host_permissions)
          .SetAction(extensions::ActionInfo::Type::kBrowser)
          .SetID(crx_file::id_util::GenerateId(name))
          .Build();
  if (withhold_permissions) {
    extensions::ExtensionPrefs::Get(profile())->SetWithholdingPermissions(
        extension->id(), true);
  }
  extension_registrar()->AddExtension(extension.get());
  return extension;
}

void ExtensionsToolbarViewModelBrowserTest::NavigateTo(
    std::string_view host_name) {
  const GURL url = embedded_test_server()->GetURL(host_name, "/simple.html");
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), url));
}

void ExtensionsToolbarViewModelBrowserTest::SetUpOnMainThread() {
  ExtensionBrowserTest::SetUpOnMainThread();
  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(embedded_test_server()->Start());

  toolbar_delegate_ = std::make_unique<TestExtensionsToolbarDelegate>(
      browser_window_interface());
  toolbar_model_ = std::make_unique<ExtensionsToolbarViewModel>(
      toolbar_delegate_.get(), browser_window_interface(),
      ToolbarActionsModel::Get(profile()));

  toolbar_model_->AddObserver(&mock_observer());
}

void ExtensionsToolbarViewModelBrowserTest::TearDownOnMainThread() {
  if (toolbar_model_) {
    toolbar_model_->RemoveObserver(&mock_observer());
  }

  toolbar_model_.reset();
  toolbar_delegate_.reset();
  ExtensionBrowserTest::TearDownOnMainThread();
}

// Tests that the view model is correctly populated when initialized.
IN_PROC_BROWSER_TEST_F(ExtensionsToolbarViewModelBrowserTest,
                       PopulateActionModels) {
  // Add extensions before creating the model we want to test.
  auto extension1 = AddExtension("Alpha");
  auto extension2 = AddExtension("Beta");
  auto extension3 = AddExtension("Gamma");

  // Create a new model. The one in SetUpOnMainThread was created when no
  // extensions existed. We want to test the constructor's population logic.
  auto delegate = std::make_unique<TestExtensionsToolbarDelegate>(
      browser_window_interface());
  auto model = std::make_unique<ExtensionsToolbarViewModel>(
      delegate.get(), browser_window_interface(),
      ToolbarActionsModel::Get(profile()));

  // Verify that action models were added and that sorted in alphabetical order.
  std::vector<std::string> expected = {extension1->id(), extension2->id(),
                                       extension3->id()};
  std::sort(expected.begin(), expected.end());
  EXPECT_THAT(model->GetAllActionIds(), ElementsAreArray(expected));
}

// Tests that an action model is created in the view model when its
// corresponding extension is installed.
IN_PROC_BROWSER_TEST_F(ExtensionsToolbarViewModelBrowserTest,
                       OnToolbarActionAdded) {
  EXPECT_CALL(mock_observer(), OnActionAdded(_));

  // Add an extension.
  auto extension = AddExtension("Alpha");

  // Verify the View Model automatically updated its state to include it.
  EXPECT_TRUE(toolbar_model()->GetActionForId(extension->id()));
}

// Tests that the action model is removed from the view model when the
// corresponding extension is uninstalled.
IN_PROC_BROWSER_TEST_F(ExtensionsToolbarViewModelBrowserTest,
                       OnToolbarActionRemoved) {
  // Add an extension.
  auto extension = AddExtension("Alpha");
  ASSERT_TRUE(toolbar_model()->GetActionForId(extension->id()));

  // Uninstall the extension.
  EXPECT_CALL(mock_observer(), OnActionRemoved(extension->id()));
  extension_registrar()->RemoveExtension(
      extension->id(), extensions::UnloadedExtensionReason::UNINSTALL);

  // Verify the View Model updated its state to remove it.
  EXPECT_FALSE(toolbar_model()->GetActionForId(extension->id()));
  EXPECT_FALSE(toolbar_model()->HasAnyExtensions());
}

IN_PROC_BROWSER_TEST_F(ExtensionsToolbarViewModelBrowserTest,
                       OnToolbarPinnedActionsChanged) {
  auto extension1 = AddExtension("Alpha");
  auto extension2 = AddExtension("Beta");
  auto extension3 = AddExtension("Gamma");

  // No pinned action yet.
  EXPECT_THAT(toolbar_model()->GetPinnedActionIds(), ElementsAre());

  // This updates the ToolbarActionsModel, which in turn notifies our ViewModel.
  EXPECT_CALL(mock_observer(), OnPinnedActionsChanged());
  ToolbarActionsModel::Get(profile())->SetActionVisibility(extension1->id(),
                                                           true);
  EXPECT_THAT(toolbar_model()->GetPinnedActionIds(),
              ElementsAre(extension1->id()));

  // Pin another action and confirm the order.
  EXPECT_CALL(mock_observer(), OnPinnedActionsChanged());
  ToolbarActionsModel::Get(profile())->SetActionVisibility(extension2->id(),
                                                           true);
  EXPECT_THAT(toolbar_model()->GetPinnedActionIds(),
              ElementsAre(extension1->id(), extension2->id()));

  // Pin another action and confirm the order.
  EXPECT_CALL(mock_observer(), OnPinnedActionsChanged());
  ToolbarActionsModel::Get(profile())->SetActionVisibility(extension3->id(),
                                                           true);
  EXPECT_THAT(
      toolbar_model()->GetPinnedActionIds(),
      ElementsAre(extension1->id(), extension2->id(), extension3->id()));
}

IN_PROC_BROWSER_TEST_F(ExtensionsToolbarViewModelBrowserTest,
                       OnToolbarPinnedActionMoved) {
  // Set up 3 pinned extensions.
  auto extension1 = AddExtension("Alpha");
  auto extension2 = AddExtension("Beta");
  auto extension3 = AddExtension("Gamma");
  ToolbarActionsModel::Get(profile())->SetActionVisibility(extension1->id(),
                                                           true);
  ToolbarActionsModel::Get(profile())->SetActionVisibility(extension2->id(),
                                                           true);
  ToolbarActionsModel::Get(profile())->SetActionVisibility(extension3->id(),
                                                           true);
  EXPECT_THAT(
      toolbar_model()->GetPinnedActionIds(),
      ElementsAre(extension1->id(), extension2->id(), extension3->id()));

  // Move extension 1 to the second position.
  EXPECT_CALL(mock_observer(), OnPinnedActionsChanged());
  toolbar_model()->MovePinnedAction(extension1->id(), 1);

  // Confirm that the action was moved.
  EXPECT_THAT(
      toolbar_model()->GetPinnedActionIds(),
      ElementsAre(extension2->id(), extension1->id(), extension3->id()));
}

// Tests that the observer is notified when navigation happens.
IN_PROC_BROWSER_TEST_F(ExtensionsToolbarViewModelBrowserTest,
                       ObserverCalledOnNavigation) {
  EXPECT_CALL(mock_observer(), OnActiveWebContentsChanged(_))
      .Times(testing::AtLeast(1));

  NavigateTo("example.com");
}

// Tests that the observer is notified when the active tab changes.
IN_PROC_BROWSER_TEST_F(ExtensionsToolbarViewModelBrowserTest,
                       ObserverCalledOnActiveTabChanged) {
  EXPECT_CALL(mock_observer(), OnActiveWebContentsChanged(_))
      .Times(testing::AtLeast(1));

  TabListInterface* tab_list =
      TabListInterface::From(browser_window_interface());
  toolbar_model()->OnActiveTabChanged(*tab_list, tab_list->GetActiveTab());
}

// Tests that GetRequestAccessButtonParams returns empty when there are no
// extensions requesting access.
IN_PROC_BROWSER_TEST_F(ExtensionsToolbarViewModelBrowserTest,
                       GetRequestAccessButtonParams_NoRequests) {
  NavigateTo("example.com");
  auto params =
      toolbar_model()->GetRequestAccessButtonParams(GetActiveWebContents());
  EXPECT_TRUE(params.extension_ids.empty());
  EXPECT_TRUE(params.tooltip_text.empty());
}

IN_PROC_BROWSER_TEST_F(ExtensionsToolbarViewModelBrowserTest,
                       GetRequestAccessButtonParams_AddAndRemoveRequests) {
  NavigateTo("example.com");
  auto* web_contents = GetActiveWebContents();
  int tab_id = extensions::ExtensionTabUtil::GetTabId(web_contents);
  extensions::PermissionsManager* permissions_manager =
      extensions::PermissionsManager::Get(profile());

  // Add one request
  const std::string extension_a_id =
      crx_file::id_util::GenerateId("Extension A");
  extensions::ExtensionPrefs::Get(profile())->SetWithholdingPermissions(
      extension_a_id, true);
  auto extension_a = AddExtension("Extension A", {}, {"*://example.com/*"});
  permissions_manager->AddHostAccessRequest(web_contents, tab_id, *extension_a);
  auto params = toolbar_model()->GetRequestAccessButtonParams(web_contents);
  EXPECT_THAT(params.extension_ids, testing::ElementsAre(extension_a->id()));
  EXPECT_NE(params.tooltip_text.find(u"Extension A"), std::u16string::npos);
  EXPECT_NE(params.tooltip_text.find(u"example.com"), std::u16string::npos);

  // Add a second request
  const std::string extension_b_id =
      crx_file::id_util::GenerateId("Extension B");
  extensions::ExtensionPrefs::Get(profile())->SetWithholdingPermissions(
      extension_b_id, true);
  auto extension_b = AddExtension("Extension B", {}, {"*://example.com/*"});
  permissions_manager->AddHostAccessRequest(web_contents, tab_id, *extension_b);
  params = toolbar_model()->GetRequestAccessButtonParams(web_contents);
  EXPECT_THAT(params.extension_ids, testing::UnorderedElementsAre(
                                        extension_a->id(), extension_b->id()));
  EXPECT_NE(params.tooltip_text.find(u"Extension A"), std::u16string::npos);
  EXPECT_NE(params.tooltip_text.find(u"Extension B"), std::u16string::npos);

  // Remove the request for extension A
  permissions_manager->RemoveHostAccessRequest(tab_id, extension_a->id());
  params = toolbar_model()->GetRequestAccessButtonParams(web_contents);
  EXPECT_THAT(params.extension_ids, testing::ElementsAre(extension_b->id()));
  EXPECT_EQ(params.tooltip_text.find(u"Extension A"), std::u16string::npos);
  EXPECT_NE(params.tooltip_text.find(u"Extension B"), std::u16string::npos);

  // Remove the second request to return to the initial state
  permissions_manager->RemoveHostAccessRequest(tab_id, extension_b->id());
  params = toolbar_model()->GetRequestAccessButtonParams(web_contents);
  EXPECT_TRUE(params.extension_ids.empty());
  EXPECT_TRUE(params.tooltip_text.empty());
}

// Tests that GetRequestAccessButtonParams returns empty when an extension has
// host permissions but is not actively requesting access.
IN_PROC_BROWSER_TEST_F(
    ExtensionsToolbarViewModelBrowserTest,
    GetRequestAccessButtonParams_ExtensionNotActivelyRequestingAccess) {
  AddExtension("Test Extension", {}, {"*://example.com/*"});

  NavigateTo("example.com");
  auto params =
      toolbar_model()->GetRequestAccessButtonParams(GetActiveWebContents());
  EXPECT_TRUE(params.extension_ids.empty());
  EXPECT_TRUE(params.tooltip_text.empty());
}
