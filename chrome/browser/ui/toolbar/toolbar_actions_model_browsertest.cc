// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_set.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

// We use some arbitrary extensions. Use constants for more clarity.
constexpr char kExtension1Path[] = "simple_with_file";
constexpr char kExtension1Name[] = "foo";
constexpr char kExtension2Path[] = "simple_with_icon";
constexpr char kExtension2Name[] = "Simple Extension with icon";
constexpr char kExtension3Path[] = "simple_with_host";
constexpr char kExtension3Name[] = "bar";

}  // namespace

class ToolbarActionsModelBrowserTest : public extensions::ExtensionBrowserTest {
 public:
  ToolbarActionsModelBrowserTest() = default;
  ~ToolbarActionsModelBrowserTest() override = default;

  void SetUpOnMainThread() override {
    extensions::ExtensionBrowserTest::SetUpOnMainThread();
    toolbar_model_ = ToolbarActionsModel::Get(profile());
    ASSERT_TRUE(toolbar_model_);
  }

  void TearDownOnMainThread() override {
    toolbar_model_ = nullptr;
    extensions::ExtensionBrowserTest::TearDownOnMainThread();
  }

  ToolbarActionsModel* toolbar_model() { return toolbar_model_; }
  base::HistogramTester* histogram_tester() { return &histogram_tester_; }

 private:
  raw_ptr<ToolbarActionsModel> toolbar_model_ = nullptr;
  base::HistogramTester histogram_tester_;
};

IN_PROC_BROWSER_TEST_F(ToolbarActionsModelBrowserTest, PRE_PinnedStateMetrics) {
  const extensions::Extension* extension1 =
      LoadExtension(test_data_dir_.AppendASCII("simple_with_file"));
  ASSERT_TRUE(extension1);

  const extensions::Extension* extension2 =
      LoadExtension(test_data_dir_.AppendASCII("simple_with_icon"));
  ASSERT_TRUE(extension2);

  EXPECT_EQ(2u, toolbar_model()->action_ids().size());

  EXPECT_FALSE(toolbar_model()->IsActionPinned(extension1->id()));
  EXPECT_FALSE(toolbar_model()->IsActionPinned(extension2->id()));
  toolbar_model()->SetActionVisibility(extension1->id(), true);
  EXPECT_TRUE(toolbar_model()->IsActionPinned(extension1->id()));
  EXPECT_FALSE(toolbar_model()->IsActionPinned(extension2->id()));
}

IN_PROC_BROWSER_TEST_F(ToolbarActionsModelBrowserTest, PinnedStateMetrics) {
  EXPECT_EQ(2u, toolbar_model()->action_ids().size());
  EXPECT_EQ(1u, toolbar_model()->pinned_action_ids().size());

  histogram_tester()->ExpectUniqueSample(
      "Extensions.Toolbar.PinnedExtensionPercentage3", 50, 1);
  histogram_tester()->ExpectUniqueSample(
      "Extensions.Toolbar.PinnedExtensionCount2", 1, 1);
}

// Test that a user's pinned extensions and ordering persist across sessions. We
// exercise this in a two-step browser test, which most closely reflects a
// "real world" multi-session scenario.
IN_PROC_BROWSER_TEST_F(ToolbarActionsModelBrowserTest,
                       PRE_PinnedStatePersistence) {
  const extensions::Extension* const extension1 =
      LoadExtension(test_data_dir_.AppendASCII(kExtension1Path));
  ASSERT_TRUE(extension1);

  const extensions::Extension* const extension2 =
      LoadExtension(test_data_dir_.AppendASCII(kExtension2Path));
  ASSERT_TRUE(extension2);

  const extensions::Extension* const extension3 =
      LoadExtension(test_data_dir_.AppendASCII(kExtension3Path));
  ASSERT_TRUE(extension3);

  EXPECT_THAT(toolbar_model()->action_ids(),
              ::testing::UnorderedElementsAre(
                  extension1->id(), extension2->id(), extension3->id()));
  EXPECT_THAT(toolbar_model()->pinned_action_ids(), ::testing::IsEmpty());

  // Pin extension 3, followed by 2. The pinned extensions should be in the
  // order 3, 2.
  // TODO(devlin): The order of ToolbarActionsModel::action_ids() is not updated
  // as a result of pinning, but is updated as a result of moving extensions
  // around. Since pinned extensions are now the sorted order (and the rest are
  // displayed in alphabetical order in the menu), we can likely get rid of
  // sorting in action_ids() entirely. We should at least be consistent.
  toolbar_model()->SetActionVisibility(extension3->id(), true);
  toolbar_model()->SetActionVisibility(extension2->id(), true);

  EXPECT_THAT(toolbar_model()->action_ids(),
              ::testing::UnorderedElementsAre(
                  extension1->id(), extension2->id(), extension3->id()));
  EXPECT_THAT(toolbar_model()->pinned_action_ids(),
              ::testing::ElementsAre(extension3->id(), extension2->id()));
}

IN_PROC_BROWSER_TEST_F(ToolbarActionsModelBrowserTest, PinnedStatePersistence) {
  // Re-look-up the extensions. (Note that we can't use ID, since unpacked
  // extensions' ids are based on their absolute file path.)
  extensions::ExtensionRegistry* const registry =
      extensions::ExtensionRegistry::Get(profile());
  auto get_extension_by_name =
      [registry](const char* name) -> const extensions::Extension* {
    for (const auto& extension : registry->enabled_extensions()) {
      if (extension->name() == name) {
        return extension.get();
      }
    }
    return nullptr;
  };

  const extensions::Extension* const extension1 =
      get_extension_by_name(kExtension1Name);
  ASSERT_TRUE(extension1);
  const extensions::Extension* const extension2 =
      get_extension_by_name(kExtension2Name);
  ASSERT_TRUE(extension2);
  const extensions::Extension* const extension3 =
      get_extension_by_name(kExtension3Name);
  ASSERT_TRUE(extension3);

  // Pin state should have been persisted across sessions.
  EXPECT_THAT(toolbar_model()->action_ids(),
              ::testing::UnorderedElementsAre(
                  extension1->id(), extension2->id(), extension3->id()));
  EXPECT_THAT(toolbar_model()->pinned_action_ids(),
              ::testing::ElementsAre(extension3->id(), extension2->id()));
}
