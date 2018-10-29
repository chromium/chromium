// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"

#include <stddef.h>

#include <memory>

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_action_test_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/toolbar/component_toolbar_actions_factory.h"
#include "chrome/browser/ui/toolbar/mock_component_toolbar_actions_factory.h"
#include "chrome/browser/ui/toolbar/test_toolbar_action_view_controller.h"
#include "chrome/common/extensions/api/extension_action/action_info.h"
#include "chrome/common/pref_names.h"
#include "components/crx_file/id_util.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/pref_names.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest.h"
#include "extensions/common/value_builder.h"
#include "extensions/test/test_extension_dir.h"

namespace {

using ActionType = extensions::ExtensionBuilder::ActionType;

// A simple observer that tracks the number of times certain events occur.
class ToolbarActionsModelTestObserver : public ToolbarActionsModel::Observer {
 public:
  explicit ToolbarActionsModelTestObserver(ToolbarActionsModel* model);
  ~ToolbarActionsModelTestObserver() override;

  size_t inserted_count() const { return inserted_count_; }
  size_t removed_count() const { return removed_count_; }
  size_t moved_count() const { return moved_count_; }
  int highlight_mode_count() const { return highlight_mode_count_; }
  size_t initialized_count() const { return initialized_count_; }

 private:
  // ToolbarActionsModel::Observer:
  void OnToolbarActionAdded(const ToolbarActionsModel::ToolbarItem& item,
                            int index) override {
    ++inserted_count_;
  }

  void OnToolbarActionRemoved(const std::string& id) override {
    ++removed_count_;
  }

  void OnToolbarActionLoadFailed() override {}

  void OnToolbarActionMoved(const std::string& id, int index) override {
    ++moved_count_;
  }

  void OnToolbarActionUpdated(const std::string& id) override {}

  void OnToolbarVisibleCountChanged() override {}

  void OnToolbarHighlightModeChanged(bool is_highlighting) override {
    // Add one if highlighting, subtract one if not.
    highlight_mode_count_ += is_highlighting ? 1 : -1;
  }

  void OnToolbarModelInitialized() override { ++initialized_count_; }

  ToolbarActionsModel* model_;

  size_t inserted_count_;
  size_t removed_count_;
  size_t moved_count_;
  // Int because it could become negative (if something goes wrong).
  int highlight_mode_count_;
  size_t initialized_count_;

  DISALLOW_COPY_AND_ASSIGN(ToolbarActionsModelTestObserver);
};

ToolbarActionsModelTestObserver::ToolbarActionsModelTestObserver(
    ToolbarActionsModel* model)
    : model_(model),
      inserted_count_(0),
      removed_count_(0),
      moved_count_(0),
      highlight_mode_count_(0),
      initialized_count_(0) {
  model_->AddObserver(this);
}

ToolbarActionsModelTestObserver::~ToolbarActionsModelTestObserver() {
  model_->RemoveObserver(this);
}

}  // namespace

class ToolbarActionsModelUnitTest
    : public extensions::ExtensionServiceTestBase {
 public:
  ToolbarActionsModelUnitTest() {}
  ~ToolbarActionsModelUnitTest() override {}

 protected:
  // Initialize the ExtensionService, ToolbarActionsModel, and ExtensionSystem.
  void Init();

  // Initializes the ExtensionService, ToolbarActionsModel, and ExtensionSystem,
  // making ToolbarActionsModel use a MockComponentToolbarActionsFactory.
  void InitWithMockActionsFactory();

  void TearDown() override;

  // Adds or removes the given |extension| and verify success.
  testing::AssertionResult AddExtension(
      const scoped_refptr<const extensions::Extension>& extension)
      WARN_UNUSED_RESULT;
  testing::AssertionResult RemoveExtension(
      const scoped_refptr<const extensions::Extension>& extension)
      WARN_UNUSED_RESULT;

  // Adds three extensions, all with browser actions.
  testing::AssertionResult AddBrowserActionExtensions() WARN_UNUSED_RESULT;

  // Adds three extensions, one each for browser action, page action, and no
  // action, and are added in that order.
  testing::AssertionResult AddActionExtensions() WARN_UNUSED_RESULT;

  // Returns the action's id at the given index in the toolbar model, or empty
  // if one does not exist.
  // If |model| is specified, it is used. Otherwise, this defaults to
  // |toolbar_model_|.
  const std::string GetActionIdAtIndex(size_t index,
                                       const ToolbarActionsModel* model) const;
  const std::string GetActionIdAtIndex(size_t index) const;

  // Returns true if the |toobar_model_| has an action with the given |id|.
  bool ModelHasActionForId(const std::string& id) const;

  ToolbarActionsModel* toolbar_model() { return toolbar_model_; }

  const ToolbarActionsModelTestObserver* observer() const {
    return model_observer_.get();
  }
  size_t num_toolbar_items() const {
    return toolbar_model_->toolbar_items().size();
  }
  const extensions::Extension* browser_action_a() const {
    return browser_action_a_.get();
  }
  const extensions::Extension* browser_action_b() const {
    return browser_action_b_.get();
  }
  const extensions::Extension* browser_action_c() const {
    return browser_action_c_.get();
  }
  const extensions::Extension* browser_action() const {
    return browser_action_extension_.get();
  }
  const extensions::Extension* page_action() const {
    return page_action_extension_.get();
  }
  const extensions::Extension* no_action() const {
    return no_action_extension_.get();
  }

  // The mock component action will be referred to as "MCA" below.
  const char* component_action_id() {
    return MockComponentToolbarActionsFactory::kActionIdForTesting;
  }

 private:
  // Verifies that all extensions in |extensions| are added successfully.
  testing::AssertionResult AddAndVerifyExtensions(
      const extensions::ExtensionList& extensions);

  // The toolbar model associated with the testing profile.
  ToolbarActionsModel* toolbar_model_;

  // The test observer to track events. Must come after toolbar_model_ so that
  // it is destroyed and removes itself as an observer first.
  std::unique_ptr<ToolbarActionsModelTestObserver> model_observer_;

  // Sample extensions with only browser actions.
  scoped_refptr<const extensions::Extension> browser_action_a_;
  scoped_refptr<const extensions::Extension> browser_action_b_;
  scoped_refptr<const extensions::Extension> browser_action_c_;

  // Sample extensions with different kinds of actions.
  scoped_refptr<const extensions::Extension> browser_action_extension_;
  scoped_refptr<const extensions::Extension> page_action_extension_;
  scoped_refptr<const extensions::Extension> no_action_extension_;

  DISALLOW_COPY_AND_ASSIGN(ToolbarActionsModelUnitTest);
};

void ToolbarActionsModelUnitTest::Init() {
  InitializeEmptyExtensionService();
  toolbar_model_ =
      extensions::extension_action_test_util::CreateToolbarModelForProfile(
          profile());
  model_observer_ =
      std::make_unique<ToolbarActionsModelTestObserver>(toolbar_model_);
}

void ToolbarActionsModelUnitTest::InitWithMockActionsFactory() {
  InitializeEmptyExtensionService();
  toolbar_model_ = extensions::extension_action_test_util::
      CreateToolbarModelForProfileWithoutWaitingForReady(profile());
  toolbar_model_->SetMockActionsFactoryForTest(
      std::make_unique<MockComponentToolbarActionsFactory>(profile()));

  // Trigger ToolbarActionsModel::OnReady() after the actions factory has been
  // swapped out for a mock one.
  static_cast<extensions::TestExtensionSystem*>(
      extensions::ExtensionSystem::Get(profile()))
      ->SetReady();
  base::RunLoop().RunUntilIdle();

  model_observer_ =
      std::make_unique<ToolbarActionsModelTestObserver>(toolbar_model_);
}

void ToolbarActionsModelUnitTest::TearDown() {
  model_observer_.reset();
  extensions::ExtensionServiceTestBase::TearDown();
}

testing::AssertionResult ToolbarActionsModelUnitTest::AddExtension(
    const scoped_refptr<const extensions::Extension>& extension) {
  if (registry()->enabled_extensions().GetByID(extension->id())) {
    return testing::AssertionFailure() << "Extension " << extension->name()
                                       << " already installed!";
  }
  service()->AddExtension(extension.get());
  if (!registry()->enabled_extensions().GetByID(extension->id())) {
    return testing::AssertionFailure() << "Failed to install extension: "
                                       << extension->name();
  }
  return testing::AssertionSuccess();
}

