// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/toolbar_actions_bar_unittest.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_action_test_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/load_error_reporter.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar_delegate.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/extension.h"
#include "extensions/test/test_extension_dir.h"
#include "ui/base/test/material_design_controller_test_api.h"

namespace {

using ActionType = extensions::ExtensionBuilder::ActionType;

const extensions::Extension* GetExtensionByPath(
    const extensions::ExtensionSet& extensions,
    const base::FilePath& path) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath extension_path = base::MakeAbsoluteFilePath(path);
  EXPECT_TRUE(!extension_path.empty());
  for (const scoped_refptr<const extensions::Extension>& extension :
       extensions) {
    if (extension->path() == extension_path) {
      return extension.get();
    }
  }
  return nullptr;
}

// Verifies that the toolbar order matches for the given |actions_bar|. If the
// order matches, the return value is empty; otherwise, it contains the error.
std::string VerifyToolbarOrderForBar(
    const ToolbarActionsBar* actions_bar,
    BrowserActionTestUtil* browser_action_test_util,
    const char* expected_names[],
    size_t total_size,
    size_t visible_count) {
  const ToolbarActionsBar::ToolbarActions& toolbar_actions =
      actions_bar->toolbar_actions_unordered();
  // If the total size is wrong, we risk segfaulting by continuing. Abort now.
  if (total_size != toolbar_actions.size()) {
    return base::StringPrintf("Incorrect action count: expected %d, found %d",
                              static_cast<int>(total_size),
                              static_cast<int>(toolbar_actions.size()));
  }

  // Check that the ToolbarActionsBar matches the expected state.
  std::string error;
  for (size_t i = 0; i < total_size; ++i) {
    if (std::string(expected_names[i]) !=
            base::UTF16ToUTF8(toolbar_actions[i]->GetActionName())) {
      error += base::StringPrintf(
          "Incorrect action in bar at index %d: expected '%s', found '%s'.\n",
          static_cast<int>(i),
          expected_names[i],
          base::UTF16ToUTF8(toolbar_actions[i]->GetActionName()).c_str());
    }
  }
  size_t icon_count = actions_bar->GetIconCount();
  if (visible_count != icon_count)
    error += base::StringPrintf(
        "Incorrect visible count: expected %d, found %d.\n",
        static_cast<int>(visible_count), static_cast<int>(icon_count));

  // Test that the (platform-specific) toolbar view matches the expected state.
  for (size_t i = 0; i < total_size; ++i) {
    std::string id = browser_action_test_util->GetExtensionId(i);
    if (id != toolbar_actions[i]->GetId()) {
      error += base::StringPrintf(
          "Incorrect action in view at index %d: expected '%s', found '%s'.\n",
          static_cast<int>(i),
          toolbar_actions[i]->GetId().c_str(),
          id.c_str());
    }
  }
  size_t view_icon_count = browser_action_test_util->VisibleBrowserActions();
  if (visible_count != view_icon_count)
    error += base::StringPrintf(
        "Incorrect visible count in view: expected %d, found %d.\n",
        static_cast<int>(visible_count), static_cast<int>(view_icon_count));

  return error;
}

// The ToolbarActionErrorTestObserver is used to notify when an extension
// failed to load.
class ToolbarActionErrorTestObserver
    : public extensions::LoadErrorReporter::Observer {
 public:
  ToolbarActionErrorTestObserver() : extension_error_reporter_observer_(this) {
    extension_error_reporter_observer_.Add(
        extensions::LoadErrorReporter::GetInstance());
  }

  ~ToolbarActionErrorTestObserver() override {}

  void WaitForOnLoadFailure() { run_loop_.Run(); }

 private:
  // extensions::LoadErrorReporter::Observer:
  void OnLoadFailure(content::BrowserContext* browser_context,
                     const base::FilePath& extension_path,
                     const std::string& error) override {
    run_loop_.Quit();
  }

  base::RunLoop run_loop_;

  ScopedObserver<extensions::LoadErrorReporter,
                 extensions::LoadErrorReporter::Observer>
      extension_error_reporter_observer_;

  DISALLOW_COPY_AND_ASSIGN(ToolbarActionErrorTestObserver);
};

}  // namespace

ToolbarActionsBarUnitTest::ToolbarActionsBarUnitTest()
    : toolbar_model_(nullptr) {}

ToolbarActionsBarUnitTest::~ToolbarActionsBarUnitTest() {}

