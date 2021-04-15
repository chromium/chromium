// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui_handler.h"

#include <memory>

#include "base/macros.h"
#include "base/optional.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui_embedder.h"
#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui_layout.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "content/public/browser/web_ui.h"
#include "content/public/test/test_web_ui.h"
#include "content/public/test/web_contents_tester.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/point.h"

namespace {

class TestTabStripUIHandler : public TabStripUIHandler {
 public:
  explicit TestTabStripUIHandler(content::WebUI* web_ui,
                                 Browser* browser,
                                 TabStripUIEmbedder* embedder)
      : TabStripUIHandler(browser, embedder) {
    set_web_ui(web_ui);
  }
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
  TabStripUILayout GetLayout() override { return TabStripUILayout(); }
  SkColor GetColor(int id) const override { return SK_ColorWHITE; }
  SkColor GetSystemColor(ui::NativeTheme::ColorId id) const override {
    return SK_ColorWHITE;
  }
};

}  // namespace

class TabStripUIHandlerTest : public BrowserWithTestWindowTest {
 public:
  TabStripUIHandlerTest() = default;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(profile()));
    web_ui_.set_web_contents(web_contents_.get());
    handler_ = std::make_unique<TestTabStripUIHandler>(web_ui(), browser(),
                                                       &stub_embedder_);
    handler()->AllowJavascriptForTesting();
    web_ui()->ClearTrackedCalls();
  }
  void TearDown() override {
    web_contents_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

  TabStripUIHandler* handler() { return handler_.get(); }
  content::TestWebUI* web_ui() { return &web_ui_; }

  void ExpectVisualDataDictionary(
      const tab_groups::TabGroupVisualData visual_data,
      const base::DictionaryValue* visual_data_dict) {
    std::string group_title;
    ASSERT_TRUE(visual_data_dict->GetString("title", &group_title));
    EXPECT_EQ(base::UTF16ToASCII(visual_data.title()), group_title);

    std::string group_color;
    ASSERT_TRUE(visual_data_dict->GetString("color", &group_color));
    EXPECT_EQ(color_utils::SkColorToRgbString(SK_ColorWHITE), group_color);
  }

 private:
  StubTabStripUIEmbedder stub_embedder_;
  std::unique_ptr<content::WebContents> web_contents_;
  content::TestWebUI web_ui_;
  std::unique_ptr<TestTabStripUIHandler> handler_;
};

TEST_F(TabStripUIHandlerTest, GroupClosedEvent) {
  AddTab(browser(), GURL("http://foo"));
  tab_groups::TabGroupId expected_group_id =
      browser()->tab_strip_model()->AddToNewGroup({0});
  browser()->tab_strip_model()->RemoveFromGroup({0});

  const content::TestWebUI::CallData& call_data = *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIListenerCallback", call_data.function_name());
  EXPECT_EQ("tab-group-closed", call_data.arg1()->GetString());
  EXPECT_EQ(expected_group_id.ToString(), call_data.arg2()->GetString());
}

TEST_F(TabStripUIHandlerTest, GroupStateChangedEvents) {
  AddTab(browser(), GURL("http://foo/1"));
  AddTab(browser(), GURL("http://foo/2"));

  // Add one of the tabs to a group to test for a tab-group-state-changed event.
  tab_groups::TabGroupId expected_group_id =
      browser()->tab_strip_model()->AddToNewGroup({0, 1});
  int expected_tab_id = extensions::ExtensionTabUtil::GetTabId(
      browser()->tab_strip_model()->GetWebContentsAt(1));

  const content::TestWebUI::CallData& grouped_call_data =
      *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIListenerCallback", grouped_call_data.function_name());
  EXPECT_EQ("tab-group-state-changed", grouped_call_data.arg1()->GetString());
  EXPECT_EQ(expected_tab_id, grouped_call_data.arg2()->GetInt());
  EXPECT_EQ(1, grouped_call_data.arg3()->GetInt());
  EXPECT_EQ(expected_group_id.ToString(),
            grouped_call_data.arg4()->GetString());

  // Remove the tab from the group to test for a tab-group-state-changed event.
  browser()->tab_strip_model()->RemoveFromGroup({1});
  const content::TestWebUI::CallData& ungrouped_call_data =
      *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIListenerCallback", ungrouped_call_data.function_name());
  EXPECT_EQ("tab-group-state-changed", ungrouped_call_data.arg1()->GetString());
  EXPECT_EQ(expected_tab_id, ungrouped_call_data.arg2()->GetInt());
  EXPECT_EQ(1, ungrouped_call_data.arg3()->GetInt());
  EXPECT_EQ(nullptr, ungrouped_call_data.arg4());
}

