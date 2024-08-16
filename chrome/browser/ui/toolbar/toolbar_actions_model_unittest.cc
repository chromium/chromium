// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"

#include <stddef.h>

#include <memory>
#include <string>

#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"
#include "chrome/browser/extensions/extension_action_test_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_user_test_base.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/toolbar/test_toolbar_action_view_controller.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/crx_file/id_util.h"
#include "components/policy/core/common/policy_map.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/extension_action_manager.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/pref_names.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/api/extension_action/action_info.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/manifest.h"
#include "extensions/test/test_extension_dir.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

using extensions::mojom::ManifestLocation;

// A simple observer that tracks the number of times certain events occur.
class ToolbarActionsModelTestObserver : public ToolbarActionsModel::Observer {
 public:
  explicit ToolbarActionsModelTestObserver(ToolbarActionsModel* model);

  ToolbarActionsModelTestObserver(const ToolbarActionsModelTestObserver&) =
      delete;
  ToolbarActionsModelTestObserver& operator=(
      const ToolbarActionsModelTestObserver&) = delete;

  ~ToolbarActionsModelTestObserver() override;

  size_t inserted_count() const { return inserted_count_; }
  size_t removed_count() const { return removed_count_; }
  size_t initialized_count() const { return initialized_count_; }

  const std::vector<ToolbarActionsModel::ActionId>& last_pinned_action_ids()
      const {
    return last_pinned_action_ids_;
  }

 private:
  // ToolbarActionsModel::Observer:
  void OnToolbarActionAdded(
      const ToolbarActionsModel::ActionId& action_id) override {
    ++inserted_count_;
  }

  void OnToolbarActionRemoved(
      const ToolbarActionsModel::ActionId& id) override {
    ++removed_count_;
  }

  void OnToolbarActionUpdated(
      const ToolbarActionsModel::ActionId& id) override {}

  void OnToolbarModelInitialized() override { ++initialized_count_; }

  void OnToolbarPinnedActionsChanged() override {
    last_pinned_action_ids_ = model_->pinned_action_ids();
  }

  const raw_ptr<ToolbarActionsModel> model_;

  size_t inserted_count_;
  size_t removed_count_;
  size_t initialized_count_;

  std::vector<ToolbarActionsModel::ActionId> last_pinned_action_ids_;
};

ToolbarActionsModelTestObserver::ToolbarActionsModelTestObserver(
    ToolbarActionsModel* model)
    : model_(model),
      inserted_count_(0),
      removed_count_(0),
      initialized_count_(0) {
  model_->AddObserver(this);
}

ToolbarActionsModelTestObserver::~ToolbarActionsModelTestObserver() {
  model_->RemoveObserver(this);
}

}  // namespace

class ToolbarActionsModelUnitTest
    : public extensions::ExtensionServiceUserTestBase {
 public:
  ToolbarActionsModelUnitTest() {}

  ToolbarActionsModelUnitTest(const ToolbarActionsModelUnitTest&) = delete;
  ToolbarActionsModelUnitTest& operator=(const ToolbarActionsModelUnitTest&) =
      delete;

  ~ToolbarActionsModelUnitTest() override {}

 protected:
  // Initialize the ExtensionService, ToolbarActionsModel, and ExtensionSystem.
  void Init();

  void InitToolbarModelAndObserver();

  void TearDown() override;

  // Adds or removes the given |extension| and verify success.
  [[nodiscard]] testing::AssertionResult AddExtension(
      const scoped_refptr<const extensions::Extension>& extension);
  [[nodiscard]] testing::AssertionResult RemoveExtension(
      const scoped_refptr<const extensions::Extension>& extension);

  // Adds three extensions, all with browser actions.
  [[nodiscard]] testing::AssertionResult AddBrowserActionExtensions();

  // Adds three extensions, one each for browser action, page action, and no
  // action, and are added in that order.
  [[nodiscard]] testing::AssertionResult AddActionExtensions();

  // Returns true if the |toobar_model_| has an action with the given |id|.
  bool ModelHasActionForId(const std::string& id) const;

  // Test that certain histograms are emitted for user and non-user profiles
  // (for ChromeOS Ash we look at user accounts vs profiles).
  void RunEmitUserHistogramsTest(int incremented_histogram_count);

  ToolbarActionsModel* toolbar_model() { return toolbar_model_; }

  const ToolbarActionsModelTestObserver* observer() const {
    return model_observer_.get();
  }
  size_t num_actions() const { return toolbar_model_->action_ids().size(); }
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

 private:
  // Verifies that all extensions in |extensions| are added successfully.
  testing::AssertionResult AddAndVerifyExtensions(
      const extensions::ExtensionList& extensions);

  // The toolbar model associated with the testing profile.
  raw_ptr<ToolbarActionsModel> toolbar_model_;

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
};

void ToolbarActionsModelUnitTest::Init() {
  InitializeEmptyExtensionService();
  InitToolbarModelAndObserver();
}

void ToolbarActionsModelUnitTest::InitToolbarModelAndObserver() {
  toolbar_model_ =
      extensions::extension_action_test_util::CreateToolbarModelForProfile(
          // ExtensionServiceTestBase::profile() returns a different profile on
          // Ash if it's a guest session. testing_profile() gives use the same
          // profile, but we must downcast to satisfy the
          // CreateToolbarModelForProfile which expect a Profile.
          static_cast<Profile*>(testing_profile()));
  model_observer_ =
      std::make_unique<ToolbarActionsModelTestObserver>(toolbar_model_);
}

void ToolbarActionsModelUnitTest::TearDown() {
  model_observer_.reset();
  extensions::ExtensionServiceUserTestBase::TearDown();
}