testing::AssertionResult ToolbarActionsModelUnitTest::RemoveExtension(
    const scoped_refptr<const extensions::Extension>& extension) {
  if (!registry()->enabled_extensions().GetByID(extension->id())) {
    return testing::AssertionFailure() << "Extension " << extension->name()
                                       << " not installed!";
  }
  service()->UnloadExtension(extension->id(),
                             extensions::UnloadedExtensionReason::DISABLE);
  if (registry()->enabled_extensions().GetByID(extension->id())) {
    return testing::AssertionFailure() << "Failed to unload extension: "
                                       << extension->name();
  }
  return testing::AssertionSuccess();
}

testing::AssertionResult ToolbarActionsModelUnitTest::AddActionExtensions() {
  browser_action_extension_ = extensions::ExtensionBuilder("browser_action")
                                  .SetAction(ActionType::BROWSER_ACTION)
                                  .SetLocation(extensions::Manifest::INTERNAL)
                                  .Build();
  page_action_extension_ = extensions::ExtensionBuilder("page_action")
                               .SetAction(ActionType::PAGE_ACTION)
                               .SetLocation(extensions::Manifest::INTERNAL)
                               .Build();
  no_action_extension_ = extensions::ExtensionBuilder("no_action")
                             .SetLocation(extensions::Manifest::INTERNAL)
                             .Build();

  extensions::ExtensionList extensions;
  extensions.push_back(browser_action_extension_);
  extensions.push_back(page_action_extension_);
  extensions.push_back(no_action_extension_);

  return AddAndVerifyExtensions(extensions);
}

testing::AssertionResult
ToolbarActionsModelUnitTest::AddBrowserActionExtensions() {
  browser_action_a_ = extensions::ExtensionBuilder("browser_actionA")
                          .SetAction(ActionType::BROWSER_ACTION)
                          .SetLocation(extensions::Manifest::INTERNAL)
                          .Build();
  browser_action_b_ = extensions::ExtensionBuilder("browser_actionB")
                          .SetAction(ActionType::BROWSER_ACTION)
                          .SetLocation(extensions::Manifest::INTERNAL)
                          .Build();
  browser_action_c_ = extensions::ExtensionBuilder("browser_actionC")
                          .SetAction(ActionType::BROWSER_ACTION)
                          .SetLocation(extensions::Manifest::INTERNAL)
                          .Build();

  extensions::ExtensionList extensions;
  extensions.push_back(browser_action_a_);
  extensions.push_back(browser_action_b_);
  extensions.push_back(browser_action_c_);

  return AddAndVerifyExtensions(extensions);
}

const std::string ToolbarActionsModelUnitTest::GetActionIdAtIndex(
    size_t index,
    const ToolbarActionsModel* model) const {
  return index < model->toolbar_items().size()
             ? model->toolbar_items()[index].id
             : std::string();
}

const std::string ToolbarActionsModelUnitTest::GetActionIdAtIndex(
    size_t index) const {
  return GetActionIdAtIndex(index, toolbar_model_);
}

bool ToolbarActionsModelUnitTest::ModelHasActionForId(
    const std::string& id) const {
  for (const auto& item : toolbar_model_->toolbar_items()) {
    if (item.id == id)
      return true;
  }
  return false;
}

testing::AssertionResult ToolbarActionsModelUnitTest::AddAndVerifyExtensions(
    const extensions::ExtensionList& extensions) {
  for (auto iter = extensions.begin(); iter != extensions.end(); ++iter) {
    if (!AddExtension(*iter)) {
      return testing::AssertionFailure() << "Failed to install extension: "
                                         << (*iter)->name();
    }
  }
  return testing::AssertionSuccess();
}

// A basic test for component actions and extensions with browser actions
// showing up in the toolbar.
TEST_F(ToolbarActionsModelUnitTest, BasicToolbarActionsModelTest) {
  Init();

  // Load an extension with a browser action.
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("browser_action")
          .SetAction(ActionType::BROWSER_ACTION)
          .SetLocation(extensions::Manifest::INTERNAL)
          .Build();
  ASSERT_TRUE(AddExtension(extension));

  // We should now find our extension in the model.
  EXPECT_EQ(1u, observer()->inserted_count());
  EXPECT_EQ(1u, num_toolbar_items());
  EXPECT_EQ(extension->id(), GetActionIdAtIndex(0u));

  // Should be a no-op, but still fires the events.
  toolbar_model()->MoveActionIcon(extension->id(), 0);
  EXPECT_EQ(1u, observer()->moved_count());
  EXPECT_EQ(1u, num_toolbar_items());
  EXPECT_EQ(extension->id(), GetActionIdAtIndex(0u));

  // Remove the extension and verify.
  ASSERT_TRUE(RemoveExtension(extension));
  EXPECT_EQ(1u, observer()->removed_count());
  EXPECT_EQ(0u, num_toolbar_items());
  EXPECT_EQ(std::string(), GetActionIdAtIndex(0u));
}

// Test various different reorderings, removals, and reinsertions.
TEST_F(ToolbarActionsModelUnitTest, ActionsToolbarReorderAndReinsert) {
  Init();

  // Add the three browser action extensions.
  ASSERT_TRUE(AddBrowserActionExtensions());

  // Verify the three actions are in the model in the proper order.
  EXPECT_EQ(3u, num_toolbar_items());
  EXPECT_EQ(browser_action_a()->id(), GetActionIdAtIndex(0u));
  EXPECT_EQ(browser_action_b()->id(), GetActionIdAtIndex(1u));
  EXPECT_EQ(browser_action_c()->id(), GetActionIdAtIndex(2u));

  // Order is now A, B, C. Let's put C first.
  toolbar_model()->MoveActionIcon(browser_action_c()->id(), 0);
  EXPECT_EQ(1u, observer()->moved_count());
  EXPECT_EQ(3u, num_toolbar_items());
  EXPECT_EQ(browser_action_c()->id(), GetActionIdAtIndex(0u));
  EXPECT_EQ(browser_action_a()->id(), GetActionIdAtIndex(1u));
  EXPECT_EQ(browser_action_b()->id(), GetActionIdAtIndex(2u));

  // Order is now C, A, B. Let's put A last.
  toolbar_model()->MoveActionIcon(browser_action_a()->id(), 2);
  EXPECT_EQ(2u, observer()->moved_count());
  EXPECT_EQ(3u, num_toolbar_items());
  EXPECT_EQ(browser_action_c()->id(), GetActionIdAtIndex(0u));
  EXPECT_EQ(browser_action_b()->id(), GetActionIdAtIndex(1u));
  EXPECT_EQ(browser_action_a()->id(), GetActionIdAtIndex(2u));

  // Order is now C, B, A. Let's remove B.
  ASSERT_TRUE(RemoveExtension(browser_action_b()));
  EXPECT_EQ(1u, observer()->removed_count());
  EXPECT_EQ(2u, num_toolbar_items());
  EXPECT_EQ(browser_action_c()->id(), GetActionIdAtIndex(0u));
  EXPECT_EQ(browser_action_a()->id(), GetActionIdAtIndex(1u));

  // Load extension B again.
  ASSERT_TRUE(AddExtension(browser_action_b()));

  // Extension B loaded again.
  EXPECT_EQ(4u, observer()->inserted_count());
  EXPECT_EQ(3u, num_toolbar_items());
  // Make sure it gets its old spot in the list.
  EXPECT_EQ(browser_action_b()->id(), GetActionIdAtIndex(1u));

  // Unload B again.
  ASSERT_TRUE(RemoveExtension(browser_action_b()));
  EXPECT_EQ(2u, observer()->removed_count());
  EXPECT_EQ(2u, num_toolbar_items());

  // Order is now C, A. Flip it.
  toolbar_model()->MoveActionIcon(browser_action_a()->id(), 0);
  EXPECT_EQ(3u, observer()->moved_count());
  EXPECT_EQ(browser_action_a()->id(), GetActionIdAtIndex(0u));
  EXPECT_EQ(browser_action_c()->id(), GetActionIdAtIndex(1u));

  // Move A to the location it already occupies.
  toolbar_model()->MoveActionIcon(browser_action_a()->id(), 0);
  EXPECT_EQ(4u, observer()->moved_count());
  EXPECT_EQ(browser_action_a()->id(), GetActionIdAtIndex(0u));
  EXPECT_EQ(browser_action_c()->id(), GetActionIdAtIndex(1u));

  // Order is now A, C.
  ASSERT_TRUE(RemoveExtension(browser_action_c()));
  EXPECT_EQ(3u, observer()->removed_count());
  EXPECT_EQ(1u, num_toolbar_items());
  EXPECT_EQ(browser_action_a()->id(), GetActionIdAtIndex(0u));

  // Load extension C again.
  ASSERT_TRUE(AddExtension(browser_action_c()));

  // Extension C loaded again.
  EXPECT_EQ(5u, observer()->inserted_count());
  EXPECT_EQ(2u, num_toolbar_items());
  // Make sure it gets its old spot in the list (at the very end).
  EXPECT_EQ(browser_action_c()->id(), GetActionIdAtIndex(1u));
}