void ToolbarActionsBarUnitTest::SetUp() {
  // Overriding MD state needs to be done before setting up the test window to
  // maintain consistency throughout its lifetime.
  material_design_state_ =
      std::make_unique<ui::test::MaterialDesignControllerTestAPI>(GetParam());
  BrowserWithTestWindowTest::SetUp();
  extensions::LoadErrorReporter::Init(false);

  // The toolbar typically displays extension icons, so create some extension
  // test infrastructure.
  extensions::TestExtensionSystem* extension_system =
      static_cast<extensions::TestExtensionSystem*>(
          extensions::ExtensionSystem::Get(profile()));
  extension_system->CreateExtensionService(
      base::CommandLine::ForCurrentProcess(),
      base::FilePath(),
      false);
  toolbar_model_ =
      extensions::extension_action_test_util::CreateToolbarModelForProfile(
          profile());

  ToolbarActionsBar::disable_animations_for_testing_ = true;
  browser_action_test_util_ = BrowserActionTestUtil::Create(browser(), false);

  overflow_browser_action_test_util_ =
      browser_action_test_util_->CreateOverflowBar(browser());
}

void ToolbarActionsBarUnitTest::TearDown() {
  // Since the profile gets destroyed in BrowserWithTestWindowTest::TearDown(),
  // we need to delete this now.
  browser_action_test_util_.reset();
  overflow_browser_action_test_util_.reset();
  ToolbarActionsBar::disable_animations_for_testing_ = false;
  BrowserWithTestWindowTest::TearDown();
  material_design_state_.reset();
}

void ToolbarActionsBarUnitTest::ActivateTab(int index) {
  ASSERT_NE(nullptr, browser()->tab_strip_model()->GetWebContentsAt(index));
  browser()->tab_strip_model()->ActivateTabAt(
      index, {TabStripModel::GestureType::kOther});
}

scoped_refptr<const extensions::Extension>
ToolbarActionsBarUnitTest::CreateAndAddExtension(const std::string& name,
                                                 ActionType action_type) {
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder(name)
          .SetAction(action_type)
          .SetLocation(extensions::Manifest::INTERNAL)
          .Build();
  extensions::ExtensionSystem::Get(profile())->extension_service()->
      AddExtension(extension.get());
  return extension;
}

void ToolbarActionsBarUnitTest::SetActionWantsToRunOnTab(
    ExtensionAction* action,
    content::WebContents* web_contents,
    bool wants_to_run) {
  action->SetIsVisible(SessionTabHelper::IdForTab(web_contents).id(),
                       wants_to_run);
  extensions::ExtensionActionAPI::Get(profile())->NotifyChange(
      action, web_contents, profile());
}

testing::AssertionResult ToolbarActionsBarUnitTest::VerifyToolbarOrder(
    const char* expected_names[],
    size_t total_size,
    size_t visible_count) {
  std::string main_bar_error =
      VerifyToolbarOrderForBar(toolbar_actions_bar(),
                               browser_action_test_util(),
                               expected_names,
                               total_size,
                               visible_count);
  std::string overflow_bar_error;
  overflow_bar_error = VerifyToolbarOrderForBar(
      overflow_bar(), overflow_browser_action_test_util(), expected_names,
      total_size, total_size - visible_count);

  return main_bar_error.empty() && overflow_bar_error.empty() ?
      testing::AssertionSuccess() :
      testing::AssertionFailure() << "main bar error:\n" << main_bar_error <<
          "overflow bar error:\n" << overflow_bar_error;
}

// Note: First argument is optional and intentionally left blank.
// (it's a prefix for the generated test cases)
INSTANTIATE_TEST_SUITE_P(,
                         ToolbarActionsBarUnitTest,
                         testing::Values(false, true));