TEST_F(TabStripUIHandlerTest, GetGroupVisualData) {
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

  base::ListValue args;
  args.AppendString("callback-id");
  handler()->HandleGetGroupVisualData(&args);

  const content::TestWebUI::CallData& call_data = *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIResponse", call_data.function_name());
  EXPECT_EQ("callback-id", call_data.arg1()->GetString());
  EXPECT_TRUE(call_data.arg2()->GetBool());

  const base::DictionaryValue* returned_data;
  ASSERT_TRUE(call_data.arg3()->GetAsDictionary(&returned_data));

  const base::DictionaryValue* group1_dict;
  ASSERT_TRUE(returned_data->GetDictionary(group1.ToString(), &group1_dict));
  ExpectVisualDataDictionary(group1_visuals, group1_dict);

  const base::DictionaryValue* group2_dict;
  ASSERT_TRUE(returned_data->GetDictionary(group2.ToString(), &group2_dict));
  ExpectVisualDataDictionary(group2_visuals, group2_dict);
}

TEST_F(TabStripUIHandlerTest, GroupVisualDataChangedEvent) {
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

  const content::TestWebUI::CallData& call_data = *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIListenerCallback", call_data.function_name());
  EXPECT_EQ("tab-group-visuals-changed", call_data.arg1()->GetString());
  EXPECT_EQ(expected_group_id.ToString(), call_data.arg2()->GetString());

  const base::DictionaryValue* visual_data;
  ASSERT_TRUE(call_data.arg3()->GetAsDictionary(&visual_data));
  ExpectVisualDataDictionary(new_visual_data, visual_data);
}

TEST_F(TabStripUIHandlerTest, GroupTab) {
  // Add a tab inside of a group.
  AddTab(browser(), GURL("http://foo"));
  tab_groups::TabGroupId group_id =
      browser()->tab_strip_model()->AddToNewGroup({0});

  // Add another tab, and try to group it.
  AddTab(browser(), GURL("http://foo"));
  base::ListValue args;
  args.AppendInteger(extensions::ExtensionTabUtil::GetTabId(
      browser()->tab_strip_model()->GetWebContentsAt(0)));
  args.AppendString(group_id.ToString());
  handler()->HandleGroupTab(&args);

  ASSERT_EQ(group_id, browser()->tab_strip_model()->GetTabGroupForTab(0));
}

TEST_F(TabStripUIHandlerTest, MoveGroup) {
  AddTab(browser(), GURL("http://foo/1"));
  AddTab(browser(), GURL("http://foo/2"));
  tab_groups::TabGroupId group_id =
      browser()->tab_strip_model()->AddToNewGroup({0});
  web_ui()->ClearTrackedCalls();

  // Move the group to index 1.
  int new_index = 1;
  base::ListValue args;
  args.AppendString(group_id.ToString());
  args.AppendInteger(new_index);
  handler()->HandleMoveGroup(&args);

  gfx::Range tabs_in_group = browser()
                                 ->tab_strip_model()
                                 ->group_model()
                                 ->GetTabGroup(group_id)
                                 ->ListTabs();
  ASSERT_EQ(new_index, static_cast<int>(tabs_in_group.start()));
  ASSERT_EQ(new_index, static_cast<int>(tabs_in_group.end()) - 1);

  EXPECT_EQ(1U, web_ui()->call_data().size());
  const content::TestWebUI::CallData& call_data =
      *web_ui()->call_data().front();
  EXPECT_EQ("cr.webUIListenerCallback", call_data.function_name());
  EXPECT_EQ("tab-group-moved", call_data.arg1()->GetString());
  EXPECT_EQ(group_id.ToString(), call_data.arg2()->GetString());
}

