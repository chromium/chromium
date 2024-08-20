// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/tab_strip/tab_strip_page_handler.h"

#include <memory>
#include <optional>

#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui.h"
#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui_embedder.h"
#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui_layout.h"
#include "chrome/browser/ui/webui/webui_util_desktop.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "content/public/browser/web_ui.h"
#include "content/public/test/test_web_ui.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/point.h"

using testing::_;
using testing::AtLeast;
using testing::InSequence;
using testing::Truly;

namespace {

class TestTabStripPageHandler : public TabStripPageHandler {
 public:
  explicit TestTabStripPageHandler(
      mojo::PendingRemote<tab_strip::mojom::Page> page,
      content::WebUI* web_ui,
      Browser* browser,
      TabStripUIEmbedder* embedder)
      : TabStripPageHandler(
            mojo::PendingReceiver<tab_strip::mojom::PageHandler>(),
            std::move(page),
            web_ui,
            browser,
            embedder) {}
};

class StubTabStripUIEmbedder : public TabStripUIEmbedder {
 public:
  const ui::AcceleratorProvider* GetAcceleratorProvider() const override {
    return nullptr;
  }
  void CloseContainer() override {}
  void ShowContextMenuAtPoint(
      gfx::Point point,
      std::unique_ptr<ui::MenuModel> menu_model,
      base::RepeatingClosure on_menu_closed_callback) override {}
  void CloseContextMenu() override {}
  void ShowEditDialogForGroupAtPoint(gfx::Point point,
                                     gfx::Rect rect,
                                     tab_groups::TabGroupId group_id) override {
  }
  void HideEditDialogForGroup() override {}
  TabStripUILayout GetLayout() override { return TabStripUILayout(); }
  SkColor GetColorProviderColor(ui::ColorId id) const override {
    return SK_ColorWHITE;
  }
};

class MockPage : public tab_strip::mojom::Page {
 public:
  MockPage() = default;
  ~MockPage() override = default;

  mojo::PendingRemote<tab_strip::mojom::Page> BindAndGetRemote() {
    DCHECK(!receiver_.is_bound());
    return receiver_.BindNewPipeAndPassRemote();
  }
  mojo::Receiver<tab_strip::mojom::Page> receiver_{this};

  MOCK_METHOD1(LayoutChanged,
               void(const base::flat_map<std::string, std::string>& layout));
  MOCK_METHOD(void, ReceivedKeyboardFocus, ());
  MOCK_METHOD(void, ContextMenuClosed, ());
  MOCK_METHOD(void, LongPress, ());
  MOCK_METHOD(void,
              TabGroupVisualsChanged,
              (const std::string& group_id,
               tab_strip::mojom::TabGroupVisualDataPtr tab_group));
  MOCK_METHOD(void,
              TabGroupMoved,
              (const std::string& group_id, int32_t index));
  MOCK_METHOD(void, TabGroupClosed, (const std::string& group_id));
  MOCK_METHOD(void,
              TabGroupStateChanged,
              (int32_t tab_id,
               int32_t index,
               const std::optional<std::string>& group_id));
  MOCK_METHOD(void, TabCloseCancelled, (int32_t tab_id));
  MOCK_METHOD(void, TabCreated, (tab_strip::mojom::TabPtr tab));
  MOCK_METHOD(void, TabRemoved, (int32_t tab_id));
  MOCK_METHOD(void,
              TabMoved,
              (int32_t tab_id, int32_t to_index, bool in_pinned));
  MOCK_METHOD(void, TabReplaced, (int32_t tab_id, int32_t new_tab_id));
  MOCK_METHOD(void, TabActiveChanged, (int32_t tab_id));
  MOCK_METHOD(void, TabUpdated, (tab_strip::mojom::TabPtr tab));
  MOCK_METHOD(void,
              TabThumbnailUpdated,
              (int32_t tab_id, const std::string& data_uri));
  MOCK_METHOD(void, ShowContextMenu, ());
  MOCK_METHOD(void, ThemeChanged, ());
};

}  // namespace