TEST_P(ToolbarActionsBarUnitTest, BasicToolbarActionsBarTest) {
  // Add three extensions to the profile; this is the easiest way to have
  // toolbar actions.
  for (int i = 0; i < 3; ++i) {
    CreateAndAddExtension(base::StringPrintf("extension %d", i),
                          ActionType::BROWSER_ACTION);
  }

  const ToolbarActionsBar::PlatformSettings& platform_settings =
      toolbar_actions_bar()->platform_settings();

  // By default, all three actions should be visible.
  EXPECT_EQ(3u, toolbar_actions_bar()->GetIconCount());
  const gfx::Size view_size = toolbar_actions_bar()->GetViewSize();
  // Check the widths.
  int expected_width =
      3 * view_size.width() + 2 * GetLayoutConstant(TOOLBAR_ELEMENT_PADDING);
  EXPECT_EQ(expected_width, toolbar_actions_bar()->GetFullSize().width());
  // Since all icons are showing, the current width should be the max width.
  int maximum_width = expected_width;
  EXPECT_EQ(maximum_width, toolbar_actions_bar()->GetMaximumWidth());
  // The minimum width should be non-zero (as long as there are any icons) so
  // we can render the grippy to allow the user to drag to adjust the width.
  int minimum_width = platform_settings.item_spacing;
  EXPECT_EQ(minimum_width, toolbar_actions_bar()->GetMinimumWidth());

  // Test the connection between the ToolbarActionsBar and the model by
  // adjusting the visible count.
  toolbar_model()->SetVisibleIconCount(2u);
  EXPECT_EQ(2u, toolbar_actions_bar()->GetIconCount());

  // The current width should now be enough for two icons.
  expected_width =
      2 * view_size.width() + GetLayoutConstant(TOOLBAR_ELEMENT_PADDING);
  EXPECT_EQ(expected_width, toolbar_actions_bar()->GetFullSize().width());
  // The maximum and minimum widths should have remained constant (since we have
  // the same number of actions).
  EXPECT_EQ(maximum_width, toolbar_actions_bar()->GetMaximumWidth());
  EXPECT_EQ(minimum_width, toolbar_actions_bar()->GetMinimumWidth());

  // Test drag-and-drop logic.
  const char kExtension0[] = "extension 0";
  const char kExtension1[] = "extension 1";
  const char kExtension2[] = "extension 2";

  {
    // The order should start as 0, 1, 2.
    const char* expected_names[] = { kExtension0, kExtension1, kExtension2 };
    EXPECT_TRUE(VerifyToolbarOrder(expected_names, 3u, 2u));
  }

  {
    // Drag 0 to be in the second spot; 1, 0, 2, within the same container.
    toolbar_actions_bar()->OnDragDrop(0, 1, ToolbarActionsBar::DRAG_TO_SAME);
    const char* expected_names[] = { kExtension1, kExtension0, kExtension2 };
    EXPECT_TRUE(VerifyToolbarOrder(expected_names, 3u, 2u));
  }

  {
    // Drag 0 to be in the third spot, in the overflow container.
    // Order should be 1, 2, 0, and the icon count should reduce by 1.
    toolbar_actions_bar()->OnDragDrop(
        1, 2, ToolbarActionsBar::DRAG_TO_OVERFLOW);
    const char* expected_names[] = { kExtension1, kExtension2, kExtension0 };
    EXPECT_TRUE(VerifyToolbarOrder(expected_names, 3u, 1u));
    // The model should also reflect the updated icon count.
    EXPECT_EQ(1u, toolbar_model()->visible_icon_count());
    // Dragging 2 to the main container should work, even if its spot in the
    // "list" remains constant.
    // Order remains 1, 2, 0, but now we have 2 icons visible.
    toolbar_actions_bar()->OnDragDrop(1, 1, ToolbarActionsBar::DRAG_TO_MAIN);
    EXPECT_TRUE(VerifyToolbarOrder(expected_names, 3u, 2u));
    // Similarly, dragging 2 to overflow, with the same "list" spot, should also
    // work. Order remains 1, 2, 0, but icon count goes back to 1.
    toolbar_actions_bar()->OnDragDrop(
        1, 1, ToolbarActionsBar::DRAG_TO_OVERFLOW);
    EXPECT_TRUE(VerifyToolbarOrder(expected_names, 3u, 1u));
  }

  // Try resizing the toolbar. Start with the current width (enough for 1 icon).
  int width = toolbar_actions_bar()->GetFullSize().width();

  // If we try to resize by increasing, without allowing enough room for a new
  // icon, width, and icon count should stay the same.
  toolbar_actions_bar()->OnResizeComplete(width + 1);
  EXPECT_EQ(width, toolbar_actions_bar()->GetFullSize().width());
  EXPECT_EQ(1u, toolbar_actions_bar()->GetIconCount());

  // If we resize by enough to include a new icon, width and icon count should
  // both increase.
  width += view_size.width() + GetLayoutConstant(TOOLBAR_ELEMENT_PADDING);
  toolbar_actions_bar()->OnResizeComplete(width);
  EXPECT_EQ(width, toolbar_actions_bar()->GetFullSize().width());
  EXPECT_EQ(2u, toolbar_actions_bar()->GetIconCount());

  // If we shrink the bar so that a full icon can't fit, it should resize to
  // hide that icon.
  toolbar_actions_bar()->OnResizeComplete(width - 1);
  width -= view_size.width() + GetLayoutConstant(TOOLBAR_ELEMENT_PADDING);
  EXPECT_EQ(width, toolbar_actions_bar()->GetFullSize().width());
  EXPECT_EQ(1u, toolbar_actions_bar()->GetIconCount());
}