// Test that order persists after unloading and disabling, but not across
// uninstallation.
TEST_F(ToolbarActionsModelUnitTest, ActionsToolbarUnloadDisableAndUninstall) {
  Init();

  // Add the three browser action extensions.
  ASSERT_TRUE(AddBrowserActionExtensions());

  // Verify the three actions are in the model in the proper order: A, B, C.
  EXPECT_EQ(3u, num_toolbar_items());
  EXPECT_EQ(browser_action_a()->id(), GetActionIdAtIndex(0u));
  EXPECT_EQ(browser_action_b()->id(), GetActionIdAtIndex(1u));
  EXPECT_EQ(browser_action_c()->id(), GetActionIdAtIndex(2u));

  // Unload B, then C, then A, and then reload C, then A, then B.
  ASSERT_TRUE(RemoveExtension(browser_action_b()));
  ASSERT_TRUE(RemoveExtension(browser_action_c()));
  ASSERT_TRUE(RemoveExtension(browser_action_a()));
  EXPECT_EQ(0u, num_toolbar_items());  // Sanity check: all gone?
  ASSERT_TRUE(AddExtension(browser_action_c()));
  ASSERT_TRUE(AddExtension(browser_action_a()));
  ASSERT_TRUE(AddExtension(browser_action_b()));
  EXPECT_EQ(3u, num_toolbar_items());  // Sanity check: all back?
  EXPECT_EQ(0u, observer()->moved_count());

  // Even though we unloaded and reloaded in a different order, the original
  // order (A, B, C) should be preserved.
  EXPECT_EQ(browser_action_a()->id(), GetActionIdAtIndex(0u));
  EXPECT_EQ(browser_action_b()->id(), GetActionIdAtIndex(1u));
  EXPECT_EQ(browser_action_c()->id(), GetActionIdAtIndex(2u));

  // Disabling extensions should also preserve order.
  service()->DisableExtension(browser_action_b()->id(),
                              extensions::disable_reason::DISABLE_USER_ACTION);
  service()->DisableExtension(browser_action_c()->id(),
                              extensions::disable_reason::DISABLE_USER_ACTION);
  service()->DisableExtension(browser_action_a()->id(),
                              extensions::disable_reason::DISABLE_USER_ACTION);
  service()->EnableExtension(browser_action_c()->id());
  service()->EnableExtension(browser_action_a()->id());
  service()->EnableExtension(browser_action_b()->id());

  // Make sure we still get the original A, B, C order.
  EXPECT_EQ(browser_action_a()->id(), GetActionIdAtIndex(0u));
  EXPECT_EQ(browser_action_b()->id(), GetActionIdAtIndex(1u));
  EXPECT_EQ(browser_action_c()->id(), GetActionIdAtIndex(2u));

  // Move browser_action_b() to be first.
  toolbar_model()->MoveActionIcon(browser_action_b()->id(), 0);
  EXPECT_EQ(browser_action_b()->id(), GetActionIdAtIndex(0u));

  // Uninstall Extension B.
  service()->UninstallExtension(browser_action_b()->id(),
                                extensions::UNINSTALL_REASON_FOR_TESTING,
                                NULL);  // Ignore error.
  // List contains only A and C now. Validate that.
  EXPECT_EQ(2u, num_toolbar_items());
  EXPECT_EQ(browser_action_a()->id(), GetActionIdAtIndex(0u));
  EXPECT_EQ(browser_action_c()->id(), GetActionIdAtIndex(1u));

  ASSERT_TRUE(AddExtension(browser_action_b()));

  // Make sure Extension B is _not_ first (its old position should have been
  // forgotten at uninstall time). Order should be A, C, B.
  EXPECT_EQ(3u, num_toolbar_items());
  EXPECT_EQ(browser_action_a()->id(), GetActionIdAtIndex(0u));
  EXPECT_EQ(browser_action_c()->id(), GetActionIdAtIndex(1u));
  EXPECT_EQ(browser_action_b()->id(), GetActionIdAtIndex(2u));
}

TEST_F(ToolbarActionsModelUnitTest, ReorderOnPrefChange) {
  Init();

  // Add the three browser action extensions.
  ASSERT_TRUE(AddBrowserActionExtensions());
  EXPECT_EQ(3u, num_toolbar_items());

  // Change the value of the toolbar preference.
  std::vector<std::string> new_order;
  new_order.push_back(browser_action_c()->id());
  new_order.push_back(browser_action_b()->id());
  extensions::ExtensionPrefs::Get(profile())->SetToolbarOrder(new_order);

  // Verify order is changed.
  EXPECT_EQ(browser_action_c()->id(), GetActionIdAtIndex(0u));
  EXPECT_EQ(browser_action_b()->id(), GetActionIdAtIndex(1u));
  EXPECT_EQ(browser_action_a()->id(), GetActionIdAtIndex(2u));
}

// Test that new extension actions are always visible on installation and
// inserted at the "end" of the visible section.
TEST_F(ToolbarActionsModelUnitTest, NewToolbarExtensionsAreVisible) {
  Init();

  // Three extensions with actions.
  scoped_refptr<const extensions::Extension> extension_a =
      extensions::ExtensionBuilder("a")
          .SetAction(ActionType::BROWSER_ACTION)
          .SetLocation(extensions::Manifest::INTERNAL)
          .Build();
  scoped_refptr<const extensions::Extension> extension_b =
      extensions::ExtensionBuilder("b")
          .SetAction(ActionType::BROWSER_ACTION)
          .SetLocation(extensions::Manifest::INTERNAL)
          .Build();
  scoped_refptr<const extensions::Extension> extension_c =
      extensions::ExtensionBuilder("c")
          .SetAction(ActionType::BROWSER_ACTION)
          .SetLocation(extensions::Manifest::INTERNAL)
          .Build();
  scoped_refptr<const extensions::Extension> extension_d =
      extensions::ExtensionBuilder("d")
          .SetAction(ActionType::BROWSER_ACTION)
          .SetLocation(extensions::Manifest::INTERNAL)
          .Build();

  // We should start off without any actions.
  EXPECT_EQ(0u, num_toolbar_items());
  EXPECT_EQ(0u, toolbar_model()->visible_icon_count());

  // Add one action. It should be visible.
  service()->AddExtension(extension_a.get());
  EXPECT_EQ(1u, num_toolbar_items());
  EXPECT_EQ(1u, toolbar_model()->visible_icon_count());
  EXPECT_EQ(extension_a.get()->id(), GetActionIdAtIndex(0u));

  // Hide all actions.
  toolbar_model()->SetVisibleIconCount(0);
  EXPECT_EQ(0u, toolbar_model()->visible_icon_count());

  // Add a new action - it should be visible, so it should be in the first
  // index. The other action should remain hidden.
  service()->AddExtension(extension_b.get());
  EXPECT_EQ(2u, num_toolbar_items());
  EXPECT_EQ(1u, toolbar_model()->visible_icon_count());
  EXPECT_EQ(extension_b.get()->id(), GetActionIdAtIndex(0u));
  EXPECT_EQ(extension_a.get()->id(), GetActionIdAtIndex(1u));

  // Show all actions.
  toolbar_model()->SetVisibleIconCount(2);
  EXPECT_EQ(2u, toolbar_model()->visible_icon_count());
  EXPECT_TRUE(toolbar_model()->all_icons_visible());

  // Add the third action. Since all action are visible, it should go in the
  // last index.
  service()->AddExtension(extension_c.get());
  EXPECT_EQ(3u, num_toolbar_items());
  EXPECT_EQ(3u, toolbar_model()->visible_icon_count());
  EXPECT_TRUE(toolbar_model()->all_icons_visible());
  EXPECT_EQ(extension_b.get()->id(), GetActionIdAtIndex(0u));
  EXPECT_EQ(extension_a.get()->id(), GetActionIdAtIndex(1u));
  EXPECT_EQ(extension_c.get()->id(), GetActionIdAtIndex(2u));

  // Hide one action (two remaining visible).
  toolbar_model()->SetVisibleIconCount(2);
  EXPECT_EQ(2u, toolbar_model()->visible_icon_count());

  // Add a fourth action. It should go at the end of the visible section and
  // be visible, so it increases visible count by 1, and goes into the fourth
  // index. The hidden action should remain hidden.
  service()->AddExtension(extension_d.get());
  EXPECT_EQ(4u, num_toolbar_items());
  EXPECT_EQ(3u, toolbar_model()->visible_icon_count());
  EXPECT_EQ(extension_b.get()->id(), GetActionIdAtIndex(0u));
  EXPECT_EQ(extension_a.get()->id(), GetActionIdAtIndex(1u));
  EXPECT_EQ(extension_d.get()->id(), GetActionIdAtIndex(2u));
  EXPECT_EQ(extension_c.get()->id(), GetActionIdAtIndex(3u));
}