class TabStripPageHandlerTest : public BrowserWithTestWindowTest {
 public:
  TabStripPageHandlerTest() = default;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(profile()));
    web_ui_.set_web_contents(web_contents_.get());
    handler_ = std::make_unique<TestTabStripPageHandler>(
        page_.BindAndGetRemote(), web_ui(), browser(), &stub_embedder_);
    web_ui()->ClearTrackedCalls();
  }
  void TearDown() override {
    web_contents_.reset();
    handler_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

  TabStripPageHandler* handler() { return handler_.get(); }
  content::TestWebUI* web_ui() { return &web_ui_; }

  void ExpectVisualData(const tab_groups::TabGroupVisualData& visual_data,
                        const tab_strip::mojom::TabGroupVisualData& tab_group) {
    EXPECT_EQ(base::UTF16ToASCII(visual_data.title()), tab_group.title);
    EXPECT_EQ(color_utils::SkColorToRgbString(SK_ColorWHITE), tab_group.color);
  }

 protected:
  MockPage page_;

 private:
  StubTabStripUIEmbedder stub_embedder_;
  std::unique_ptr<content::WebContents> web_contents_;
  content::TestWebUI web_ui_;
  std::unique_ptr<TestTabStripPageHandler> handler_;
};

TEST_F(TabStripPageHandlerTest, GroupClosedEvent) {
  AddTab(browser(), GURL("http://foo"));
  tab_groups::TabGroupId expected_group_id =
      browser()->tab_strip_model()->AddToNewGroup({0});
  browser()->tab_strip_model()->RemoveFromGroup({0});

  EXPECT_CALL(page_, TabGroupClosed(expected_group_id.ToString()));
}

TEST_F(TabStripPageHandlerTest, GroupStateChangedEvents) {
  AddTab(browser(), GURL("http://foo/1"));
  AddTab(browser(), GURL("http://foo/2"));

  // Add one of the tabs to a group to test for a tab-group-state-changed event.
  tab_groups::TabGroupId expected_group_id =
      browser()->tab_strip_model()->AddToNewGroup({0, 1});

  EXPECT_CALL(page_,
              TabGroupStateChanged(
                  extensions::ExtensionTabUtil::GetTabId(
                      browser()->tab_strip_model()->GetWebContentsAt(0)),
                  0, std::optional<std::string>(expected_group_id.ToString())));
  EXPECT_CALL(page_,
              TabGroupStateChanged(
                  extensions::ExtensionTabUtil::GetTabId(
                      browser()->tab_strip_model()->GetWebContentsAt(1)),
                  1, std::optional<std::string>(expected_group_id.ToString())));

  // Remove the tab from the group to test for a tab-group-state-changed event.
  browser()->tab_strip_model()->RemoveFromGroup({1});

  EXPECT_CALL(page_, TabGroupStateChanged(
                         extensions::ExtensionTabUtil::GetTabId(
                             browser()->tab_strip_model()->GetWebContentsAt(1)),
                         1, std::optional<std::string>()));
}

TEST_F(TabStripPageHandlerTest, GetGroupVisualData) {
  AddTab(browser(), GURL("http://foo/1"));
  AddTab(browser(), GURL("http://foo/2"));
  tab_groups::TabGroupId group1 =
      browser()->tab_strip_model()->AddToNewGroup({0});
  const tab_groups::TabGroupVisualData group1_visuals(
      u"Group 1", tab_groups::TabGroupColorId::kGreen);
  browser()
      ->tab_strip_model()
      ->group_model()
      ->GetTabGroup(group1)
      ->SetVisualData(group1_visuals);
  tab_groups::TabGroupId group2 =
      browser()->tab_strip_model()->AddToNewGroup({1});
  const tab_groups::TabGroupVisualData group2_visuals(
      u"Group 2", tab_groups::TabGroupColorId::kCyan);
  browser()
      ->tab_strip_model()
      ->group_model()
      ->GetTabGroup(group2)
      ->SetVisualData(group2_visuals);

  tab_strip::mojom::PageHandler::GetGroupVisualDataCallback callback =
      base::BindLambdaForTesting(
          [=, this](base::flat_map<std::string,
                                   tab_strip::mojom::TabGroupVisualDataPtr>
                        group_visual_datas) {
            ExpectVisualData(group1_visuals,
                             *group_visual_datas[group1.ToString()]);
            ExpectVisualData(group2_visuals,
                             *group_visual_datas[group2.ToString()]);
          });
  handler()->GetGroupVisualData(std::move(callback));
}