TEST_P(ToolbarActionsBarUnitTest, ToolbarActionsReorderOnPrefChange) {
  for (int i = 0; i < 3; ++i) {
    CreateAndAddExtension(base::StringPrintf("extension %d", i),
                          ActionType::BROWSER_ACTION);
  }
  EXPECT_EQ(3u, toolbar_actions_bar()->GetIconCount());
  // Change the value of the toolbar preference.
  // Test drag-and-drop logic.
  const char kExtension0[] = "extension 0";
  const char kExtension1[] = "extension 1";
  const char kExtension2[] = "extension 2";
  {
    // The order should start as 0, 1, 2.
    const char* expected_names[] = { kExtension0, kExtension1, kExtension2 };
    EXPECT_TRUE(VerifyToolbarOrder(expected_names, 3u, 3u));
  }

  std::vector<std::string> new_order;
  new_order.push_back(toolbar_actions_bar()->toolbar_actions_unordered()[1]->
      GetId());
  new_order.push_back(toolbar_actions_bar()->toolbar_actions_unordered()[2]->
      GetId());
  extensions::ExtensionPrefs::Get(profile())->SetToolbarOrder(new_order);

  {
    // The order should now reflect the prefs, and be 1, 2, 0.
    const char* expected_names[] = { kExtension1, kExtension2, kExtension0 };
    EXPECT_TRUE(VerifyToolbarOrder(expected_names, 3u, 3u));
  }
}

TEST_P(ToolbarActionsBarUnitTest, TestHighlightMode) {
  std::vector<std::string> ids;
  for (int i = 0; i < 3; ++i) {
    ids.push_back(CreateAndAddExtension(base::StringPrintf("extension %d", i),
                                        ActionType::BROWSER_ACTION)
                      ->id());
  }
  EXPECT_EQ(3u, toolbar_actions_bar()->GetIconCount());
  const char kExtension0[] = "extension 0";
  const char kExtension1[] = "extension 1";
  const char kExtension2[] = "extension 2";

  {
    // The order should start as 0, 1, 2.
    const char* expected_names[] = {kExtension0, kExtension1, kExtension2};
    EXPECT_TRUE(VerifyToolbarOrder(expected_names, 3u, 3u));
  }

  std::vector<std::string> ids_to_highlight;
  ids_to_highlight.push_back(ids[0]);
  ids_to_highlight.push_back(ids[2]);
  toolbar_model()->HighlightActions(ids_to_highlight,
                                    ToolbarActionsModel::HIGHLIGHT_WARNING);

  {
    // The order should now be 0, 2, since 1 is not being highlighted.
    const char* expected_names[] = {kExtension0, kExtension2};
    EXPECT_TRUE(VerifyToolbarOrder(expected_names, 2u, 2u));
  }

  toolbar_model()->StopHighlighting();

  {
    // The order should go back to normal.
    const char* expected_names[] = {kExtension0, kExtension1, kExtension2};
    EXPECT_TRUE(VerifyToolbarOrder(expected_names, 3u, 3u));
  }

  ids_to_highlight.push_back(ids[1]);
  toolbar_model()->HighlightActions(ids_to_highlight,
                                    ToolbarActionsModel::HIGHLIGHT_WARNING);
  {
    // All actions should be highlighted (in the order of the vector passed in,
    // so with '1' at the end).
    const char* expected_names[] = {kExtension0, kExtension2, kExtension1};
    EXPECT_TRUE(VerifyToolbarOrder(expected_names, 3u, 3u));
  }

  ids_to_highlight.clear();
  ids_to_highlight.push_back(ids[1]);
  toolbar_model()->HighlightActions(ids_to_highlight,
                                    ToolbarActionsModel::HIGHLIGHT_WARNING);

  {
    // Only extension 1 should be visible.
    const char* expected_names[] = {kExtension1};
    EXPECT_TRUE(VerifyToolbarOrder(expected_names, 1u, 1u));
  }

  toolbar_model()->StopHighlighting();
  {
    // And, again, back to normal.
    const char* expected_names[] = {kExtension0, kExtension1, kExtension2};
    EXPECT_TRUE(VerifyToolbarOrder(expected_names, 3u, 3u));
  }
}