void ToolbarActionsModelUnitTest::RunEmitUserHistogramsTest(
    int incremented_histogram_count) {
  base::HistogramTester histograms;

  InitToolbarModelAndObserver();

  histograms.ExpectTotalCount("ExtensionToolbarModel.BrowserActionsCount", 1);
  histograms.ExpectTotalCount("Extension.Toolbar.BrowserActionsCount2",
                              incremented_histogram_count);
}

testing::AssertionResult ToolbarActionsModelUnitTest::AddExtension(
    const scoped_refptr<const extensions::Extension>& extension) {
  if (registry()->enabled_extensions().GetByID(extension->id())) {
    return testing::AssertionFailure()
           << "Extension " << extension->name() << " already installed!";
  }
  service()->AddExtension(extension.get());
  if (!registry()->enabled_extensions().GetByID(extension->id())) {
    return testing::AssertionFailure()
           << "Failed to install extension: " << extension->name();
  }
  return testing::AssertionSuccess();
}

testing::AssertionResult ToolbarActionsModelUnitTest::RemoveExtension(
    const scoped_refptr<const extensions::Extension>& extension) {
  if (!registry()->enabled_extensions().GetByID(extension->id())) {
    return testing::AssertionFailure()
           << "Extension " << extension->name() << " not installed!";
  }
  service()->UnloadExtension(extension->id(),
                             extensions::UnloadedExtensionReason::DISABLE);
  if (registry()->enabled_extensions().GetByID(extension->id())) {
    return testing::AssertionFailure()
           << "Failed to unload extension: " << extension->name();
  }
  return testing::AssertionSuccess();
}

testing::AssertionResult ToolbarActionsModelUnitTest::AddActionExtensions() {
  browser_action_extension_ =
      extensions::ExtensionBuilder("browser_action")
          .SetAction(extensions::ActionInfo::Type::kBrowser)
          .SetLocation(ManifestLocation::kInternal)
          .Build();
  page_action_extension_ = extensions::ExtensionBuilder("page_action")
                               .SetAction(extensions::ActionInfo::Type::kPage)
                               .SetLocation(ManifestLocation::kInternal)
                               .Build();
  no_action_extension_ = extensions::ExtensionBuilder("no_action")
                             .SetLocation(ManifestLocation::kInternal)
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
                          .SetAction(extensions::ActionInfo::Type::kBrowser)
                          .SetLocation(ManifestLocation::kInternal)
                          .Build();
  browser_action_b_ = extensions::ExtensionBuilder("browser_actionB")
                          .SetAction(extensions::ActionInfo::Type::kBrowser)
                          .SetLocation(ManifestLocation::kInternal)
                          .Build();
  browser_action_c_ = extensions::ExtensionBuilder("browser_actionC")
                          .SetAction(extensions::ActionInfo::Type::kBrowser)
                          .SetLocation(ManifestLocation::kInternal)
                          .Build();

  extensions::ExtensionList extensions;
  extensions.push_back(browser_action_a_);
  extensions.push_back(browser_action_b_);
  extensions.push_back(browser_action_c_);

  return AddAndVerifyExtensions(extensions);
}

bool ToolbarActionsModelUnitTest::ModelHasActionForId(
    const std::string& id) const {
  for (const auto& toolbar_action_id : toolbar_model_->action_ids()) {
    if (toolbar_action_id == id)
      return true;
  }
  return false;
}

testing::AssertionResult ToolbarActionsModelUnitTest::AddAndVerifyExtensions(
    const extensions::ExtensionList& extensions) {
  for (auto iter = extensions.begin(); iter != extensions.end(); ++iter) {
    if (!AddExtension(*iter)) {
      return testing::AssertionFailure()
             << "Failed to install extension: " << (*iter)->name();
    }
  }
  return testing::AssertionSuccess();
}

// A basic test for extensions with browser actions showing up in the toolbar.
TEST_F(ToolbarActionsModelUnitTest, BasicToolbarActionsModelTest) {
  Init();

  // Starts empty.
  EXPECT_EQ(0u, observer()->inserted_count());
  EXPECT_THAT(toolbar_model()->action_ids(), ::testing::IsEmpty());
  EXPECT_THAT(toolbar_model()->pinned_action_ids(), ::testing::IsEmpty());

  // Load an extension with a browser action.
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("browser_action")
          .SetAction(extensions::ActionInfo::Type::kBrowser)
          .SetLocation(ManifestLocation::kInternal)
          .Build();
  ASSERT_TRUE(AddExtension(extension));

  // We should now find our extension in the model.
  EXPECT_EQ(1u, observer()->inserted_count());
  EXPECT_THAT(toolbar_model()->action_ids(),
              ::testing::UnorderedElementsAre(extension->id()));
  // It should be unpinned.
  EXPECT_THAT(toolbar_model()->pinned_action_ids(), ::testing::IsEmpty());

  // Remove the extension and verify it is removed in the model.
  ASSERT_TRUE(RemoveExtension(extension));
  EXPECT_EQ(1u, observer()->removed_count());
  EXPECT_THAT(toolbar_model()->action_ids(), ::testing::IsEmpty());
  EXPECT_THAT(toolbar_model()->pinned_action_ids(), ::testing::IsEmpty());
}