TEST_F(TabStripPageHandlerTest, GroupVisualDataChangedEvent) {
  AddTab(browser(), GURL("http://foo"));
  tab_groups::TabGroupId expected_group_id =
      browser()->tab_strip_model()->AddToNewGroup({0});
  const tab_groups::TabGroupVisualData new_visual_data(
      u"My new title", tab_groups::TabGroupColorId::kGreen);
  browser()
      ->tab_strip_model()
      ->group_model()
      ->GetTabGroup(expected_group_id)
      ->SetVisualData(new_visual_data);

  EXPECT_CALL(
      page_,
      TabGroupVisualsChanged(
          expected_group_id.ToString(),
          Truly(
              [=, this](
                  const tab_strip::mojom::TabGroupVisualDataPtr& visual_data) {
                if (visual_data->title.size() > 0) {
                  ExpectVisualData(new_visual_data, *visual_data);
                }
                return true;
              })))
      .Times(2);
}

TEST_F(TabStripPageHandlerTest, GroupTab) {
  // Add a tab inside of a group.
  AddTab(browser(), GURL("http://foo"));
  tab_groups::TabGroupId group_id =
      browser()->tab_strip_model()->AddToNewGroup({0});

  // Add another tab, and try to group it.
  AddTab(browser(), GURL("http://foo"));
  handler()->GroupTab(extensions::ExtensionTabUtil::GetTabId(
                          browser()->tab_strip_model()->GetWebContentsAt(0)),
                      group_id.ToString());

  ASSERT_EQ(group_id, browser()->tab_strip_model()->GetTabGroupForTab(0));
}

TEST_F(TabStripPageHandlerTest, MoveGroup) {
  AddTab(browser(), GURL("http://foo/1"));
  AddTab(browser(), GURL("http://foo/2"));

  auto* tab_strip_model = browser()->tab_strip_model();
  const int moved_tab_id = extensions::ExtensionTabUtil::GetTabId(
      tab_strip_model->GetWebContentsAt(0));
  tab_groups::TabGroupId group_id = tab_strip_model->AddToNewGroup({0});
  web_ui()->ClearTrackedCalls();

  // Move the group to index 1.
  constexpr int kMoveIndex = 1;
  handler()->MoveGroup(group_id.ToString(), kMoveIndex);

  gfx::Range tabs_in_group =
      tab_strip_model->group_model()->GetTabGroup(group_id)->ListTabs();
  ASSERT_EQ(kMoveIndex, static_cast<int>(tabs_in_group.start()));
  ASSERT_EQ(kMoveIndex, static_cast<int>(tabs_in_group.end()) - 1);

  EXPECT_CALL(page_, TabMoved(moved_tab_id, kMoveIndex, false));
  EXPECT_CALL(page_, TabGroupMoved(group_id.ToString(), kMoveIndex));
}

class MockTabStripModelObserver : public TabStripModelObserver {
 public:
  MOCK_METHOD(void,
              OnTabStripModelChanged,
              (TabStripModel*,
               const TabStripModelChange&,
               const TabStripSelectionChange&),
              (override));
  MOCK_METHOD(void, OnTabGroupChanged, (const TabGroupChange&), (override));
};