TEST_F(ToolbarActionsModelUnitTest, ActionsToolbarHighlightMode) {
  Init();

  EXPECT_FALSE(toolbar_model()->HighlightActions(
      std::vector<std::string>(), ToolbarActionsModel::HIGHLIGHT_WARNING));
  EXPECT_EQ(0, observer()->highlight_mode_count());

  // Add the three browser action extensions.
  ASSERT_TRUE(AddBrowserActionExtensions());
  EXPECT_EQ(3u, num_toolbar_items());

  // Start with a visible count of 2 (non-zero, and not all).
  toolbar_model()->SetVisibleIconCount(2u);

  // Highlight one extension.
  std::vector<std::string> action_ids;
  action_ids.push_back(browser_action_b()->id());
  toolbar_model()->HighlightActions(action_ids,
                                    ToolbarActionsModel::HIGHLIGHT_WARNING);
  EXPECT_EQ(1, observer()->highlight_mode_count());
  EXPECT_TRUE(toolbar_model()->is_highlighting());
  EXPECT_EQ(1u, num_toolbar_items());
  EXPECT_EQ(browser_action_b()->id(), GetActionIdAtIndex(0u));
  EXPECT_EQ(1u, toolbar_model()->visible_icon_count());

  // Stop highlighting.
  toolbar_model()->StopHighlighting();
  EXPECT_EQ(0, observer()->highlight_mode_count());
  EXPECT_FALSE(toolbar_model()->is_highlighting());

  // Verify that the extensions are back to normal.
  EXPECT_EQ(3u, num_toolbar_items());
  EXPECT_EQ(browser_action_a()->id(), GetActionIdAtIndex(0u));
  EXPECT_EQ(browser_action_b()->id(), GetActionIdAtIndex(1u));
  EXPECT_EQ(browser_action_c()->id(), GetActionIdAtIndex(2u));
  EXPECT_EQ(2u, toolbar_model()->visible_icon_count());

  // Call stop highlighting a second time (shouldn't be notified).
  toolbar_model()->StopHighlighting();
  EXPECT_EQ(0, observer()->highlight_mode_count());
  EXPECT_FALSE(toolbar_model()->is_highlighting());

  // Highlight all extensions.
  action_ids.clear();
  action_ids.push_back(browser_action_a()->id());
  action_ids.push_back(browser_action_b()->id());
  action_ids.push_back(browser_action_c()->id());
  toolbar_model()->HighlightActions(action_ids,
                                    ToolbarActionsModel::HIGHLIGHT_WARNING);
  EXPECT_EQ(1, observer()->highlight_mode_count());
  EXPECT_EQ(3u, num_toolbar_items());
  EXPECT_EQ(browser_action_a()->id(), GetActionIdAtIndex(0u));
  EXPECT_EQ(browser_action_b()->id(), GetActionIdAtIndex(1u));
  EXPECT_EQ(browser_action_c()->id(), GetActionIdAtIndex(2u));
  EXPECT_EQ(3u, toolbar_model()->visible_icon_count());
  // Even though the visible count is 3, we shouldn't adjust the stored
  // preference.
  EXPECT_EQ(2, profile()->GetPrefs()->GetInteger(
                   extensions::pref_names::kToolbarSize));

  // Highlight only extension B (shrink the highlight list).
  action_ids.clear();
  action_ids.push_back(browser_action_b()->id());
  toolbar_model()->HighlightActions(action_ids,
                                    ToolbarActionsModel::HIGHLIGHT_WARNING);
  EXPECT_EQ(2, observer()->highlight_mode_count());
  EXPECT_EQ(1u, num_toolbar_items());
  EXPECT_EQ(browser_action_b()->id(), GetActionIdAtIndex(0u));

  // Highlight extensions A and B (grow the highlight list).
  action_ids.clear();
  action_ids.push_back(browser_action_a()->id());
  action_ids.push_back(browser_action_b()->id());
  toolbar_model()->HighlightActions(action_ids,
                                    ToolbarActionsModel::HIGHLIGHT_WARNING);
  EXPECT_EQ(3, observer()->highlight_mode_count());
  EXPECT_EQ(2u, num_toolbar_items());
  EXPECT_EQ(browser_action_a()->id(), GetActionIdAtIndex(0u));
  EXPECT_EQ(browser_action_b()->id(), GetActionIdAtIndex(1u));

  // Highlight no extensions (empty the highlight list).
  action_ids.clear();
  toolbar_model()->HighlightActions(action_ids,
                                    ToolbarActionsModel::HIGHLIGHT_WARNING);
  EXPECT_EQ(2, observer()->highlight_mode_count());
  EXPECT_FALSE(toolbar_model()->is_highlighting());
  EXPECT_EQ(browser_action_a()->id(), GetActionIdAtIndex(0u));
  EXPECT_EQ(browser_action_b()->id(), GetActionIdAtIndex(1u));
  EXPECT_EQ(browser_action_c()->id(), GetActionIdAtIndex(2u));
  // Our toolbar size should be back to normal.
  EXPECT_EQ(2u, toolbar_model()->visible_icon_count());
  EXPECT_EQ(2, profile()->GetPrefs()->GetInteger(
                   extensions::pref_names::kToolbarSize));
}

TEST_F(ToolbarActionsModelUnitTest, ActionsToolbarHighlightModeRemove) {
  Init();

  // Add the three browser action extensions.
  ASSERT_TRUE(AddBrowserActionExtensions());
  EXPECT_EQ(3u, num_toolbar_items());

  // Highlight two of the extensions.
  std::vector<std::string> action_ids;
  action_ids.push_back(browser_action_a()->id());
  action_ids.push_back(browser_action_b()->id());
  toolbar_model()->HighlightActions(action_ids,
                                    ToolbarActionsModel::HIGHLIGHT_WARNING);
  EXPECT_TRUE(toolbar_model()->is_highlighting());
  EXPECT_EQ(1, observer()->highlight_mode_count());
  EXPECT_EQ(2u, num_toolbar_items());

  // Disable one of them - only one should remain highlighted.
  service()->DisableExtension(browser_action_a()->id(),
                              extensions::disable_reason::DISABLE_USER_ACTION);
  EXPECT_TRUE(toolbar_model()->is_highlighting());
  EXPECT_EQ(1u, num_toolbar_items());
  EXPECT_EQ(browser_action_b()->id(), GetActionIdAtIndex(0u));

  // Uninstall the remaining highlighted extension. This should result in
  // highlight mode exiting.
  service()->UninstallExtension(browser_action_b()->id(),
                                extensions::UNINSTALL_REASON_FOR_TESTING,
                                NULL);  // Ignore error.
  EXPECT_FALSE(toolbar_model()->is_highlighting());
  EXPECT_EQ(0, observer()->highlight_mode_count());
  EXPECT_EQ(1u, num_toolbar_items());
  EXPECT_EQ(browser_action_c()->id(), GetActionIdAtIndex(0u));

  // Test that removing an unhighlighted extension still works.
  // Reinstall extension B, and then highlight extension C.
  ASSERT_TRUE(AddExtension(browser_action_b()));
  EXPECT_EQ(browser_action_b()->id(), GetActionIdAtIndex(1u));
  action_ids.clear();
  action_ids.push_back(browser_action_c()->id());
  toolbar_model()->HighlightActions(action_ids,
                                    ToolbarActionsModel::HIGHLIGHT_WARNING);
  EXPECT_EQ(1, observer()->highlight_mode_count());
  EXPECT_TRUE(toolbar_model()->is_highlighting());
  EXPECT_EQ(1u, num_toolbar_items());
  EXPECT_EQ(browser_action_c()->id(), GetActionIdAtIndex(0u));

  // Uninstalling B should not have visible impact.
  service()->UninstallExtension(browser_action_b()->id(),
                                extensions::UNINSTALL_REASON_FOR_TESTING,
                                NULL);  // Ignore error.
  EXPECT_TRUE(toolbar_model()->is_highlighting());
  EXPECT_EQ(1, observer()->highlight_mode_count());
  EXPECT_EQ(1u, num_toolbar_items());
  EXPECT_EQ(browser_action_c()->id(), GetActionIdAtIndex(0u));

  // When we stop, only action C should remain.
  toolbar_model()->StopHighlighting();
  EXPECT_FALSE(toolbar_model()->is_highlighting());
  EXPECT_EQ(0, observer()->highlight_mode_count());
  EXPECT_EQ(1u, num_toolbar_items());
  EXPECT_EQ(browser_action_c()->id(), GetActionIdAtIndex(0u));
}