// Test that new extension actions are always visible on installation and
// inserted at the "end" of the visible section.
TEST_F(ToolbarActionsModelUnitTest, NewToolbarExtensionsAreUnpinned) {
  Init();

  // Three extensions with actions.
  scoped_refptr<const extensions::Extension> extension_a =
      extensions::ExtensionBuilder("a")
          .SetAction(extensions::ActionInfo::Type::kBrowser)
          .SetLocation(ManifestLocation::kInternal)
          .Build();
  scoped_refptr<const extensions::Extension> extension_b =
      extensions::ExtensionBuilder("b")
          .SetAction(extensions::ActionInfo::Type::kBrowser)
          .SetLocation(ManifestLocation::kInternal)
          .Build();
  scoped_refptr<const extensions::Extension> extension_c =
      extensions::ExtensionBuilder("c")
          .SetAction(extensions::ActionInfo::Type::kBrowser)
          .SetLocation(ManifestLocation::kInternal)
          .Build();

  // We should start off without any actions.
  EXPECT_EQ(0u, num_actions());

  // Add one action. It should be unpinned.
  service()->AddExtension(extension_a.get());
  EXPECT_EQ(1u, num_actions());
  EXPECT_THAT(toolbar_model()->pinned_action_ids(), ::testing::IsEmpty());

  // Add a second. It should also be unpinned (even with existing extensions,
  // default state is unpinned).
  service()->AddExtension(extension_b.get());
  EXPECT_EQ(2u, num_actions());
  EXPECT_THAT(toolbar_model()->pinned_action_ids(), ::testing::IsEmpty());

  // Pin the second. It should now be the only pinned icon.
  toolbar_model()->SetActionVisibility(extension_b->id(), true);
  EXPECT_EQ(2u, num_actions());
  EXPECT_THAT(toolbar_model()->pinned_action_ids(),
              ::testing::ElementsAre(extension_b->id()));

  // Add a third extension. It should be unpinned (pin state should not carry
  // to new extensions).
  service()->AddExtension(extension_c.get());
  EXPECT_EQ(3u, num_actions());
  EXPECT_THAT(toolbar_model()->pinned_action_ids(),
              ::testing::ElementsAre(extension_b->id()));
}

// Test that the model contains all types of extensions, except those which
// should not be displayed on the toolbar (like component extensions).
TEST_F(ToolbarActionsModelUnitTest, TestToolbarExtensionTypesEnabledSwitch) {
  Init();

  ASSERT_TRUE(AddActionExtensions());

  // extensions with page actions and no action should also be displayed in the
  // toolbar.
  EXPECT_THAT(
      toolbar_model()->action_ids(),
      testing::UnorderedElementsAre(browser_action()->id(), page_action()->id(),
                                    no_action()->id()));

  // Extensions that are installed by default shouldn't be given an icon.
  auto default_installed_manifest =
      base::Value::Dict()
          .Set("name", "default installed")
          .Set("description", "A default installed extension")
          .Set("manifest_version", 2)
          .Set("version", "1.0.0.0");
  scoped_refptr<const extensions::Extension> default_installed_extension =
      extensions::ExtensionBuilder()
          .SetManifest(std::move(default_installed_manifest))
          .SetID(crx_file::id_util::GenerateId("default"))
          .SetLocation(ManifestLocation::kInternal)
          .AddFlags(extensions::Extension::WAS_INSTALLED_BY_DEFAULT)
          .Build();
  EXPECT_TRUE(AddExtension(default_installed_extension.get()));
  EXPECT_EQ(3u, num_actions());
  EXPECT_FALSE(ModelHasActionForId(default_installed_extension->id()));

  // Component extensions shouldn't be given an icon.
  scoped_refptr<const extensions::Extension> component_extension_no_action =
      extensions::ExtensionBuilder("component ext no action")
          .SetLocation(ManifestLocation::kComponent)
          .Build();
  EXPECT_TRUE(AddExtension(component_extension_no_action.get()));
  EXPECT_EQ(3u, num_actions());
  EXPECT_FALSE(ModelHasActionForId(component_extension_no_action->id()));

  // Sanity check: A new extension that's installed from the webstore should
  // have an icon.
  scoped_refptr<const extensions::Extension> internal_extension_no_action =
      extensions::ExtensionBuilder("internal ext no action")
          .SetLocation(ManifestLocation::kInternal)
          .Build();
  EXPECT_TRUE(AddExtension(internal_extension_no_action.get()));
  EXPECT_EQ(4u, num_actions());
  EXPECT_TRUE(ModelHasActionForId(internal_extension_no_action->id()));
}

TEST_F(ToolbarActionsModelUnitTest, PinnedStateIsTransferredToIncognito) {
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

  // Pin extensions A and C. State is A, C, [B].
  toolbar_model()->SetActionVisibility(browser_action_a()->id(), true);
  toolbar_model()->SetActionVisibility(browser_action_c()->id(), true);
  EXPECT_THAT(toolbar_model()->pinned_action_ids(),
              ::testing::ElementsAre(browser_action_a()->id(),
                                     browser_action_c()->id()));

  // Get an incognito profile and toolbar.
  ToolbarActionsModel* incognito_model =
      extensions::extension_action_test_util::CreateToolbarModelForProfile(
          profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true));

  // We should have two actions in the incognito bar, C and B. The pinned state
  // should be preserved, so C should be pinned.
  EXPECT_THAT(incognito_model->action_ids(),
              ::testing::ElementsAre(browser_action_b()->id(),
                                     browser_action_c()->id()));
  EXPECT_THAT(incognito_model->pinned_action_ids(),
              ::testing::ElementsAre(browser_action_c()->id()));

  // Pinning from the original profile transfers to the incognito profile, so
  // pinning B results in a change.
  toolbar_model()->SetActionVisibility(browser_action_b()->id(), true);
  EXPECT_THAT(incognito_model->pinned_action_ids(),
              ::testing::ElementsAre(browser_action_c()->id(),
                                     browser_action_b()->id()));
  // Similarly, unpinning C transfers to the incognito profile.
  toolbar_model()->SetActionVisibility(browser_action_c()->id(), false);
  EXPECT_THAT(incognito_model->pinned_action_ids(),
              ::testing::ElementsAre(browser_action_b()->id()));
}