// Tests the event order from a multi-tab group move. The WebUI event handling
// implementation relies on this order of events. If it ever changes the WebUI
// implementation should also change.
TEST_F(TabStripPageHandlerTest, ValidateTabGroupEventStream) {
  MockTabStripModelObserver mock_observer_;
  auto* tab_strip_model = browser()->tab_strip_model();
  tab_strip_model->AddObserver(&mock_observer_);

  AddTab(browser(), GURL("http://foo/1"));
  AddTab(browser(), GURL("http://foo/2"));
  AddTab(browser(), GURL("http://foo/3"));
  AddTab(browser(), GURL("http://foo/4"));
  AddTab(browser(), GURL("http://foo/5"));

  content::WebContents* first_tab_in_group =
      tab_strip_model->GetWebContentsAt(0);
  content::WebContents* second_tab_in_group =
      tab_strip_model->GetWebContentsAt(1);
  content::WebContents* third_tab_in_group =
      tab_strip_model->GetWebContentsAt(2);

  // Group tabs {0, 1, 2} together.
  std::vector<int> tab_group_indicies = {0, 1, 2};
  tab_groups::TabGroupId group_id =
      tab_strip_model->AddToNewGroup(tab_group_indicies);

  // Moving tabs {0, 1, 2} to index 2 will result in the first tab in the group
  // being at index 2 after the move. This is how the index is calculdated,
  // however, we process the group move operation one tab at a time. So if we
  // want to move a group to the end of this particular array the to_index will
  // be (length of tabstrip - 1). Ex:
  // Indices:  0 1 2   3 4
  // Before: { 0 1 2 } 3 4
  // Indices:  0 1 2
  // Middle:   3 4 (Specifying 2 puts the group at the end)
  // Indices:  0 1   2 3 4
  // After:    3 4 { 0 1 2 }
  constexpr int kMoveIndex = 2;
  constexpr int kNewGroupStartIndex = 2;
  {
    InSequence s;
    EXPECT_CALL(mock_observer_,
                OnTabStripModelChanged(
                    _, Truly([&](const TabStripModelChange& change) {
                      return change.type() == TabStripModelChange::kMoved;
                    }),
                    _))
        .Times(3);
    EXPECT_CALL(
        mock_observer_,
        OnTabGroupChanged(Truly([&](const TabGroupChange& change) {
          TabGroupModel* group_model = tab_strip_model->group_model();
          const int start_tab =
              group_model->GetTabGroup(change.group)->ListTabs().start();
          return change.type == TabGroupChange::kMoved &&
                 change.group == group_id && start_tab == kNewGroupStartIndex;
        })));
  }
  tab_strip_model->MoveGroupTo(group_id, kMoveIndex);
  ASSERT_EQ(first_tab_in_group,
            browser()->tab_strip_model()->GetWebContentsAt(2));
  ASSERT_EQ(second_tab_in_group,
            browser()->tab_strip_model()->GetWebContentsAt(3));
  ASSERT_EQ(third_tab_in_group,
            browser()->tab_strip_model()->GetWebContentsAt(4));
}

TEST_F(TabStripPageHandlerTest, MoveGroupAcrossWindows) {
  AddTab(browser(), GURL("http://foo"));

  // Create a new window with the same profile, and add a group to it.
  std::unique_ptr<BrowserWindow> new_window(CreateBrowserWindow());
  std::unique_ptr<Browser> new_browser =
      CreateBrowser(profile(), browser()->type(), false, new_window.get());
  AddTab(new_browser.get(), GURL("http://foo"));
  AddTab(new_browser.get(), GURL("http://foo"));
  tab_groups::TabGroupId group_id =
      new_browser.get()->tab_strip_model()->AddToNewGroup({0, 1});

  // Create some visual data to make sure it gets transferred.
  const tab_groups::TabGroupVisualData visual_data(
      u"My group", tab_groups::TabGroupColorId::kGreen);
  new_browser.get()
      ->tab_strip_model()
      ->group_model()
      ->GetTabGroup(group_id)
      ->SetVisualData(visual_data);

  content::WebContents* moved_contents1 =
      new_browser.get()->tab_strip_model()->GetWebContentsAt(0);
  content::WebContents* moved_contents2 =
      new_browser.get()->tab_strip_model()->GetWebContentsAt(1);
  web_ui()->ClearTrackedCalls();

  int new_index = -1;
  handler()->MoveGroup(group_id.ToString(), new_index);

  ASSERT_EQ(0U, new_browser.get()
                    ->tab_strip_model()
                    ->group_model()
                    ->ListTabGroups()
                    .size());
  ASSERT_EQ(moved_contents1, browser()->tab_strip_model()->GetWebContentsAt(1));
  ASSERT_EQ(moved_contents2, browser()->tab_strip_model()->GetWebContentsAt(2));

  std::optional<tab_groups::TabGroupId> new_group_id =
      browser()->tab_strip_model()->GetTabGroupForTab(1);
  ASSERT_TRUE(new_group_id.has_value());
  ASSERT_EQ(browser()->tab_strip_model()->GetTabGroupForTab(1),
            browser()->tab_strip_model()->GetTabGroupForTab(2));

  const tab_groups::TabGroupVisualData* new_visual_data =
      browser()
          ->tab_strip_model()
          ->group_model()
          ->GetTabGroup(new_group_id.value())
          ->visual_data();
  ASSERT_EQ(visual_data.title(), new_visual_data->title());
  ASSERT_EQ(visual_data.color(), new_visual_data->color());
}