TEST_F(ToolbarActionsModelUnitTest, ActionsToolbarHighlightModeAdd) {
  Init();

  // Add the three browser action extensions.
  ASSERT_TRUE(AddBrowserActionExtensions());
  EXPECT_EQ(3u, num_toolbar_items());

  // Remove one (down to two).
  ASSERT_TRUE(RemoveExtension(browser_action_c()));

  // Highlight one of the two actions.
  std::vector<std::string> action_ids;
  action_ids.push_back(browser_action_a()->id());
  toolbar_model()->HighlightActions(action_ids,
                                    ToolbarActionsModel::HIGHLIGHT_WARNING);
  EXPECT_TRUE(toolbar_model()->is_highlighting());
  EXPECT_EQ(1u, num_toolbar_items());
  EXPECT_EQ(browser_action_a()->id(), GetActionIdAtIndex(0u));

  // Adding a new extension should have no visible effect.
  ASSERT_TRUE(AddExtension(browser_action_c()));
  EXPECT_TRUE(toolbar_model()->is_highlighting());
  EXPECT_EQ(1u, num_toolbar_items());
  EXPECT_EQ(browser_action_a()->id(), GetActionIdAtIndex(0u));

  // When we stop highlighting, we should see the new extension show up.
  toolbar_model()->StopHighlighting();
  EXPECT_FALSE(toolbar_model()->is_highlighting());
  EXPECT_EQ(3u, num_toolbar_items());
  EXPECT_EQ(browser_action_a()->id(), GetActionIdAtIndex(0u));
  EXPECT_EQ(browser_action_b()->id(), GetActionIdAtIndex(1u));
  EXPECT_EQ(browser_action_c()->id(), GetActionIdAtIndex(2u));
}

// Test that the action toolbar maintains the proper size, even after a pref
// change.
TEST_F(ToolbarActionsModelUnitTest, ActionsToolbarSizeAfterPrefChange) {
  Init();

  // Add the three browser action extensions.
  ASSERT_TRUE(AddBrowserActionExtensions());
  EXPECT_EQ(3u, num_toolbar_items());

  // Should be at max size.
  EXPECT_TRUE(toolbar_model()->all_icons_visible());
  EXPECT_EQ(num_toolbar_items(), toolbar_model()->visible_icon_count());
  toolbar_model()->OnActionToolbarPrefChange();
  // Should still be at max size.
  EXPECT_TRUE(toolbar_model()->all_icons_visible());
  EXPECT_EQ(num_toolbar_items(), toolbar_model()->visible_icon_count());
}

// Test that, with the extension-action-redesign switch, the model contains
// all types of extensions, except those which should not be displayed on the
// toolbar (like component extensions).
TEST_F(ToolbarActionsModelUnitTest, TestToolbarExtensionTypesEnabledSwitch) {
  Init();

  ASSERT_TRUE(AddActionExtensions());

  // With the switch on, extensions with page actions and no action should also
  // be displayed in the toolbar.
  EXPECT_EQ(3u, num_toolbar_items());
  EXPECT_EQ(browser_action()->id(), GetActionIdAtIndex(0u));
  EXPECT_EQ(page_action()->id(), GetActionIdAtIndex(1u));
  EXPECT_EQ(no_action()->id(), GetActionIdAtIndex(2u));

  // Extensions that are installed by default shouldn't be given an icon.
  extensions::DictionaryBuilder default_installed_manifest;
  default_installed_manifest.Set("name", "default installed")
      .Set("description", "A default installed extension")
      .Set("manifest_version", 2)
      .Set("version", "1.0.0.0");
  scoped_refptr<const extensions::Extension> default_installed_extension =
      extensions::ExtensionBuilder()
          .SetManifest(default_installed_manifest.Build())
          .SetID(crx_file::id_util::GenerateId("default"))
          .SetLocation(extensions::Manifest::INTERNAL)
          .AddFlags(extensions::Extension::WAS_INSTALLED_BY_DEFAULT)
          .Build();
  EXPECT_TRUE(AddExtension(default_installed_extension.get()));
  EXPECT_EQ(3u, num_toolbar_items());
  EXPECT_FALSE(ModelHasActionForId(default_installed_extension->id()));

  // Component extensions shouldn't be given an icon.
  scoped_refptr<const extensions::Extension> component_extension_no_action =
      extensions::ExtensionBuilder("component ext no action")
          .SetLocation(extensions::Manifest::COMPONENT)
          .Build();
  EXPECT_TRUE(AddExtension(component_extension_no_action.get()));
  EXPECT_EQ(3u, num_toolbar_items());
  EXPECT_FALSE(ModelHasActionForId(component_extension_no_action->id()));

  // Sanity check: A new extension that's installed from the webstore should
  // have an icon.
  scoped_refptr<const extensions::Extension> internal_extension_no_action =
      extensions::ExtensionBuilder("internal ext no action")
          .SetLocation(extensions::Manifest::INTERNAL)
          .Build();
  EXPECT_TRUE(AddExtension(internal_extension_no_action.get()));
  EXPECT_EQ(4u, num_toolbar_items());
  EXPECT_TRUE(ModelHasActionForId(internal_extension_no_action->id()));
}

TEST_F(ToolbarActionsModelUnitTest, ActionsToolbarIncognitoModeTest) {
  Init();
  ASSERT_TRUE(AddBrowserActionExtensions());

  // Give two extensions incognito access.
  // Note: We use ExtensionPrefs::SetIsIncognitoEnabled instead of
  // util::SetIsIncognitoEnabled because the latter tries to reload the
  // extension, which requries a filepath associated with the extension (and,
  // for this test, reloading the extension is irrelevant to us).
  extensions::ExtensionPrefs* extension_prefs =
      extensions::ExtensionPrefs::Get(profile());
  extension_prefs->SetIsIncognitoEnabled(browser_action_b()->id(), true);
  extension_prefs->SetIsIncognitoEnabled(browser_action_c()->id(), true);

  extensions::util::SetIsIncognitoEnabled(browser_action_b()->id(), profile(),
                                          true);
  extensions::util::SetIsIncognitoEnabled(browser_action_c()->id(), profile(),
                                          true);

  // Move C to the second index.
  toolbar_model()->MoveActionIcon(browser_action_c()->id(), 1u);
  // Set visible count to 3 so that C is overflowed. State is A, C, [B].
  toolbar_model()->SetVisibleIconCount(2);
  EXPECT_EQ(1u, observer()->moved_count());

  // Get an incognito profile and toolbar.
  ToolbarActionsModel* incognito_model =
      extensions::extension_action_test_util::CreateToolbarModelForProfile(
          profile()->GetOffTheRecordProfile());

  ToolbarActionsModelTestObserver incognito_observer(incognito_model);
  EXPECT_EQ(0u, incognito_observer.moved_count());

  // We should have two items: C, B, and the order should be preserved from the
  // original model.
  EXPECT_EQ(2u, incognito_model->toolbar_items().size());
  EXPECT_EQ(browser_action_c()->id(), GetActionIdAtIndex(0u, incognito_model));
  EXPECT_EQ(browser_action_b()->id(), GetActionIdAtIndex(1u, incognito_model));

  // Actions in the overflow menu in the regular toolbar should remain in
  // overflow in the incognito toolbar. So, we should have C, [B].
  EXPECT_EQ(1u, incognito_model->visible_icon_count());
  // The regular model should still have two icons visible.
  EXPECT_EQ(2u, toolbar_model()->visible_icon_count());

  // Changing the incognito model size should not affect the regular model.
  incognito_model->SetVisibleIconCount(0);
  EXPECT_EQ(0u, incognito_model->visible_icon_count());
  EXPECT_EQ(2u, toolbar_model()->visible_icon_count());

  // Expanding the incognito model to 3 should register as "all icons"
  // since it is all of the incognito-enabled extensions.
  incognito_model->SetVisibleIconCount(2u);
  EXPECT_EQ(2u, incognito_model->visible_icon_count());
  EXPECT_TRUE(incognito_model->all_icons_visible());

  // Moving icons in the incognito toolbar should not affect the regular
  // toolbar. Incognito currently has C, B...
  incognito_model->MoveActionIcon(browser_action_b()->id(), 0u);
  // So now it should be B, C...
  EXPECT_EQ(1u, incognito_observer.moved_count());
  EXPECT_EQ(browser_action_b()->id(), GetActionIdAtIndex(0u, incognito_model));
  EXPECT_EQ(browser_action_c()->id(), GetActionIdAtIndex(1u, incognito_model));
  // ... and the regular toolbar should be unaffected.
  EXPECT_EQ(browser_action_a()->id(), GetActionIdAtIndex(0u));
  EXPECT_EQ(browser_action_c()->id(), GetActionIdAtIndex(1u));
  EXPECT_EQ(browser_action_b()->id(), GetActionIdAtIndex(2u));

  // Similarly, the observer for the regular model should not have received
  // any updates.
  EXPECT_EQ(1u, observer()->moved_count());

  // And performing moves on the regular model should have no effect on the
  // incognito model or its observers.
  toolbar_model()->MoveActionIcon(browser_action_c()->id(), 2u);
  EXPECT_EQ(2u, observer()->moved_count());
  EXPECT_EQ(1u, incognito_observer.moved_count());
}