TEST_F(ToolbarActionsModelUnitTest,
       MovingPinnedActionsTransfersBetweenIncognito) {
  Init();
  ASSERT_TRUE(AddBrowserActionExtensions());

  // Give all extensions incognito access.
  // Note: We use ExtensionPrefs::SetIsIncognitoEnabled instead of
  // util::SetIsIncognitoEnabled because the latter tries to reload the
  // extension, which requires a filepath associated with the extension (and,
  // for this test, reloading the extension is irrelevant to us).
  extensions::ExtensionPrefs* extension_prefs =
      extensions::ExtensionPrefs::Get(profile());
  extension_prefs->SetIsIncognitoEnabled(browser_action_a()->id(), true);
  extension_prefs->SetIsIncognitoEnabled(browser_action_b()->id(), true);
  extension_prefs->SetIsIncognitoEnabled(browser_action_c()->id(), true);

  // Pin all extensions, to allow moving them around.
  toolbar_model()->SetActionVisibility(browser_action_a()->id(), true);
  toolbar_model()->SetActionVisibility(browser_action_b()->id(), true);
  toolbar_model()->SetActionVisibility(browser_action_c()->id(), true);

  // Get an incognito profile and toolbar.
  ToolbarActionsModel* incognito_model =
      extensions::extension_action_test_util::CreateToolbarModelForProfile(
          profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true));

  // The incognito pinned actions should be A, B, C (matching the order from
  // the on-the-record profile).
  EXPECT_THAT(
      incognito_model->pinned_action_ids(),
      ::testing::ElementsAre(browser_action_a()->id(), browser_action_b()->id(),
                             browser_action_c()->id()));

  // Moving extension C to index 0 affects both profiles.
  toolbar_model()->MovePinnedAction(browser_action_c()->id(), 0);
  EXPECT_THAT(
      toolbar_model()->pinned_action_ids(),
      ::testing::ElementsAre(browser_action_c()->id(), browser_action_a()->id(),
                             browser_action_b()->id()));
  EXPECT_THAT(
      incognito_model->pinned_action_ids(),
      ::testing::ElementsAre(browser_action_c()->id(), browser_action_a()->id(),
                             browser_action_b()->id()));
}