TEST_F(TabStripPageHandlerTest, MoveGroupAcrossProfiles) {
  AddTab(browser(), GURL("http://foo"));

  TestingProfile* different_profile =
      profile_manager()->CreateTestingProfile("different_profile");
  std::unique_ptr<BrowserWindow> new_window(CreateBrowserWindow());
  std::unique_ptr<Browser> new_browser = CreateBrowser(
      different_profile, browser()->type(), false, new_window.get());
  AddTab(new_browser.get(), GURL("http://foo"));
  tab_groups::TabGroupId group_id =
      new_browser.get()->tab_strip_model()->AddToNewGroup({0});

  int new_index = -1;
  handler()->MoveGroup(group_id.ToString(), new_index);

  ASSERT_TRUE(
      new_browser.get()->tab_strip_model()->group_model()->ContainsTabGroup(
          group_id));

  // Close all tabs before destructing.
  new_browser.get()->tab_strip_model()->CloseAllTabs();
}

TEST_F(TabStripPageHandlerTest, MoveTab) {
  AddTab(browser(), GURL("http://foo"));
  AddTab(browser(), GURL("http://foo"));

  content::WebContents* contents_prev_at_0 =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  content::WebContents* contents_prev_at_1 =
      browser()->tab_strip_model()->GetWebContentsAt(1);

  // Move tab at index 0 to index 1.
  handler()->MoveTab(extensions::ExtensionTabUtil::GetTabId(contents_prev_at_0),
                     1);

  ASSERT_EQ(1, browser()->tab_strip_model()->GetIndexOfWebContents(
                   contents_prev_at_0));
  ASSERT_EQ(0, browser()->tab_strip_model()->GetIndexOfWebContents(
                   contents_prev_at_1));
}

TEST_F(TabStripPageHandlerTest, MoveTabAcrossProfiles) {
  AddTab(browser(), GURL("http://foo"));

  TestingProfile* different_profile =
      profile_manager()->CreateTestingProfile("different_profile");
  std::unique_ptr<BrowserWindow> new_window(CreateBrowserWindow());
  std::unique_ptr<Browser> new_browser = CreateBrowser(
      different_profile, browser()->type(), false, new_window.get());
  AddTab(new_browser.get(), GURL("http://foo"));

  handler()->MoveTab(extensions::ExtensionTabUtil::GetTabId(
                         new_browser->tab_strip_model()->GetWebContentsAt(0)),
                     1);

  ASSERT_FALSE(browser()->tab_strip_model()->ContainsIndex(1));

  // Close all tabs before destructing.
  new_browser.get()->tab_strip_model()->CloseAllTabs();
}

TEST_F(TabStripPageHandlerTest, MoveTabAcrossWindows) {
  AddTab(browser(), GURL("http://foo"));

  std::unique_ptr<BrowserWindow> new_window(CreateBrowserWindow());
  std::unique_ptr<Browser> new_browser =
      CreateBrowser(profile(), browser()->type(), false, new_window.get());
  AddTab(new_browser.get(), GURL("http://foo"));
  content::WebContents* moved_contents =
      new_browser.get()->tab_strip_model()->GetWebContentsAt(0);

  handler()->MoveTab(extensions::ExtensionTabUtil::GetTabId(
                         new_browser->tab_strip_model()->GetWebContentsAt(0)),
                     1);

  ASSERT_EQ(moved_contents, browser()->tab_strip_model()->GetWebContentsAt(1));

  // Close all tabs before destructing.
  new_browser.get()->tab_strip_model()->CloseAllTabs();
}

TEST_F(TabStripPageHandlerTest, TabCreated) {
  AddTab(browser(), GURL("http://foo"));

  int expected_tab_id = extensions::ExtensionTabUtil::GetTabId(
      browser()->tab_strip_model()->GetWebContentsAt(0));
  EXPECT_CALL(page_, TabCreated(Truly([=](const tab_strip::mojom::TabPtr& tab) {
                return tab->id =
                           expected_tab_id && tab->active && tab->index == 0;
              })));
}

TEST_F(TabStripPageHandlerTest, TabRemoved) {
  // Two tabs so the browser does not close when a tab is closed.
  AddTab(browser(), GURL("http://foo"));
  AddTab(browser(), GURL("http://foo"));
  int expected_tab_id = extensions::ExtensionTabUtil::GetTabId(
      browser()->tab_strip_model()->GetWebContentsAt(0));
  browser()->tab_strip_model()->GetWebContentsAt(0)->Close();
  EXPECT_CALL(page_, TabRemoved(expected_tab_id));
}