// Test the bounds calculation for different indices.
TEST_P(ToolbarActionsBarUnitTest, TestActionFrameBounds) {
  const auto size = toolbar_actions_bar()->GetViewSize();
  const int element_padding = GetLayoutConstant(TOOLBAR_ELEMENT_PADDING);
  const auto icon_rect = [size, element_padding](int x, int y) {
    return gfx::Rect(gfx::Point(x * (element_padding + size.width()),
                                y * (element_padding + size.height())),
                     size);
  };

  constexpr int kIconsPerOverflowRow = 3;
  constexpr int kNumExtensions = 7;

  // Initialization: 7 total extensions, with 3 visible per row in overflow.
  // Start with all visible on the main bar.
  for (int i = 0; i < kNumExtensions; ++i) {
    CreateAndAddExtension(base::StringPrintf("extension %d", i),
                          ActionType::BROWSER_ACTION);
  }
  toolbar_model()->SetVisibleIconCount(kNumExtensions);
  const int overflow_width =
      kIconsPerOverflowRow *
          (toolbar_actions_bar()->GetViewSize().width() + element_padding) -
      element_padding;
  overflow_bar()->SetOverflowRowWidth(overflow_width);
  EXPECT_EQ(kIconsPerOverflowRow,
            overflow_bar()->platform_settings().icons_per_overflow_menu_row);

  // Check main bar calculations. Actions should be laid out in a line, so
  // all on the same (0) y-axis.
  EXPECT_EQ(icon_rect(0, 0), toolbar_actions_bar()->GetFrameForIndex(0));
  EXPECT_EQ(icon_rect(1, 0), toolbar_actions_bar()->GetFrameForIndex(1));
  EXPECT_EQ(icon_rect(kNumExtensions - 1, 0),
            toolbar_actions_bar()->GetFrameForIndex(kNumExtensions - 1));

  // Check overflow bar calculations.
  toolbar_model()->SetVisibleIconCount(3);
  // Any actions that are shown on the main bar should have an empty rect for
  // the frame.
  EXPECT_EQ(gfx::Rect(), overflow_bar()->GetFrameForIndex(0));
  EXPECT_EQ(gfx::Rect(), overflow_bar()->GetFrameForIndex(2));

  // Other actions should start from their relative index; that is, the first
  // action shown should be in the first spot's bounds, even though it's the
  // third action by index.
  EXPECT_EQ(icon_rect(0, 0), overflow_bar()->GetFrameForIndex(3));
  EXPECT_EQ(icon_rect(1, 0), overflow_bar()->GetFrameForIndex(4));
  EXPECT_EQ(icon_rect(2, 0), overflow_bar()->GetFrameForIndex(5));
  // And the actions should wrap, so that it starts back at the left on a new
  // row.
  EXPECT_EQ(icon_rect(0, 1), overflow_bar()->GetFrameForIndex(6));

  // Check with > 2 rows.
  toolbar_model()->SetVisibleIconCount(0);
  EXPECT_EQ(icon_rect(0, 0), overflow_bar()->GetFrameForIndex(0));
  EXPECT_EQ(icon_rect(0, 2), overflow_bar()->GetFrameForIndex(6));
}