TEST_F(TabStripUIHandlerTest, MoveGroupAcrossWindows) {
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
  base::ListValue args;
  args.AppendString(group_id.ToString());
  args.AppendInteger(new_index);
  handler()->HandleMoveGroup(&args);

  ASSERT_EQ(0U, new_browser.get()
                    ->tab_strip_model()
                    ->group_model()
                    ->ListTabGroups()
                    .size());
  ASSERT_EQ(moved_contents1, browser()->tab_strip_model()->GetWebContentsAt(1));
  ASSERT_EQ(moved_contents2, browser()->tab_strip_model()->GetWebContentsAt(2));

  base::Optional<tab_groups::TabGroupId> new_group_id =
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

TEST_F(TabStripUIHandlerTest, MoveGroupAcrossProfiles) {
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
  base::ListValue args;
  args.AppendString(group_id.ToString());
  args.AppendInteger(new_index);
  handler()->HandleMoveGroup(&args);

  ASSERT_TRUE(
      new_browser.get()->tab_strip_model()->group_model()->ContainsTabGroup(
          group_id));

  // Close all tabs before destructing.
  new_browser.get()->tab_strip_model()->CloseAllTabs();
}

TEST_F(TabStripUIHandlerTest, MoveTab) {
  AddTab(browser(), GURL("http://foo"));
  AddTab(browser(), GURL("http://foo"));

  content::WebContents* contents_prev_at_0 =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  content::WebContents* contents_prev_at_1 =
      browser()->tab_strip_model()->GetWebContentsAt(1);

  // Move tab at index 0 to index 1.
  base::ListValue args;
  args.AppendInteger(
      extensions::ExtensionTabUtil::GetTabId(contents_prev_at_0));
  args.AppendInteger(1);
  handler()->HandleMoveTab(&args);

  ASSERT_EQ(1, browser()->tab_strip_model()->GetIndexOfWebContents(
                   contents_prev_at_0));
  ASSERT_EQ(0, browser()->tab_strip_model()->GetIndexOfWebContents(
                   contents_prev_at_1));
}

TEST_F(TabStripUIHandlerTest, MoveTabAcrossProfiles) {
  AddTab(browser(), GURL("http://foo"));

  TestingProfile* different_profile =
      profile_manager()->CreateTestingProfile("different_profile");
  std::unique_ptr<BrowserWindow> new_window(CreateBrowserWindow());
  std::unique_ptr<Browser> new_browser = CreateBrowser(
      different_profile, browser()->type(), false, new_window.get());
  AddTab(new_browser.get(), GURL("http://foo"));

  base::ListValue args;
  args.AppendInteger(extensions::ExtensionTabUtil::GetTabId(
      new_browser->tab_strip_model()->GetWebContentsAt(0)));
  args.AppendInteger(1);
  handler()->HandleMoveTab(&args);

  ASSERT_FALSE(browser()->tab_strip_model()->ContainsIndex(1));

  // Close all tabs before destructing.
  new_browser.get()->tab_strip_model()->CloseAllTabs();
}

TEST_F(TabStripUIHandlerTest, MoveTabAcrossWindows) {
  AddTab(browser(), GURL("http://foo"));

  std::unique_ptr<BrowserWindow> new_window(CreateBrowserWindow());
  std::unique_ptr<Browser> new_browser =
      CreateBrowser(profile(), browser()->type(), false, new_window.get());
  AddTab(new_browser.get(), GURL("http://foo"));
  content::WebContents* moved_contents =
      new_browser.get()->tab_strip_model()->GetWebContentsAt(0);

  base::ListValue args;
  args.AppendInteger(extensions::ExtensionTabUtil::GetTabId(
      new_browser->tab_strip_model()->GetWebContentsAt(0)));
  args.AppendInteger(1);
  handler()->HandleMoveTab(&args);

  ASSERT_EQ(moved_contents, browser()->tab_strip_model()->GetWebContentsAt(1));

  // Close all tabs before destructing.
  new_browser.get()->tab_strip_model()->CloseAllTabs();
}

TEST_F(TabStripUIHandlerTest, TabCreated) {
  AddTab(browser(), GURL("http://foo"));

  const content::TestWebUI::CallData& call_data =
      *web_ui()->call_data().front();
  EXPECT_EQ("cr.webUIListenerCallback", call_data.function_name());
  EXPECT_EQ("tab-created", call_data.arg1()->GetString());

  const base::DictionaryValue* tab_data;
  ASSERT_TRUE(call_data.arg2()->GetAsDictionary(&tab_data));

  int tab_id;
  ASSERT_TRUE(tab_data->GetInteger("id", &tab_id));
  ASSERT_EQ(extensions::ExtensionTabUtil::GetTabId(
                browser()->tab_strip_model()->GetWebContentsAt(0)),
            tab_id);

  bool is_active;
  ASSERT_TRUE(tab_data->GetBoolean("active", &is_active));
  ASSERT_TRUE(is_active);

  int tab_index;
  ASSERT_TRUE(tab_data->GetInteger("index", &tab_index));
  ASSERT_EQ(0, tab_index);
}

TEST_F(TabStripUIHandlerTest, TabRemoved) {
  // Two tabs so the browser does not close when a tab is closed.
  AddTab(browser(), GURL("http://foo"));
  AddTab(browser(), GURL("http://foo"));
  int expected_tab_id = extensions::ExtensionTabUtil::GetTabId(
      browser()->tab_strip_model()->GetWebContentsAt(0));

  web_ui()->ClearTrackedCalls();
  browser()->tab_strip_model()->GetWebContentsAt(0)->Close();

  const content::TestWebUI::CallData& call_data =
      *web_ui()->call_data().front();
  EXPECT_EQ("cr.webUIListenerCallback", call_data.function_name());
  EXPECT_EQ("tab-removed", call_data.arg1()->GetString());
  EXPECT_EQ(expected_tab_id, call_data.arg2()->GetInt());
}

TEST_F(TabStripUIHandlerTest, TabMoved) {
  AddTab(browser(), GURL("http://foo"));
  AddTab(browser(), GURL("http://foo"));

  int from_index = 0;
  int expected_to_index = 1;
  int expected_tab_id = extensions::ExtensionTabUtil::GetTabId(
      browser()->tab_strip_model()->GetWebContentsAt(from_index));

  browser()->tab_strip_model()->MoveWebContentsAt(from_index, expected_to_index,
                                                  false);

  const content::TestWebUI::CallData& call_data = *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIListenerCallback", call_data.function_name());
  EXPECT_EQ("tab-moved", call_data.arg1()->GetString());
  EXPECT_EQ(expected_tab_id, call_data.arg2()->GetInt());
  EXPECT_EQ(expected_to_index, call_data.arg3()->GetInt());
  EXPECT_EQ(false, call_data.arg4()->GetBool());
}

TEST_F(TabStripUIHandlerTest, TabMovedAndPinned) {
  AddTab(browser(), GURL("http://foo"));
  AddTab(browser(), GURL("http://foo"));
  web_ui()->ClearTrackedCalls();

  int from_index = 1;
  int expected_to_index = 0;
  int expected_tab_id = extensions::ExtensionTabUtil::GetTabId(
      browser()->tab_strip_model()->GetWebContentsAt(from_index));

  browser()->tab_strip_model()->SetTabPinned(from_index, true);

  const content::TestWebUI::CallData& moved_event =
      *web_ui()->call_data().front();
  EXPECT_EQ("cr.webUIListenerCallback", moved_event.function_name());
  EXPECT_EQ("tab-moved", moved_event.arg1()->GetString());
  EXPECT_EQ(expected_tab_id, moved_event.arg2()->GetInt());
  EXPECT_EQ(expected_to_index, moved_event.arg3()->GetInt());
  EXPECT_EQ(true, moved_event.arg4()->GetBool());

  const content::TestWebUI::CallData& updated_event =
      *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIListenerCallback", updated_event.function_name());
  EXPECT_EQ("tab-updated", updated_event.arg1()->GetString());
  const base::DictionaryValue* updated_data;
  ASSERT_TRUE(updated_event.arg2()->GetAsDictionary(&updated_data));
  bool pinned;
  ASSERT_TRUE(updated_data->GetBoolean("pinned", &pinned));
  ASSERT_TRUE(pinned);
}

TEST_F(TabStripUIHandlerTest, TabReplaced) {
  AddTab(browser(), GURL("http://foo"));
  int expected_previous_id = extensions::ExtensionTabUtil::GetTabId(
      browser()->tab_strip_model()->GetWebContentsAt(0));

  web_ui()->ClearTrackedCalls();
  browser()->tab_strip_model()->ReplaceWebContentsAt(
      0, content::WebContentsTester::CreateTestWebContents(profile(), nullptr));
  int expected_new_id = extensions::ExtensionTabUtil::GetTabId(
      browser()->tab_strip_model()->GetWebContentsAt(0));

  const content::TestWebUI::CallData& call_data =
      *web_ui()->call_data().front();
  EXPECT_EQ("cr.webUIListenerCallback", call_data.function_name());
  EXPECT_EQ("tab-replaced", call_data.arg1()->GetString());
  ASSERT_EQ(expected_previous_id, call_data.arg2()->GetInt());
  ASSERT_EQ(expected_new_id, call_data.arg3()->GetInt());
}

TEST_F(TabStripUIHandlerTest, TabActivated) {
  AddTab(browser(), GURL("http://foo"));
  AddTab(browser(), GURL("http://foo"));
  AddTab(browser(), GURL("http://foo"));

  web_ui()->ClearTrackedCalls();
  browser()->tab_strip_model()->ActivateTabAt(1);

  const content::TestWebUI::CallData& call_data = *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIListenerCallback", call_data.function_name());
  EXPECT_EQ("tab-active-changed", call_data.arg1()->GetString());
  EXPECT_EQ(extensions::ExtensionTabUtil::GetTabId(
                browser()->tab_strip_model()->GetWebContentsAt(1)),
            call_data.arg2()->GetInt());
}

TEST_F(TabStripUIHandlerTest, UngroupTab) {
  // Add a tab inside of a group.
  AddTab(browser(), GURL("http://foo"));
  browser()->tab_strip_model()->AddToNewGroup({0});

  // Add another tab at index 1, and try to group it.
  base::ListValue args;
  args.AppendInteger(extensions::ExtensionTabUtil::GetTabId(
      browser()->tab_strip_model()->GetWebContentsAt(0)));
  handler()->HandleUngroupTab(&args);

  ASSERT_FALSE(browser()->tab_strip_model()->GetTabGroupForTab(0).has_value());
}

TEST_F(TabStripUIHandlerTest, CloseTab) {
  AddTab(browser(), GURL("http://foo"));
  AddTab(browser(), GURL("http://bar"));

  base::ListValue args;
  args.AppendInteger(extensions::ExtensionTabUtil::GetTabId(
      browser()->tab_strip_model()->GetWebContentsAt(0)));
  args.AppendBoolean(false);  // If the tab is closed by swipe.
  handler()->HandleCloseTab(&args);

  ASSERT_EQ(1, browser()->tab_strip_model()->GetTabCount());
}

TEST_F(TabStripUIHandlerTest, RemoveTabIfInvalidContextMenu) {
  AddTab(browser(), GURL("http://foo"));

  std::unique_ptr<BrowserWindow> new_window(CreateBrowserWindow());
  std::unique_ptr<Browser> new_browser =
      CreateBrowser(profile(), browser()->type(), false, new_window.get());
  AddTab(new_browser.get(), GURL("http://bar"));

  web_ui()->ClearTrackedCalls();

  base::ListValue args;
  args.AppendInteger(extensions::ExtensionTabUtil::GetTabId(
      new_browser->tab_strip_model()->GetWebContentsAt(0)));
  args.AppendDouble(50);
  args.AppendDouble(100);
  handler()->HandleShowTabContextMenu(&args);

  const content::TestWebUI::CallData& call_data = *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIListenerCallback", call_data.function_name());
  EXPECT_EQ("tab-removed", call_data.arg1()->GetString());
  EXPECT_EQ(extensions::ExtensionTabUtil::GetTabId(
                new_browser->tab_strip_model()->GetWebContentsAt(0)),
            call_data.arg2()->GetInt());

  // Close all tabs before destructing.
  new_browser.get()->tab_strip_model()->CloseAllTabs();
}