// Test that enabling extensions incognito with an active incognito profile
// works.
TEST_F(ToolbarActionsModelUnitTest, ActionsToolbarIncognitoEnableExtension) {
  Init();

  const char* kManifest =
      "{"
      "  \"name\": \"%s\","
      "  \"version\": \"1.0\","
      "  \"manifest_version\": 2,"
      "  \"browser_action\": {}"
      "}";

  // For this test, we need to have "real" extension files, because we need to
  // be able to reload them during the incognito process. Since the toolbar
  // needs to be notified of the reload, we need it this time (as opposed to
  // above, where we simply set the prefs before the incognito bar was
  // created.
  extensions::TestExtensionDir dir1;
  dir1.WriteManifest(base::StringPrintf(kManifest, "incognito1"));
  extensions::TestExtensionDir dir2;
  dir2.WriteManifest(base::StringPrintf(kManifest, "incognito2"));

  extensions::TestExtensionDir* dirs[] = {&dir1, &dir2};
  const extensions::Extension* extensions[] = {nullptr, nullptr};
  for (size_t i = 0; i < arraysize(dirs); ++i) {
    // The extension id will be calculated from the file path; we need this to
    // wait for the extension to load.
    base::FilePath path_for_id =
        base::MakeAbsoluteFilePath(dirs[i]->UnpackedPath());
    std::string id = crx_file::id_util::GenerateIdForPath(path_for_id);
    extensions::TestExtensionRegistryObserver observer(registry(), id);
    extensions::UnpackedInstaller::Create(service())->Load(
        dirs[i]->UnpackedPath());
    observer.WaitForExtensionLoaded();
    extensions[i] = registry()->enabled_extensions().GetByID(id);
    ASSERT_TRUE(extensions[i]);
  }

  // For readability, alias to A and B. Since we'll be reloading these
  // extensions, we also can't rely on pointers.
  std::string extension_a = extensions[0]->id();
  std::string extension_b = extensions[1]->id();

  // The first model should have both extensions visible.
  EXPECT_EQ(2u, toolbar_model()->toolbar_items().size());
  EXPECT_EQ(extension_a, GetActionIdAtIndex(0u));
  EXPECT_EQ(extension_b, GetActionIdAtIndex(1u));

  // Set the model to only show one extension, so the order is A, [B].
  toolbar_model()->SetVisibleIconCount(1u);

  // Get an incognito profile and toolbar.
  ToolbarActionsModel* incognito_model =
      extensions::extension_action_test_util::CreateToolbarModelForProfile(
          profile()->GetOffTheRecordProfile());
  ToolbarActionsModelTestObserver incognito_observer(incognito_model);

  // Right now, no actions are enabled in incognito mode.
  EXPECT_EQ(0u, incognito_model->toolbar_items().size());

  // Set extension B (which is overflowed) to be enabled in incognito. This
  // results in b reloading, so wait for it.
  {
    extensions::TestExtensionRegistryObserver observer(registry(), extension_b);
    extensions::util::SetIsIncognitoEnabled(extension_b, profile(), true);
    observer.WaitForExtensionLoaded();
  }

  // Now, we should have one icon in the incognito bar. But, since B is
  // overflowed in the main bar, it shouldn't be visible.
  EXPECT_EQ(1u, incognito_model->toolbar_items().size());
  EXPECT_EQ(extension_b, GetActionIdAtIndex(0u, incognito_model));
  EXPECT_EQ(0u, incognito_model->visible_icon_count());

  // Also enable extension a for incognito (again, wait for the reload).
  {
    extensions::TestExtensionRegistryObserver observer(registry(), extension_a);
    extensions::util::SetIsIncognitoEnabled(extension_a, profile(), true);
    observer.WaitForExtensionLoaded();
  }

  // Now, both extensions should be enabled in incognito mode. In addition, the
  // incognito toolbar should have expanded to show extension A (since it isn't
  // overflowed in the main bar).
  EXPECT_EQ(2u, incognito_model->toolbar_items().size());
  EXPECT_EQ(extension_a, GetActionIdAtIndex(0u, incognito_model));
  EXPECT_EQ(extension_b, GetActionIdAtIndex(1u, incognito_model));
  EXPECT_EQ(1u, incognito_model->visible_icon_count());
}

// Test that hiding actions on the toolbar results in sending them to the
// overflow menu when the redesign switch is enabled.
TEST_F(ToolbarActionsModelUnitTest,
       ActionsToolbarActionsVisibilityWithSwitchAndComponentActions) {
  Init();

  // We choose to use all types of extensions here, since the misnamed
  // BrowserActionVisibility is now for toolbar visibility.
  ASSERT_TRUE(AddActionExtensions());

  // For readability, alias extensions A B C.
  const extensions::Extension* extension_a = browser_action();
  const extensions::Extension* extension_b = page_action();
  const extensions::Extension* extension_c = no_action();

  // Sanity check: Order should start as A, B, C, with all three visible.
  EXPECT_EQ(3u, num_toolbar_items());
  EXPECT_TRUE(toolbar_model()->all_icons_visible());
  EXPECT_EQ(extension_a->id(), GetActionIdAtIndex(0u));
  EXPECT_EQ(extension_b->id(), GetActionIdAtIndex(1u));
  EXPECT_EQ(extension_c->id(), GetActionIdAtIndex(2u));

  toolbar_model()->SetActionVisibility(extension_b->id(), false);

  // Thus, the order should be A, C, B, with B in the overflow.
  EXPECT_EQ(3u, num_toolbar_items());
  EXPECT_EQ(2u, toolbar_model()->visible_icon_count());
  EXPECT_EQ(extension_a->id(), GetActionIdAtIndex(0u));
  EXPECT_EQ(extension_c->id(), GetActionIdAtIndex(1u));
  EXPECT_EQ(extension_b->id(), GetActionIdAtIndex(2u));

  // Hiding an extension's action should result in it being sent to the overflow
  // as well, but as the _first_ extension in the overflow.
  toolbar_model()->SetActionVisibility(extension_a->id(), false);
  // Thus, the order should be C, A, B, with A and B in the overflow.
  EXPECT_EQ(3u, num_toolbar_items());
  EXPECT_EQ(1u, toolbar_model()->visible_icon_count());
  EXPECT_EQ(extension_c->id(), GetActionIdAtIndex(0u));
  EXPECT_EQ(extension_a->id(), GetActionIdAtIndex(1u));
  EXPECT_EQ(extension_b->id(), GetActionIdAtIndex(2u));

  // Resetting A's visibility to true should send it back to the visible icons
  // (and should grow visible icons by 1), but it should be added to the end of
  // the visible icon list (not to its original position).
  toolbar_model()->SetActionVisibility(extension_a->id(), true);
  // So order is C, A, B, with only B in the overflow.
  EXPECT_EQ(3u, num_toolbar_items());
  EXPECT_EQ(2u, toolbar_model()->visible_icon_count());
  EXPECT_EQ(extension_c->id(), GetActionIdAtIndex(0u));
  EXPECT_EQ(extension_a->id(), GetActionIdAtIndex(1u));
  EXPECT_EQ(extension_b->id(), GetActionIdAtIndex(2u));

  // Resetting B to be visible should make the order C, A, B, with no
  // overflow.
  toolbar_model()->SetActionVisibility(extension_b->id(), true);
  EXPECT_EQ(3u, num_toolbar_items());
  EXPECT_TRUE(toolbar_model()->all_icons_visible());
  EXPECT_EQ(extension_c->id(), GetActionIdAtIndex(0u));
  EXPECT_EQ(extension_a->id(), GetActionIdAtIndex(1u));
  EXPECT_EQ(extension_b->id(), GetActionIdAtIndex(2u));
}