TEST_P(ToolbarActionsBarUnitTest, TestStartAndEndIndexes) {
  const int icon_width = toolbar_actions_bar()->GetViewSize().width();
  // Width of the resize handle.
  int extra_area_width =
      toolbar_actions_bar()->platform_settings().item_spacing +
      2 * GetLayoutConstant(TOOLBAR_STANDARD_SPACING) + 1;

  for (int i = 0; i < 3; ++i) {
    CreateAndAddExtension(base::StringPrintf("extension %d", i),
                          ActionType::BROWSER_ACTION);
  }
  // At the start, all icons should be present on the main bar, and no
  // overflow should be needed.
  EXPECT_EQ(3u, toolbar_actions_bar()->GetIconCount());
  EXPECT_EQ(0u, toolbar_actions_bar()->GetStartIndexInBounds());
  EXPECT_EQ(3u, toolbar_actions_bar()->GetEndIndexInBounds());
  EXPECT_EQ(3u, overflow_bar()->GetStartIndexInBounds());
  EXPECT_EQ(3u, overflow_bar()->GetEndIndexInBounds());
  EXPECT_FALSE(toolbar_actions_bar()->NeedsOverflow());

  // Shrink the width of the view to be a little over enough for one icon.
  browser_action_test_util()->SetWidth(icon_width + 2 + extra_area_width);
  // Tricky: GetIconCount() is what we use to determine our preferred size,
  // stored pref size, etc, and should not be affected by a minimum size that is
  // too small to show everything. It should remain constant.
  EXPECT_EQ(3u, toolbar_actions_bar()->GetIconCount());
  // The main container should display only the first icon, with the overflow
  // displaying the rest.
  EXPECT_EQ(0u, toolbar_actions_bar()->GetStartIndexInBounds());
  EXPECT_EQ(1u, toolbar_actions_bar()->GetEndIndexInBounds());
  EXPECT_EQ(1u, overflow_bar()->GetStartIndexInBounds());
  EXPECT_EQ(3u, overflow_bar()->GetEndIndexInBounds());
  EXPECT_TRUE(toolbar_actions_bar()->NeedsOverflow());

  // Shrink the container again to be too small to display even one icon.
  // The overflow container should be displaying everything.
  browser_action_test_util()->SetWidth(icon_width - 10);
  EXPECT_EQ(3u, toolbar_actions_bar()->GetIconCount());
  EXPECT_EQ(0u, toolbar_actions_bar()->GetStartIndexInBounds());
  EXPECT_EQ(0u, toolbar_actions_bar()->GetEndIndexInBounds());
  EXPECT_EQ(0u, overflow_bar()->GetStartIndexInBounds());
  EXPECT_EQ(3u, overflow_bar()->GetEndIndexInBounds());
  EXPECT_TRUE(toolbar_actions_bar()->NeedsOverflow());

  // Set the width back to the preferred width (with resize handle). All should
  // be back to normal.
  // TODO(pbos): Set the full width in a less contrived way when
  // ToolbarActionsBar and BrowserActionsContainer merge.
  browser_action_test_util()->SetWidth(
      toolbar_actions_bar()->GetFullSize().width() + extra_area_width);
  EXPECT_EQ(3u, toolbar_actions_bar()->GetIconCount());
  EXPECT_EQ(0u, toolbar_actions_bar()->GetStartIndexInBounds());
  EXPECT_EQ(3u, toolbar_actions_bar()->GetEndIndexInBounds());
  EXPECT_EQ(3u, overflow_bar()->GetStartIndexInBounds());
  EXPECT_EQ(3u, overflow_bar()->GetEndIndexInBounds());
  EXPECT_FALSE(toolbar_actions_bar()->NeedsOverflow());
}

// Tests the logic for determining if the container needs an overflow menu item.
TEST_P(ToolbarActionsBarUnitTest, TestNeedsOverflow) {
  CreateAndAddExtension("extension 1", ActionType::BROWSER_ACTION);
  // One extension on the main bar, none overflowed. Overflow not needed.
  EXPECT_EQ(1u, toolbar_actions_bar()->GetIconCount());
  EXPECT_EQ(0u, overflow_bar()->GetIconCount());
  EXPECT_FALSE(toolbar_actions_bar()->NeedsOverflow());

  // Set one extension in the overflow menu, none on the main bar. Overflow
  // needed.
  toolbar_model()->SetVisibleIconCount(0u);
  EXPECT_EQ(0u, toolbar_actions_bar()->GetIconCount());
  EXPECT_EQ(1u, overflow_bar()->GetIconCount());
  EXPECT_TRUE(toolbar_actions_bar()->NeedsOverflow());

  // Pop out an extension for a non-sticky popup. Even though the extension is
  // on the main bar, overflow is still needed because it will pop back in
  // when the menu is opened.
  ToolbarActionViewController* action = toolbar_actions_bar()->GetActions()[0];
  {
    base::RunLoop run_loop;
    toolbar_actions_bar()->PopOutAction(action, false, run_loop.QuitClosure());
    run_loop.Run();
  }
  EXPECT_EQ(1u, toolbar_actions_bar()->GetIconCount());
  EXPECT_TRUE(toolbar_actions_bar()->NeedsOverflow());

  // Back to one in overflow, none on the main bar.
  toolbar_actions_bar()->UndoPopOut();
  EXPECT_EQ(0u, toolbar_actions_bar()->GetIconCount());
  EXPECT_EQ(1u, overflow_bar()->GetIconCount());
  EXPECT_TRUE(toolbar_actions_bar()->NeedsOverflow());

  // Pop out an extension for a sticky popup. Overflow shouldn't be needed now
  // because the extension will remain popped out even when the menu opens.
  {
    base::RunLoop run_loop;
    toolbar_actions_bar()->PopOutAction(action, true, run_loop.QuitClosure());
    run_loop.Run();
  }
  EXPECT_EQ(1u, toolbar_actions_bar()->GetIconCount());
  EXPECT_FALSE(toolbar_actions_bar()->NeedsOverflow());

  // Add another extension and verify that if one is still in overflow when
  // another is popped out, we still need overflow.
  toolbar_actions_bar()->UndoPopOut();
  CreateAndAddExtension("extension 2", ActionType::BROWSER_ACTION);
  toolbar_model()->SetVisibleIconCount(0u);
  {
    base::RunLoop run_loop;
    toolbar_actions_bar()->PopOutAction(action, true, run_loop.QuitClosure());
    run_loop.Run();
  }
  EXPECT_EQ(1u, toolbar_actions_bar()->GetIconCount());
  EXPECT_TRUE(toolbar_actions_bar()->NeedsOverflow());
}

