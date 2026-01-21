// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extensions_toolbar_view_model.h"

#include "base/memory/raw_ptr.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_view_host.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/extensions/extension_action_delegate.h"
#include "chrome/browser/ui/extensions/extension_action_view_model.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "components/crx_file/id_util.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/common/extension_builder.h"
#include "testing/gmock/include/gmock/gmock.h"

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
      const std::vector<std::string>& host_permissions);

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
    const std::vector<std::string>& host_permissions) {
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder(name)
          .AddAPIPermissions(permissions)
          .AddHostPermissions(host_permissions)
          .SetID(crx_file::id_util::GenerateId(name))
          .Build();
  extension_registrar()->AddExtension(extension.get());
  return extension;
}

void ExtensionsToolbarViewModelBrowserTest::SetUpOnMainThread() {
  ExtensionBrowserTest::SetUpOnMainThread();

  toolbar_delegate_ = std::make_unique<TestExtensionsToolbarDelegate>(
      browser_window_interface());
  toolbar_model_ = std::make_unique<ExtensionsToolbarViewModel>(
      toolbar_delegate_.get(), ToolbarActionsModel::Get(profile()));

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
      delegate.get(), ToolbarActionsModel::Get(profile()));

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