// Test that enabling extensions incognito with an active incognito profile
// works.
TEST_F(ToolbarActionsModelUnitTest, ActionsToolbarIncognitoEnableExtension) {
  Init();

  static constexpr char kManifest[] =
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
  for (size_t i = 0; i < std::size(dirs); ++i) {
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

  // Pin Extension A in the on-the-record profile.
  toolbar_model()->SetActionVisibility(extension_a, true);
  EXPECT_THAT(toolbar_model()->pinned_action_ids(),
              ::testing::ElementsAre(extension_a));

  // Get an incognito profile and toolbar.
  ToolbarActionsModel* incognito_model =
      extensions::extension_action_test_util::CreateToolbarModelForProfile(
          profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true));
  ToolbarActionsModelTestObserver incognito_observer(incognito_model);

  // Right now, no extensions are enabled in incognito mode.
  EXPECT_THAT(incognito_model->action_ids(), ::testing::IsEmpty());

  // Set extension B (which is unpinned) to be enabled in incognito. This
  // results in b reloading, so wait for it.
  {
    extensions::TestExtensionRegistryObserver observer(registry(), extension_b);
    extensions::util::SetIsIncognitoEnabled(extension_b, profile(), true);
    observer.WaitForExtensionLoaded();
  }

  // Now, we should have one icon in the incognito bar. But, since B is
  // unpinned in the main bar, it shouldn't be visible.
  EXPECT_THAT(incognito_model->action_ids(),
              ::testing::ElementsAre(extension_b));
  EXPECT_THAT(incognito_model->pinned_action_ids(), ::testing::IsEmpty());

  // Also enable extension A for incognito (again, wait for the reload).
  {
    extensions::TestExtensionRegistryObserver observer(registry(), extension_a);
    extensions::util::SetIsIncognitoEnabled(extension_a, profile(), true);
    observer.WaitForExtensionLoaded();
  }

  // Now, both extensions should be enabled in incognito mode. Extension A
  // should be pinned (since it's pinned in the main bar).
  EXPECT_THAT(incognito_model->action_ids(),
              ::testing::UnorderedElementsAre(extension_a, extension_b));
  EXPECT_THAT(incognito_model->pinned_action_ids(),
              ::testing::ElementsAre(extension_a));
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

// Test that user-script extensions show up on the toolbar.
TEST_F(ToolbarActionsModelUnitTest, AddUserScriptExtension) {
  Init();

  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("a")
          .SetLocation(ManifestLocation::kInternal)
          .MergeManifest(
              base::Value::Dict().Set("converted_from_user_script", true))
          .Build();

  // We should start off without any actions.
  EXPECT_EQ(0u, num_actions());

  // Add the extension and verify it gets an icon.
  service()->AddExtension(extension.get());
  EXPECT_THAT(toolbar_model()->action_ids(),
              ::testing::UnorderedElementsAre(extension->id()));
}

TEST_F(ToolbarActionsModelUnitTest, IsActionPinnedCorrespondsToPinningState) {
  Init();
  ASSERT_TRUE(AddBrowserActionExtensions());

  // The actions should initially not be pinned.
  EXPECT_FALSE(toolbar_model()->IsActionPinned(browser_action_a()->id()));

  // Pinning is reflected in |IsActionPinned|.
  toolbar_model()->SetActionVisibility(browser_action_a()->id(), true);
  EXPECT_TRUE(toolbar_model()->IsActionPinned(browser_action_a()->id()));

  // Removing pinning should also be reflected in |IsActionPinned|.
  toolbar_model()->SetActionVisibility(browser_action_a()->id(), false);
  EXPECT_FALSE(toolbar_model()->IsActionPinned(browser_action_a()->id()));
}

TEST_F(ToolbarActionsModelUnitTest,
       TogglingVisibilityAppendsToPinnedExtensions) {
  Init();
  ASSERT_TRUE(AddBrowserActionExtensions());

  EXPECT_THAT(toolbar_model()->pinned_action_ids(), testing::IsEmpty());

  toolbar_model()->SetActionVisibility(browser_action_a()->id(), true);
  EXPECT_THAT(toolbar_model()->pinned_action_ids(),
              testing::ElementsAre(browser_action_a()->id()));

  // Pin the remaining two extensions.
  toolbar_model()->SetActionVisibility(browser_action_b()->id(), true);
  toolbar_model()->SetActionVisibility(browser_action_c()->id(), true);

  // Verify that they are added in order.
  EXPECT_THAT(
      toolbar_model()->pinned_action_ids(),
      testing::ElementsAre(browser_action_a()->id(), browser_action_b()->id(),
                           browser_action_c()->id()));

  // Verify that unpinning an extension updates the list of pinned ids.
  toolbar_model()->SetActionVisibility(browser_action_b()->id(), false);
  EXPECT_THAT(
      toolbar_model()->pinned_action_ids(),
      testing::ElementsAre(browser_action_a()->id(), browser_action_c()->id()));

  // Verify that re-pinning an extension adds it back to the end of the list.
  toolbar_model()->SetActionVisibility(browser_action_b()->id(), true);
  EXPECT_THAT(
      toolbar_model()->pinned_action_ids(),
      testing::ElementsAre(browser_action_a()->id(), browser_action_c()->id(),
                           browser_action_b()->id()));
}

TEST_F(ToolbarActionsModelUnitTest, ChangesToPinningNotifiesObserver) {
  Init();
  ASSERT_TRUE(AddBrowserActionExtensions());

  // The observer should not think that any extensions are initially pinned.
  EXPECT_THAT(observer()->last_pinned_action_ids(), testing::IsEmpty());

  // Verify that pinning the action notifies the observer.
  toolbar_model()->SetActionVisibility(browser_action_a()->id(), true);
  EXPECT_THAT(observer()->last_pinned_action_ids(),
              testing::ElementsAre(browser_action_a()->id()));

  // Verify that un-pinning an action also notifies the observer.
  toolbar_model()->SetActionVisibility(browser_action_a()->id(), false);
  EXPECT_THAT(observer()->last_pinned_action_ids(), testing::IsEmpty());
}

TEST_F(ToolbarActionsModelUnitTest, ChangesToPinningSavedInExtensionPrefs) {
  Init();
  ASSERT_TRUE(AddBrowserActionExtensions());

  extensions::ExtensionPrefs* const extension_prefs =
      extensions::ExtensionPrefs::Get(profile());

  // The preferences shouldn't have any extensions initially pinned.
  EXPECT_THAT(extension_prefs->GetPinnedExtensions(), testing::IsEmpty());

  // Verify that pinned extensions are reflected in preferences.
  toolbar_model()->SetActionVisibility(browser_action_a()->id(), true);
  toolbar_model()->SetActionVisibility(browser_action_b()->id(), true);
  toolbar_model()->SetActionVisibility(browser_action_c()->id(), true);
  EXPECT_THAT(
      extension_prefs->GetPinnedExtensions(),
      testing::ElementsAre(browser_action_a()->id(), browser_action_b()->id(),
                           browser_action_c()->id()));

  // Verify that un-pinning an action is also reflected in preferences.
  toolbar_model()->SetActionVisibility(browser_action_b()->id(), false);
  EXPECT_THAT(
      extension_prefs->GetPinnedExtensions(),
      testing::ElementsAre(browser_action_a()->id(), browser_action_c()->id()));

  // Verify that re-pinning is added last.
  toolbar_model()->SetActionVisibility(browser_action_b()->id(), true);
  EXPECT_THAT(
      extension_prefs->GetPinnedExtensions(),
      testing::ElementsAre(browser_action_a()->id(), browser_action_c()->id(),
                           browser_action_b()->id()));
}

TEST_F(ToolbarActionsModelUnitTest, ChangesToExtensionPrefsReflectedInModel) {
  Init();
  ASSERT_TRUE(AddBrowserActionExtensions());

  extensions::ExtensionPrefs* const extension_prefs =
      extensions::ExtensionPrefs::Get(profile());

  // No actions should be initially pinned.
  EXPECT_THAT(toolbar_model()->pinned_action_ids(), testing::IsEmpty());

  // Update preferences to indicate that extensions A and C are pinned.
  extensions::ExtensionIdList pinned_extension_list = {
      browser_action_a()->id(), browser_action_c()->id()};

  // Verify that setting the extension preferences updates the model.
  extension_prefs->SetPinnedExtensions(pinned_extension_list);
  EXPECT_EQ(pinned_extension_list, extension_prefs->GetPinnedExtensions());
  EXPECT_EQ(pinned_extension_list, toolbar_model()->pinned_action_ids());

  // Verify that the observer is notified as well.
  EXPECT_EQ(pinned_extension_list, observer()->last_pinned_action_ids());
}

TEST_F(ToolbarActionsModelUnitTest,
       MismatchInPinnedExtensionPreferencesNotReflectedInModel) {
  Init();
  ASSERT_TRUE(AddBrowserActionExtensions());

  extensions::ExtensionPrefs* const extension_prefs =
      extensions::ExtensionPrefs::Get(profile());

  // No actions should be initially pinned.
  EXPECT_THAT(toolbar_model()->pinned_action_ids(), testing::IsEmpty());

  // Update preferences to indicate that extensions A and C are pinned.
  extensions::ExtensionIdList pinned_extension_list = {
      browser_action_a()->id(), browser_action_c()->id()};
  extensions::ExtensionIdList pinned_extension_list_with_additional_id =
      pinned_extension_list;
  pinned_extension_list_with_additional_id.push_back("bogus id");

  // Verify that setting the extension preferences updates the model and that
  // the additional extension id is filtered out in the model.
  extension_prefs->SetPinnedExtensions(
      pinned_extension_list_with_additional_id);
  EXPECT_EQ(pinned_extension_list_with_additional_id,
            extension_prefs->GetPinnedExtensions());
  EXPECT_EQ(pinned_extension_list, toolbar_model()->pinned_action_ids());

  // Verify that the observer is notified as well.
  EXPECT_EQ(pinned_extension_list, observer()->last_pinned_action_ids());
}

TEST_F(ToolbarActionsModelUnitTest, PinnedExtensionsFilteredOnInitialization) {
  Init();
  ASSERT_TRUE(AddBrowserActionExtensions());

  extensions::ExtensionPrefs* const extension_prefs =
      extensions::ExtensionPrefs::Get(profile());

  // Update preferences to indicate that extensions A and a "bogus id" one is
  // set.
  extensions::ExtensionIdList pinned_extension_list_with_additional_id = {
      browser_action_a()->id(), "bogus id"};
  extension_prefs->SetPinnedExtensions(
      pinned_extension_list_with_additional_id);

  // Create a model after setting the prefs, this is done to ensure that the
  // pinned preferences are loaded and correctly filtered.
  ToolbarActionsModel model_created_after_prefs_set(profile(), extension_prefs);
  // Wait for load to happen (::OnReady is posted from ToolbarActionModel's
  // constructor).
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(pinned_extension_list_with_additional_id,
            extension_prefs->GetPinnedExtensions());

  // Verify that the new model loads the same action_ids() and
  // pinned_action_ids() from ExtensionPrefs that |toolbar_model()| should have
  // saved.
  EXPECT_EQ(toolbar_model()->pinned_action_ids(),
            model_created_after_prefs_set.pinned_action_ids());
  EXPECT_EQ(toolbar_model()->action_ids(),
            model_created_after_prefs_set.action_ids());

  // Verify that the new model's pinned action IDs have been pruned down to only
  // extension a.
  EXPECT_THAT(model_created_after_prefs_set.pinned_action_ids(),
              testing::ElementsAre(browser_action_a()->id()));
}

TEST_F(ToolbarActionsModelUnitTest, ChangesToPinnedOrderSavedInExtensionPrefs) {
  Init();
  ASSERT_TRUE(AddBrowserActionExtensions());

  extensions::ExtensionPrefs* const extension_prefs =
      extensions::ExtensionPrefs::Get(profile());

  // The preferences shouldn't have any extensions initially pinned.
  EXPECT_THAT(extension_prefs->GetPinnedExtensions(), testing::IsEmpty());

  // Verify that pinned extensions are reflected in preferences.
  toolbar_model()->SetActionVisibility(browser_action_a()->id(), true);
  toolbar_model()->SetActionVisibility(browser_action_b()->id(), true);
  toolbar_model()->SetActionVisibility(browser_action_c()->id(), true);
  EXPECT_THAT(
      extension_prefs->GetPinnedExtensions(),
      testing::ElementsAre(browser_action_a()->id(), browser_action_b()->id(),
                           browser_action_c()->id()));

  // Verify that moving an action left to right is reflected in preferences.
  // Note: Use index 1 (instead of 2) because moving to the end of the list
  // is handled differently.
  toolbar_model()->MovePinnedAction(browser_action_a()->id(), 1);
  EXPECT_THAT(
      extension_prefs->GetPinnedExtensions(),
      testing::ElementsAre(browser_action_b()->id(), browser_action_a()->id(),
                           browser_action_c()->id()));

  // Verify that moving an action right to left is reflected in preferences.
  toolbar_model()->MovePinnedAction(browser_action_a()->id(), 0);
  EXPECT_THAT(
      extension_prefs->GetPinnedExtensions(),
      testing::ElementsAre(browser_action_a()->id(), browser_action_b()->id(),
                           browser_action_c()->id()));

  // Verify that moving an action to index greater than rightmost index is
  // reflected in preferences as at the right end.
  toolbar_model()->MovePinnedAction(browser_action_b()->id(), 4);
  EXPECT_THAT(
      extension_prefs->GetPinnedExtensions(),
      testing::ElementsAre(browser_action_a()->id(), browser_action_c()->id(),
                           browser_action_b()->id()));

  // "Moving" an icon to its current position should be a no-op.
  toolbar_model()->MovePinnedAction(browser_action_c()->id(), 1);
  EXPECT_THAT(
      extension_prefs->GetPinnedExtensions(),
      testing::ElementsAre(browser_action_a()->id(), browser_action_c()->id(),
                           browser_action_b()->id()));

  // Repeat the above tests, but add in extra IDs into prefs (representing
  // extensions that could be installed, but not loaded). These unloaded
  // extensions' states should be preserved.
  constexpr char kExtraId1[] = "extra1";
  constexpr char kExtraId2[] = "extra2";
  extension_prefs->SetPinnedExtensions({browser_action_a()->id(), kExtraId1,
                                        kExtraId2, browser_action_c()->id(),
                                        browser_action_b()->id()});

  EXPECT_THAT(
      toolbar_model()->pinned_action_ids(),
      testing::ElementsAre(browser_action_a()->id(), browser_action_c()->id(),
                           browser_action_b()->id()));

  // Move right to left.
  toolbar_model()->MovePinnedAction(browser_action_c()->id(), 0);
  EXPECT_THAT(
      extension_prefs->GetPinnedExtensions(),
      testing::ElementsAre(browser_action_c()->id(), browser_action_a()->id(),
                           kExtraId1, kExtraId2, browser_action_b()->id()));

  // Move left to right.
  toolbar_model()->MovePinnedAction(browser_action_c()->id(), 1);
  EXPECT_THAT(
      extension_prefs->GetPinnedExtensions(),
      testing::ElementsAre(browser_action_a()->id(), kExtraId1, kExtraId2,
                           browser_action_c()->id(), browser_action_b()->id()));

  // Move past the right-most index (of the visible actions).
  toolbar_model()->MovePinnedAction(browser_action_a()->id(), 4);
  EXPECT_THAT(
      extension_prefs->GetPinnedExtensions(),
      testing::ElementsAre(kExtraId1, kExtraId2, browser_action_c()->id(),
                           browser_action_b()->id(), browser_action_a()->id()));

  // "Move" to the current position.
  toolbar_model()->MovePinnedAction(browser_action_b()->id(), 1);
  EXPECT_THAT(
      extension_prefs->GetPinnedExtensions(),
      testing::ElementsAre(kExtraId1, kExtraId2, browser_action_c()->id(),
                           browser_action_b()->id(), browser_action_a()->id()));
}

TEST_F(ToolbarActionsModelUnitTest, PinStateErasedOnUninstallation) {
  Init();

  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("extension")
          .SetAction(extensions::ActionInfo::Type::kBrowser)
          .SetLocation(ManifestLocation::kInternal)
          .Build();

  // Add and pin an extension.
  EXPECT_TRUE(AddExtension(extension));
  EXPECT_FALSE(toolbar_model()->IsActionPinned(extension->id()));
  extensions::ExtensionPrefs* const prefs =
      extensions::ExtensionPrefs::Get(profile());
  EXPECT_THAT(prefs->GetPinnedExtensions(), testing::IsEmpty());

  toolbar_model()->SetActionVisibility(extension->id(), true);
  EXPECT_TRUE(toolbar_model()->IsActionPinned(extension->id()));
  EXPECT_THAT(prefs->GetPinnedExtensions(),
              testing::ElementsAre(extension->id()));

  // Uninstall the extension. The pin state should be forgotten.
  service()->UninstallExtension(
      extension->id(), extensions::UNINSTALL_REASON_FOR_TESTING, nullptr);

  EXPECT_FALSE(toolbar_model()->IsActionPinned(extension->id()));
  EXPECT_THAT(prefs->GetPinnedExtensions(), testing::IsEmpty());

  // Re-add the extension. It should be in the default (unpinned) state.
  EXPECT_TRUE(AddExtension(extension));
  EXPECT_FALSE(toolbar_model()->IsActionPinned(extension->id()));
  EXPECT_THAT(prefs->GetPinnedExtensions(), testing::IsEmpty());
}

TEST_F(ToolbarActionsModelUnitTest, ForcePinnedByPolicy) {
  Init();

  // Set the extension to force-pin via enterprise policy.
  std::string extension_id = crx_file::id_util::GenerateId("qwertyuiop");
  std::string json = base::StringPrintf(
      R"({
        "%s": {
          "toolbar_pin": "force_pinned"
        }
      })",
      extension_id.c_str());
  std::optional<base::Value> parsed = base::JSONReader::Read(json);
  policy::PolicyMap map;
  map.Set("ExtensionSettings", policy::POLICY_LEVEL_MANDATORY,
          policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_PLATFORM,
          std::move(parsed), nullptr);
  policy_provider()->UpdateChromePolicy(map);

  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("test")
          .SetAction(extensions::ActionInfo::Type::kBrowser)
          .SetLocation(ManifestLocation::kInternal)
          .SetID(extension_id)
          .Build();

  // Add an extension. It should auto-pin because of the ExtensionSettings
  // policy.
  EXPECT_TRUE(AddExtension(extension));
  EXPECT_TRUE(toolbar_model()->IsActionPinned(extension->id()));
  auto* prefs = extensions::ExtensionPrefs::Get(profile());
  EXPECT_FALSE(base::Contains(prefs->GetPinnedExtensions(), extension_id));

  // Pin all other extensions, to allow moving them around.
  ASSERT_TRUE(AddBrowserActionExtensions());
  const auto& id_a = browser_action_a()->id();
  const auto& id_b = browser_action_b()->id();
  const auto& id_c = browser_action_c()->id();
  toolbar_model()->SetActionVisibility(id_a, true);
  toolbar_model()->SetActionVisibility(id_b, true);
  toolbar_model()->SetActionVisibility(id_c, true);

  // Force-pinned extensions aren't saved in the pref.
  EXPECT_THAT(prefs->GetPinnedExtensions(),
              testing::ElementsAre(id_a, id_b, id_c));
  EXPECT_THAT(toolbar_model()->pinned_action_ids(),
              testing::ElementsAre(id_a, id_b, id_c, extension_id));

  // Try to move the force-pinned extension. This shouldn't do anything because
  // they can't be moved. See crbug.com/1266952.
  toolbar_model()->MovePinnedAction(extension_id, 1);
  EXPECT_THAT(prefs->GetPinnedExtensions(),
              testing::ElementsAre(id_a, id_b, id_c));
  EXPECT_THAT(toolbar_model()->pinned_action_ids(),
              testing::ElementsAre(id_a, id_b, id_c, extension_id));

  // Try to move other extensions. This should work fine.
  toolbar_model()->MovePinnedAction(id_a, 1);
  EXPECT_THAT(prefs->GetPinnedExtensions(),
              testing::ElementsAre(id_b, id_a, id_c));
  EXPECT_THAT(toolbar_model()->pinned_action_ids(),
              testing::ElementsAre(id_b, id_a, id_c, extension_id));

  toolbar_model()->MovePinnedAction(id_a, 2);
  EXPECT_THAT(prefs->GetPinnedExtensions(),
              testing::ElementsAre(id_b, id_c, id_a));
  EXPECT_THAT(toolbar_model()->pinned_action_ids(),
              testing::ElementsAre(id_b, id_c, id_a, extension_id));

  toolbar_model()->MovePinnedAction(id_a, 0);
  EXPECT_THAT(prefs->GetPinnedExtensions(),
              testing::ElementsAre(id_a, id_b, id_c));
  EXPECT_THAT(toolbar_model()->pinned_action_ids(),
              testing::ElementsAre(id_a, id_b, id_c, extension_id));

  // Try to move an extension to the right of the force-pinned one. This will
  // not work, and the force-pinned one will stay to the right. But the other
  // extension will still get moved as far right as it can.
  toolbar_model()->MovePinnedAction(id_a, 3);
  EXPECT_THAT(prefs->GetPinnedExtensions(),
              testing::ElementsAre(id_b, id_c, id_a));
  EXPECT_THAT(toolbar_model()->pinned_action_ids(),
              testing::ElementsAre(id_b, id_c, id_a, extension_id));

  // Again, but using an index greater than the rightmost index (mostly to check
  // for crashes).
  toolbar_model()->MovePinnedAction(id_a, 0);
  EXPECT_THAT(prefs->GetPinnedExtensions(),
              testing::ElementsAre(id_a, id_b, id_c));
  EXPECT_THAT(toolbar_model()->pinned_action_ids(),
              testing::ElementsAre(id_a, id_b, id_c, extension_id));
  toolbar_model()->MovePinnedAction(id_a, 4);
  EXPECT_THAT(prefs->GetPinnedExtensions(),
              testing::ElementsAre(id_b, id_c, id_a));
  EXPECT_THAT(toolbar_model()->pinned_action_ids(),
              testing::ElementsAre(id_b, id_c, id_a, extension_id));
}