// Test that observers receive no Added notifications until after the
// ExtensionSystem has initialized.
TEST_F(ToolbarActionsModelUnitTest, ModelWaitsForExtensionSystemReady) {
  InitializeEmptyExtensionService();
  ToolbarActionsModel* toolbar_model = extensions::extension_action_test_util::
      CreateToolbarModelForProfileWithoutWaitingForReady(profile());
  ToolbarActionsModelTestObserver model_observer(toolbar_model);

  EXPECT_TRUE(AddBrowserActionExtensions());

  // Since the model hasn't been initialized (the ExtensionSystem::ready task
  // hasn't been run), there should be no insertion notifications.
  EXPECT_EQ(0u, model_observer.inserted_count());
  EXPECT_EQ(0u, model_observer.initialized_count());
  EXPECT_FALSE(toolbar_model->actions_initialized());

  // Run the ready task.
  static_cast<extensions::TestExtensionSystem*>(
      extensions::ExtensionSystem::Get(profile()))
      ->SetReady();
  // Run tasks posted to TestExtensionSystem.
  base::RunLoop().RunUntilIdle();

  // We should still have no insertions, but should have an initialized count.
  EXPECT_TRUE(toolbar_model->actions_initialized());
  EXPECT_EQ(0u, model_observer.inserted_count());
  EXPECT_EQ(1u, model_observer.initialized_count());
}

// Check that the toolbar model correctly clears and reorders when it detects
// a preference change.
TEST_F(ToolbarActionsModelUnitTest, ToolbarModelPrefChange) {
  Init();

  ASSERT_TRUE(AddBrowserActionExtensions());

  // We should start in the basic A, B, C order.
  ASSERT_TRUE(browser_action_a());
  EXPECT_EQ(browser_action_a()->id(), GetActionIdAtIndex(0));
  EXPECT_EQ(browser_action_b()->id(), GetActionIdAtIndex(1));
  EXPECT_EQ(browser_action_c()->id(), GetActionIdAtIndex(2));
  // Record the difference between the inserted and removed counts. The actual
  // value of the counts is not important, but we need to be sure that if we
  // call to remove any, we also add them back.
  size_t inserted_and_removed_difference =
      observer()->inserted_count() - observer()->removed_count();

  // Assign a new order, B, C, A, and write it in the prefs.
  std::vector<std::string> new_order;
  new_order.push_back(browser_action_b()->id());
  new_order.push_back(browser_action_c()->id());
  new_order.push_back(browser_action_a()->id());
  extensions::ExtensionPrefs::Get(profile())->SetToolbarOrder(new_order);

  // Ensure everything has time to run.
  base::RunLoop().RunUntilIdle();

  // The new order should be reflected in the model.
  EXPECT_EQ(browser_action_b()->id(), GetActionIdAtIndex(0));
  EXPECT_EQ(browser_action_c()->id(), GetActionIdAtIndex(1));
  EXPECT_EQ(browser_action_a()->id(), GetActionIdAtIndex(2));
  EXPECT_EQ(inserted_and_removed_difference,
            observer()->inserted_count() - observer()->removed_count());
}

// Test various different reorderings, removals, and reinsertions of the
// toolbar with component actions.
TEST_F(ToolbarActionsModelUnitTest,
       ActionsToolbarReorderAndReinsertWithSwitchAndComponentActions) {
  InitWithMockActionsFactory();

  // One component action was added when the model was initialized.
  EXPECT_EQ(1u, num_toolbar_items());
  EXPECT_EQ(component_action_id(), GetActionIdAtIndex(0u));
  EXPECT_TRUE(toolbar_model()->HasComponentAction(component_action_id()));

  // Add the three browser action extensions.
  ASSERT_TRUE(AddBrowserActionExtensions());

  // Verify the four actions are in the model in the proper order.
  EXPECT_EQ(4u, num_toolbar_items());
  EXPECT_EQ(component_action_id(), GetActionIdAtIndex(0u));
  EXPECT_EQ(browser_action_a()->id(), GetActionIdAtIndex(1u));
  EXPECT_EQ(browser_action_b()->id(), GetActionIdAtIndex(2u));
  EXPECT_EQ(browser_action_c()->id(), GetActionIdAtIndex(3u));

  // Order is now MCA, A, B, C. Let's put C first.
  toolbar_model()->MoveActionIcon(browser_action_c()->id(), 0);
  EXPECT_EQ(1u, observer()->moved_count());
  EXPECT_EQ(4u, num_toolbar_items());
  EXPECT_EQ(browser_action_c()->id(), GetActionIdAtIndex(0u));
  EXPECT_EQ(component_action_id(), GetActionIdAtIndex(1u));
  EXPECT_EQ(browser_action_a()->id(), GetActionIdAtIndex(2u));
  EXPECT_EQ(browser_action_b()->id(), GetActionIdAtIndex(3u));

  // Order is now C, MCA, A, B. Let's put MCA last.
  toolbar_model()->MoveActionIcon(component_action_id(), 3);
  EXPECT_EQ(2u, observer()->moved_count());
  EXPECT_EQ(4u, num_toolbar_items());
  EXPECT_EQ(browser_action_c()->id(), GetActionIdAtIndex(0u));
  EXPECT_EQ(browser_action_a()->id(), GetActionIdAtIndex(1u));
  EXPECT_EQ(browser_action_b()->id(), GetActionIdAtIndex(2u));
  EXPECT_EQ(component_action_id(), GetActionIdAtIndex(3u));

  // Order is now C, A, B, MCA. Move MCA to the location it already occupies.
  toolbar_model()->MoveActionIcon(component_action_id(), 3);
  EXPECT_EQ(3u, observer()->moved_count());
  EXPECT_EQ(4u, num_toolbar_items());
  EXPECT_EQ(browser_action_c()->id(), GetActionIdAtIndex(0u));
  EXPECT_EQ(browser_action_a()->id(), GetActionIdAtIndex(1u));
  EXPECT_EQ(browser_action_b()->id(), GetActionIdAtIndex(2u));
  EXPECT_EQ(component_action_id(), GetActionIdAtIndex(3u));

  // Order is still C, A, B, MCA. Move MCA to second to last, in preparation
  // for visibility checks.
  toolbar_model()->MoveActionIcon(component_action_id(), 2);
  EXPECT_EQ(4u, observer()->moved_count());
  EXPECT_EQ(4u, num_toolbar_items());
  EXPECT_EQ(browser_action_c()->id(), GetActionIdAtIndex(0u));
  EXPECT_EQ(browser_action_a()->id(), GetActionIdAtIndex(1u));
  EXPECT_EQ(component_action_id(), GetActionIdAtIndex(2u));
  EXPECT_EQ(browser_action_b()->id(), GetActionIdAtIndex(3u));

  // Order is now C, A, MCA, B. Show only three icons: C, A, MCA, [B].
  toolbar_model()->SetVisibleIconCount(3);
  EXPECT_EQ(3u, toolbar_model()->visible_icon_count());
  EXPECT_FALSE(toolbar_model()->all_icons_visible());

  // Show only two icons so we test MCA in the overflow. The icons should
  // be: C, A, [MCA], [B].
  toolbar_model()->SetVisibleIconCount(2);
  EXPECT_EQ(2u, toolbar_model()->visible_icon_count());
  EXPECT_FALSE(toolbar_model()->all_icons_visible());

  // Show all the icons again. Order should be C, A, MCA, B.
  toolbar_model()->SetVisibleIconCount(4);
  EXPECT_EQ(4u, toolbar_model()->visible_icon_count());
  EXPECT_TRUE(toolbar_model()->all_icons_visible());

  // Order is C, A, MCA, B. Remove C.
  ASSERT_TRUE(RemoveExtension(browser_action_c()));
  EXPECT_EQ(1u, observer()->removed_count());
  EXPECT_EQ(3u, num_toolbar_items());
  EXPECT_EQ(browser_action_a()->id(), GetActionIdAtIndex(0u));
  EXPECT_EQ(component_action_id(), GetActionIdAtIndex(1u));
  EXPECT_EQ(browser_action_b()->id(), GetActionIdAtIndex(2u));

  // Order is now A, MCA, B. Remove A.
  ASSERT_TRUE(RemoveExtension(browser_action_a()));
  EXPECT_EQ(2u, observer()->removed_count());
  EXPECT_EQ(2u, num_toolbar_items());
  EXPECT_EQ(component_action_id(), GetActionIdAtIndex(0u));
  EXPECT_EQ(browser_action_b()->id(), GetActionIdAtIndex(1u));

  // Order is now MCA, B. Remove B.
  ASSERT_TRUE(RemoveExtension(browser_action_b()));
  EXPECT_EQ(3u, observer()->removed_count());
  EXPECT_EQ(1u, num_toolbar_items());
  EXPECT_EQ(component_action_id(), GetActionIdAtIndex(0u));

  // Just MCA is visible.  Remove MCA.
  toolbar_model()->RemoveComponentAction(component_action_id());
  EXPECT_EQ(4u, observer()->removed_count());
  EXPECT_EQ(0u, num_toolbar_items());
  EXPECT_FALSE(toolbar_model()->HasComponentAction(component_action_id()));

  // Add MCA again.
  toolbar_model()->AddComponentAction(component_action_id());
  EXPECT_EQ(1u, num_toolbar_items());
  EXPECT_EQ(4u, observer()->inserted_count());
  EXPECT_TRUE(toolbar_model()->HasComponentAction(component_action_id()));
  // Newly added component actions get put at the end of the visible area.
  EXPECT_EQ(component_action_id(), GetActionIdAtIndex(0u));
  EXPECT_EQ(1u, toolbar_model()->visible_icon_count());
  EXPECT_TRUE(toolbar_model()->all_icons_visible());

  // Load extension C again.
  ASSERT_TRUE(AddExtension(browser_action_c()));
  EXPECT_EQ(5u, observer()->inserted_count());
  EXPECT_EQ(2u, num_toolbar_items());
  EXPECT_EQ(component_action_id(), GetActionIdAtIndex(0u));
  EXPECT_EQ(browser_action_c()->id(), GetActionIdAtIndex(1u));
}