TEST_F(TabStripPageHandlerTest, TabMoved) {
  AddTab(browser(), GURL("http://foo"));
  AddTab(browser(), GURL("http://foo"));

  int from_index = 0;
  int expected_to_index = 1;
  int expected_tab_id = extensions::ExtensionTabUtil::GetTabId(
      browser()->tab_strip_model()->GetWebContentsAt(from_index));

  browser()->tab_strip_model()->MoveWebContentsAt(from_index, expected_to_index,
                                                  false);

  EXPECT_CALL(page_, TabMoved(expected_tab_id, expected_to_index, false));
}

TEST_F(TabStripPageHandlerTest, TabMovedAndPinned) {
  AddTab(browser(), GURL("http://foo"));
  AddTab(browser(), GURL("http://foo"));
  web_ui()->ClearTrackedCalls();

  int from_index = 1;
  int expected_to_index = 0;
  int expected_tab_id = extensions::ExtensionTabUtil::GetTabId(
      browser()->tab_strip_model()->GetWebContentsAt(from_index));

  browser()->tab_strip_model()->SetTabPinned(from_index, true);

  EXPECT_CALL(page_, TabMoved(expected_tab_id, expected_to_index, true));
  EXPECT_CALL(page_, TabUpdated(_)).Times(AtLeast(1));
  EXPECT_CALL(page_, TabUpdated(Truly([=](const tab_strip::mojom::TabPtr& tab) {
                return tab->pinned;
              })));
}

TEST_F(TabStripPageHandlerTest, TabReplaced) {
  AddTab(browser(), GURL("http://foo"));
  int expected_previous_id = extensions::ExtensionTabUtil::GetTabId(
      browser()->tab_strip_model()->GetWebContentsAt(0));

  web_ui()->ClearTrackedCalls();
  browser()->tab_strip_model()->DiscardWebContentsAt(
      0, content::WebContentsTester::CreateTestWebContents(profile(), nullptr));
  int expected_new_id = extensions::ExtensionTabUtil::GetTabId(
      browser()->tab_strip_model()->GetWebContentsAt(0));

  EXPECT_CALL(page_, TabReplaced(expected_previous_id, expected_new_id));
}

TEST_F(TabStripPageHandlerTest, TabActivated) {
  AddTab(browser(), GURL("http://foo"));
  AddTab(browser(), GURL("http://foo"));
  AddTab(browser(), GURL("http://foo"));

  web_ui()->ClearTrackedCalls();
  browser()->tab_strip_model()->ActivateTabAt(1);

  EXPECT_CALL(page_, TabActiveChanged(extensions::ExtensionTabUtil::GetTabId(
                         browser()->tab_strip_model()->GetWebContentsAt(0))));
  EXPECT_CALL(page_, TabActiveChanged(extensions::ExtensionTabUtil::GetTabId(
                         browser()->tab_strip_model()->GetWebContentsAt(1))))
      .Times(2);
  EXPECT_CALL(page_, TabActiveChanged(extensions::ExtensionTabUtil::GetTabId(
                         browser()->tab_strip_model()->GetWebContentsAt(2))));
}

TEST_F(TabStripPageHandlerTest, SwitchTab) {
  AddTab(browser(), GURL("http://foo"));
  AddTab(browser(), GURL("http://foo"));
  AddTab(browser(), GURL("http://foo"));

  web_ui()->ClearTrackedCalls();
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  ASSERT_EQ(tab_strip_model->GetActiveWebContents(),
            tab_strip_model->GetWebContentsAt(0));
  int tab_id = extensions::ExtensionTabUtil::GetTabId(
      browser()->tab_strip_model()->GetWebContentsAt(1));
  handler()->ActivateTab(tab_id);
  ASSERT_EQ(tab_strip_model->GetActiveWebContents(),
            tab_strip_model->GetWebContentsAt(1));
}

TEST_F(TabStripPageHandlerTest, UngroupTab) {
  // Add a tab inside of a group.
  AddTab(browser(), GURL("http://foo"));
  browser()->tab_strip_model()->AddToNewGroup({0});

  // Add another tab at index 1, and try to group it.
  handler()->UngroupTab(extensions::ExtensionTabUtil::GetTabId(
      browser()->tab_strip_model()->GetWebContentsAt(0)));

  ASSERT_FALSE(browser()->tab_strip_model()->GetTabGroupForTab(0).has_value());
}