// Tests that the pin state (and position) for extensions that are unloaded
// (but *not* uninstalled) is preserved, even if the pinning order was modified
// while they were unloaded.
// Regression test for crbug.com/1203899.
TEST_F(ToolbarActionsModelUnitTest, UnloadedExtensionsPinnedStatePreserved) {
  Init();
  ASSERT_TRUE(AddBrowserActionExtensions());

  EXPECT_THAT(toolbar_model()->action_ids(),
              ::testing::UnorderedElementsAre(browser_action_a()->id(),
                                              browser_action_b()->id(),
                                              browser_action_c()->id()));
  EXPECT_THAT(toolbar_model()->pinned_action_ids(), ::testing::IsEmpty());
  // Pin all of them.
  toolbar_model()->SetActionVisibility(browser_action_a()->id(), true);
  toolbar_model()->SetActionVisibility(browser_action_b()->id(), true);
  toolbar_model()->SetActionVisibility(browser_action_c()->id(), true);

  EXPECT_THAT(
      toolbar_model()->pinned_action_ids(),
      ::testing::ElementsAre(browser_action_a()->id(), browser_action_b()->id(),
                             browser_action_c()->id()));

  // Disable extension A. It should no longer be reflected in the pinned
  // extensions (or the actions at all).
  service()->DisableExtension(browser_action_a()->id(),
                              extensions::disable_reason::DISABLE_USER_ACTION);
  EXPECT_THAT(toolbar_model()->action_ids(),
              ::testing::UnorderedElementsAre(browser_action_b()->id(),
                                              browser_action_c()->id()));
  EXPECT_THAT(toolbar_model()->pinned_action_ids(),
              ::testing::ElementsAre(browser_action_b()->id(),
                                     browser_action_c()->id()));

  // Re-enable extension A. It should retain it's pinned status (and position,
  // at index 0).
  service()->EnableExtension(browser_action_a()->id());
  EXPECT_THAT(toolbar_model()->action_ids(),
              ::testing::UnorderedElementsAre(browser_action_a()->id(),
                                              browser_action_b()->id(),
                                              browser_action_c()->id()));
  EXPECT_THAT(
      toolbar_model()->pinned_action_ids(),
      ::testing::ElementsAre(browser_action_a()->id(), browser_action_b()->id(),
                             browser_action_c()->id()));

  // Repeat the unload, reload flow, but move a pinned action
  // (https://crbug.com/1203899) and unpin an action
  // (https://crbug.com/1205561) between the unload and the reload.
  service()->DisableExtension(browser_action_a()->id(),
                              extensions::disable_reason::DISABLE_USER_ACTION);
  toolbar_model()->MovePinnedAction(browser_action_b()->id(), 1u);
  toolbar_model()->SetActionVisibility(browser_action_b()->id(), false);

  // Interim: state should include both B and C, but only C should be pinned.
  EXPECT_THAT(toolbar_model()->action_ids(),
              ::testing::UnorderedElementsAre(browser_action_b()->id(),
                                              browser_action_c()->id()));
  EXPECT_THAT(toolbar_model()->pinned_action_ids(),
              ::testing::ElementsAre(browser_action_c()->id()));

  // Reload - state should include all of A, B, C, with pinned order of A, C.
  service()->EnableExtension(browser_action_a()->id());
  EXPECT_THAT(toolbar_model()->action_ids(),
              ::testing::UnorderedElementsAre(browser_action_a()->id(),
                                              browser_action_b()->id(),
                                              browser_action_c()->id()));
  EXPECT_THAT(toolbar_model()->pinned_action_ids(),
              ::testing::ElementsAre(browser_action_a()->id(),
                                     browser_action_c()->id()));
}

TEST_F(ToolbarActionsModelUnitTest, InitActionList_EmitUserHistograms) {
  InitializeEmptyExtensionService();
  ASSERT_NO_FATAL_FAILURE(MaybeSetUpTestUser(
      /*is_guest=*/false));
  RunEmitUserHistogramsTest(/*incremented_histogram_count=*/1);
}

TEST_F(ToolbarActionsModelUnitTest, InitActionList_NonUserEmitHistograms) {
  InitializeEmptyExtensionService();
  ASSERT_NO_FATAL_FAILURE(MaybeSetUpTestUser(
      /*is_guest=*/true));
  RunEmitUserHistogramsTest(/*incremented_histogram_count=*/0);
}