// Tests that both the extension icon and its allocated slot in the toolbar are
// removed when an extension is reloaded with manifest errors and therefore
// fails to be loaded into Chrome,
//
// TODO(catmullings): Convert this from TEST_P to TEST_F since there is no test
// parameter dependence.
TEST_P(ToolbarActionsBarUnitTest, ReuploadExtensionFailed) {
  extensions::ExtensionService* service =
      extensions::ExtensionSystem::Get(profile())->extension_service();
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile());

  extensions::TestExtensionDir ext_dir;
  const char kManifest[] =
      "{"
      "  'name': 'Test',"
      "  'version': '1',"
      "  'manifest_version': 2"
      "}";
  ext_dir.WriteManifestWithSingleQuotes(kManifest);

  scoped_refptr<extensions::UnpackedInstaller> installer =
      extensions::UnpackedInstaller::Create(service);
  installer->Load(ext_dir.UnpackedPath());
  content::RunAllTasksUntilIdle();

  EXPECT_EQ(1u, registry->enabled_extensions().size());
  EXPECT_EQ(0u, registry->disabled_extensions().size());

  const extensions::Extension* extension = GetExtensionByPath(
      registry->enabled_extensions(), ext_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  // Ensure that the toolbar has the 1 icon for the extension loaded.
  int width = toolbar_actions_bar()->GetFullSize().width();
  EXPECT_EQ(width, toolbar_actions_bar()->GetFullSize().width());
  EXPECT_EQ(1u, toolbar_actions_bar()->GetIconCount());

  // Reload the extension again.
  extensions::TestExtensionRegistryObserver registry_observer(registry);
  service->ReloadExtension(extension->id());
  ASSERT_TRUE(registry_observer.WaitForExtensionLoaded());

  // Ensure that after reload, the toolbar still contains the 1 icon for the
  // extension.
  EXPECT_EQ(width, toolbar_actions_bar()->GetFullSize().width());
  EXPECT_EQ(1u, toolbar_actions_bar()->GetIconCount());

  // Replace the extension's valid manifest with one containing errors. In this
  // case, the error is that both the 'browser_action' and 'page_action' keys
  // are specified instead of only one.
  const char kManifestWithErrors[] =
      "{"
      "  'name': 'Test',"
      "  'version': '1',"
      "  'manifest_version': 2,"
      "  'page_action' : {},"
      "  'browser_action' : {}"
      "}";
  ext_dir.WriteManifestWithSingleQuotes(kManifestWithErrors);

  // Reload the extension again. Check that the updated extension cannot be
  // loaded due to the manifest errors.
  service->ReloadExtensionWithQuietFailure(extension->id());
  base::RunLoop().RunUntilIdle();

  // Since the extension is removed, its icon should no longer be in the
  // toolbar.
  EXPECT_EQ(0, toolbar_actions_bar()->GetFullSize().width());
  EXPECT_EQ(0u, toolbar_actions_bar()->GetIconCount());
}