TEST_F(TabStripPageHandlerTest, CloseTab) {
  AddTab(browser(), GURL("http://foo"));
  AddTab(browser(), GURL("http://bar"));

  handler()->CloseTab(extensions::ExtensionTabUtil::GetTabId(
                          browser()->tab_strip_model()->GetWebContentsAt(0)),
                      false /* closed_by_swiped */);

  ASSERT_EQ(1, browser()->tab_strip_model()->GetTabCount());
}

TEST_F(TabStripPageHandlerTest, RemoveTabIfInvalidContextMenu) {
  AddTab(browser(), GURL("http://foo"));

  std::unique_ptr<BrowserWindow> new_window(CreateBrowserWindow());
  std::unique_ptr<Browser> new_browser =
      CreateBrowser(profile(), browser()->type(), false, new_window.get());
  AddTab(new_browser.get(), GURL("http://bar"));

  web_ui()->ClearTrackedCalls();

  handler()->ShowTabContextMenu(
      extensions::ExtensionTabUtil::GetTabId(
          new_browser->tab_strip_model()->GetWebContentsAt(0)),
      50.0, 100.0);

  EXPECT_CALL(page_, TabRemoved(extensions::ExtensionTabUtil::GetTabId(
                         new_browser->tab_strip_model()->GetWebContentsAt(0))));

  new_browser.get()->tab_strip_model()->CloseAllTabs();
}

TEST_F(TabStripPageHandlerTest, PreventsInvalidTabDrags) {
  content::DropData empty_drop_data;
  EXPECT_FALSE(handler()->CanDragEnter(nullptr, empty_drop_data,
                                       blink::kDragOperationMove));

  content::DropData invalid_drop_data;
  invalid_drop_data.custom_data.insert({kWebUITabIdDataType, u"3000"});
  EXPECT_FALSE(handler()->CanDragEnter(nullptr, invalid_drop_data,
                                       blink::kDragOperationMove));

  AddTab(browser(), GURL("http://foo"));
  int valid_tab_id = extensions::ExtensionTabUtil::GetTabId(
      browser()->tab_strip_model()->GetWebContentsAt(0));
  content::DropData valid_drop_data;
  valid_drop_data.custom_data.insert(
      {kWebUITabIdDataType, base::NumberToString16(valid_tab_id)});
  EXPECT_TRUE(handler()->CanDragEnter(nullptr, valid_drop_data,
                                      blink::kDragOperationMove));
}

TEST_F(TabStripPageHandlerTest, PreventsInvalidGroupDrags) {
  content::DropData invalid_drop_data;
  invalid_drop_data.custom_data.insert(
      {kWebUITabGroupIdDataType, u"not a real group"});
  EXPECT_FALSE(handler()->CanDragEnter(nullptr, invalid_drop_data,
                                       blink::kDragOperationMove));

  AddTab(browser(), GURL("http://foo"));
  tab_groups::TabGroupId group_id =
      browser()->tab_strip_model()->AddToNewGroup({0});
  content::DropData valid_drop_data;
  valid_drop_data.custom_data.insert(
      {kWebUITabGroupIdDataType, base::ASCIIToUTF16(group_id.ToString())});
  EXPECT_TRUE(handler()->CanDragEnter(nullptr, valid_drop_data,
                                      blink::kDragOperationMove));

  // Another group from a different profile.
  std::unique_ptr<BrowserWindow> new_window(
      std::make_unique<TestBrowserWindow>());
  std::unique_ptr<Browser> new_browser = CreateBrowser(
      browser()->profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true),
      browser()->type(), false, new_window.get());
  AddTab(new_browser.get(), GURL("http://foo"));

  tab_groups::TabGroupId new_group_id =
      new_browser.get()->tab_strip_model()->AddToNewGroup({0});
  content::DropData different_profile_drop_data;
  different_profile_drop_data.custom_data.insert(
      {kWebUITabGroupIdDataType, base::ASCIIToUTF16(new_group_id.ToString())});
  EXPECT_FALSE(handler()->CanDragEnter(nullptr, different_profile_drop_data,
                                       blink::kDragOperationMove));

  // Close all tabs before destructing.
  new_browser.get()->tab_strip_model()->CloseAllTabs();
}

TEST_F(TabStripPageHandlerTest, OnThemeChanged) {
  webui::GetNativeThemeDeprecated(web_ui()->GetWebContents())
      ->NotifyOnNativeThemeUpdated();
  EXPECT_CALL(page_, ThemeChanged());
}