TEST_F(ToolbarActionsModelUnitTest, AddAndRemoveComponentActionWithOVerflow) {
  Init();
  // Add three extension actions: A, B, C.
  ASSERT_TRUE(AddBrowserActionExtensions());
  EXPECT_EQ(3u, num_toolbar_items());

  // Hide the last icon: A, B, [C].
  toolbar_model()->SetVisibleIconCount(2);
  EXPECT_EQ(2u, toolbar_model()->visible_icon_count());
  EXPECT_EQ(browser_action_c()->id(), GetActionIdAtIndex(2u));

  // Add a component action, CA. Now the icons should be: A, B, CA, [C].
  toolbar_model()->AddComponentAction(component_action_id());
  EXPECT_EQ(4u, num_toolbar_items());
  EXPECT_EQ(3u, toolbar_model()->visible_icon_count());
  EXPECT_EQ(component_action_id(), GetActionIdAtIndex(2u));
  EXPECT_EQ(browser_action_c()->id(), GetActionIdAtIndex(3u));

  // Remove the component action. Extension C should stay in the overflow.
  // The icons should be: A, B, [C].
  toolbar_model()->RemoveComponentAction(component_action_id());
  EXPECT_EQ(3u, num_toolbar_items());
  EXPECT_EQ(2u, toolbar_model()->visible_icon_count());
  EXPECT_EQ(browser_action_c()->id(), GetActionIdAtIndex(2u));
}

TEST_F(ToolbarActionsModelUnitTest, AddComponentActionInIncognito) {
  Init();
  // Add three extension actions: A, B, C.
  ASSERT_TRUE(AddBrowserActionExtensions());
  EXPECT_EQ(3u, num_toolbar_items());

  // Enable extension C in incognito.
  extensions::ExtensionPrefs* extension_prefs =
      extensions::ExtensionPrefs::Get(profile());
  extension_prefs->SetIsIncognitoEnabled(browser_action_c()->id(), true);
  extensions::util::SetIsIncognitoEnabled(browser_action_c()->id(), profile(),
                                          true);

  // Get an incognito toolbar.
  ToolbarActionsModel* incognito_model =
      extensions::extension_action_test_util::CreateToolbarModelForProfile(
          profile()->GetOffTheRecordProfile());

  // The incognito toolbar should only have extension C.
  EXPECT_EQ(1u, incognito_model->toolbar_items().size());

  // Add a component action to the incognito toolbar. It shouldn't appear on the
  // non-incognito toolbar.
  incognito_model->AddComponentAction(component_action_id());
  EXPECT_EQ(2u, incognito_model->toolbar_items().size());
  EXPECT_EQ(2u, incognito_model->visible_icon_count());
  EXPECT_EQ(component_action_id(), GetActionIdAtIndex(1u, incognito_model));
  EXPECT_EQ(3u, num_toolbar_items());
  incognito_model->RemoveComponentAction(component_action_id());

  // Set visible count to 2 so that C is overflowed on the non-incognito
  // toolbar. Its state is A, B, [C]. C stays visible on the incognito toolbar.
  toolbar_model()->SetVisibleIconCount(2);
  EXPECT_EQ(1u, incognito_model->toolbar_items().size());
  EXPECT_EQ(1u, incognito_model->visible_icon_count());

  // Add a component action to the incognito toolbar. It shouldn't appear in the
  // overflow menu.
  incognito_model->AddComponentAction(component_action_id());
  EXPECT_EQ(2u, incognito_model->toolbar_items().size());
  EXPECT_EQ(2u, incognito_model->visible_icon_count());
  EXPECT_EQ(component_action_id(), GetActionIdAtIndex(1u, incognito_model));
}

TEST_F(ToolbarActionsModelUnitTest,
       TestUninstallVisibleExtensionDoesntBringOutOther) {
  Init();
  ASSERT_TRUE(AddBrowserActionExtensions());
  toolbar_model()->SetVisibleIconCount(2u);
  EXPECT_EQ(3u, num_toolbar_items());
  EXPECT_EQ(2u, toolbar_model()->visible_icon_count());
  EXPECT_EQ(browser_action_a()->id(), GetActionIdAtIndex(0u));
  EXPECT_EQ(browser_action_b()->id(), GetActionIdAtIndex(1u));
  EXPECT_EQ(browser_action_c()->id(), GetActionIdAtIndex(2u));

  service()->UninstallExtension(
      browser_action_b()->id(),
      extensions::UNINSTALL_REASON_FOR_TESTING,
      nullptr);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(2u, num_toolbar_items());
  EXPECT_EQ(1u, toolbar_model()->visible_icon_count());
  EXPECT_EQ(browser_action_a()->id(), GetActionIdAtIndex(0u));
  EXPECT_EQ(browser_action_c()->id(), GetActionIdAtIndex(1u));
}

TEST_F(ToolbarActionsModelUnitTest, AddComponentActionBeforeInitialization) {
  InitializeEmptyExtensionService();
  ToolbarActionsModel* toolbar_model = extensions::extension_action_test_util::
      CreateToolbarModelForProfileWithoutWaitingForReady(profile());
  ASSERT_FALSE(toolbar_model->actions_initialized());

  // AddComponentAction() should be a no-op if actions_initialized() is false.
  toolbar_model->AddComponentAction(component_action_id());
  EXPECT_EQ(0u, toolbar_model->toolbar_items().size());
  EXPECT_FALSE(toolbar_model->HasComponentAction(component_action_id()));
}

// Test that user-script extensions show up on the toolbar.
TEST_F(ToolbarActionsModelUnitTest, AddUserScriptExtension) {
  Init();

  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("a")
          .SetLocation(extensions::Manifest::INTERNAL)
          .MergeManifest(extensions::DictionaryBuilder()
                             .Set("converted_from_user_script", true)
                             .Build())
          .Build();

  // We should start off without any actions.
  EXPECT_EQ(0u, num_toolbar_items());
  EXPECT_EQ(0u, toolbar_model()->visible_icon_count());

  // Add the extension. It should be visible.
  service()->AddExtension(extension.get());
  EXPECT_EQ(1u, num_toolbar_items());
  EXPECT_EQ(1u, toolbar_model()->visible_icon_count());
  EXPECT_EQ(extension.get()->id(), GetActionIdAtIndex(0u));
}
