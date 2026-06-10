// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/vertical_tab_strip_region_view.h"
#include "chrome/browser/ui/views/tabs/vertical/root_tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_group_header_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_view.h"
#include "chrome/browser/ui/views/test/vertical_tabs_interactive_test_mixin.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "ui/base/ozone_buildflags.h"
#include "ui/base/test/ui_controls.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/interaction/interactive_views_test.h"
#include "ui/views/interaction/mouse/interaction_test_util_mouse.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/view.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace {

enum class DragPosition { kAbove, kBelow };

using URL = std::string_view;
using TabGroupURLs = std::vector<URL>;
using URLs = std::vector<std::variant<URL, TabGroupURLs>>;
using PinnedURLs = std::vector<URL>;

int TabSelectModifier() {
#if BUILDFLAG(IS_MAC)
  return ui_controls::kCommand;
#else
  return ui_controls::kControl;
#endif
}

// Returns a collection of URLs that correspond to the order of tabs.
// Tab groups are added to a vector nested within the collection.
// E.g., {A, {B, C}, D}
base::RepeatingCallback<URLs()> GetTabOrder(TabStripModel* model) {
  return base::BindRepeating(
      [](TabStripModel* model) {
        URLs urls;
        std::optional<tab_groups::TabGroupId> current_group_id;
        for (auto i = 0; i < model->count(); ++i) {
          tabs::TabInterface* tab = model->GetTabAtIndex(i);
          URL url = tab->GetContents()->GetURL().spec();
          auto group_id = tab->GetGroup();
          if (group_id.has_value()) {
            if (group_id == current_group_id) {
              std::get<TabGroupURLs>(urls.back()).push_back(url);
            } else {
              urls.push_back(TabGroupURLs{url});
            }
            current_group_id = *group_id;
          } else {
            urls.push_back(url);
            current_group_id = std::nullopt;
          }
        }
        return urls;
      },
      model);
}

base::RepeatingCallback<PinnedURLs()> GetPinnedTabOrder(TabStripModel* model) {
  return base::BindRepeating(
      [](TabStripModel* model) {
        PinnedURLs urls;
        for (auto i = 0; i < model->count(); ++i) {
          tabs::TabInterface* tab = model->GetTabAtIndex(i);
          if (!tab->IsPinned()) {
            break;
          }
          urls.push_back(tab->GetContents()->GetURL().spec());
        }
        return urls;
      },
      model);
}

base::RepeatingCallback<size_t()> GetBrowserCount() {
  return base::BindRepeating(
      []() { return GlobalBrowserCollection::GetInstance()->GetSize(); });
}

base::RepeatingCallback<bool()> GetDragActive() {
  return base::BindRepeating([]() { return TabDragController::IsActive(); });
}

class WidgetVisibilityWaiter : public views::WidgetObserver {
 public:
  WidgetVisibilityWaiter(views::Widget* widget, base::RunLoop& run_loop)
      : run_loop_(run_loop) {
    obs_.Observe(widget);
  }
  void OnWidgetVisibilityChanged(views::Widget* widget, bool visible) override {
    if (visible) {
      run_loop_->Quit();
    }
  }
  void OnWidgetDestroying(views::Widget* widget) override {
    obs_.Reset();
    run_loop_->Quit();
  }

 private:
  const raw_ref<base::RunLoop> run_loop_;
  base::ScopedObservation<views::Widget, views::WidgetObserver> obs_{this};
};

}  // namespace

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTab);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kThirdTab);
DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<size_t>,
                                    kBrowserCountPoller);
DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<bool>,
                                    kDragStatePoller);
DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<URLs>,
                                    kTabOrderPoller);
DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<PinnedURLs>,
                                    kPinnedTabOrderPoller);
DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<int>,
                                    kScrollOffsetPoller);

class VerticalTabDragTest
    : public VerticalTabsInteractiveTestMixin<InteractiveBrowserTest> {
 public:
  VerticalTabDragTest() = default;
  ~VerticalTabDragTest() override = default;

  const std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures()
      override {
    auto enabled = VerticalTabsInteractiveTestMixin<
        InteractiveBrowserTest>::GetEnabledFeatures();
    enabled.push_back({features::kCollapseTabGroupDuringDrag, {}});
    return enabled;
  }

 protected:
  auto RunScheduledLayout() {
    return Do([&]() { views::test::RunScheduledLayout(&GetBrowserView()); });
  }

  auto NameTabViewAt(std::string_view tab_name, int tab_index) {
    return NameView(
        tab_name, base::BindLambdaForTesting([&, tab_index]() {
          TabStripModel* tab_strip_model = browser()->GetTabStripModel();
          tabs::TabInterface* tab = tab_strip_model->GetTabAtIndex(tab_index);
          RootTabCollectionNode* root_node =
              GetBrowserView()
                  .vertical_tab_strip_region_view_for_testing()
                  ->root_node_for_testing();
          return root_node->GetNodeForHandle(tab->GetHandle())->view();
        }));
  }

  auto StartDragBetweenTabs(int from_tab_index, int to_tab_index) {
    const char kTabToDragFrom[] = "Tab to drag";
    const char kTabToDragTo[] = "Tab to drag to";
    return Steps(
        Log("Start drag from " + base::NumberToString(from_tab_index) + " to " +
            base::NumberToString(to_tab_index)),
        NameTabViewAt(kTabToDragFrom, from_tab_index),
        NameTabViewAt(kTabToDragTo, to_tab_index), MoveMouseTo(kTabToDragFrom),
        ClickMouse(ui_controls::LEFT, /*release=*/false),
        // Poll state before moving mouse in touch mode to do a long press.
        If([this]() { return mouse_util().GetTouchMode(); },
           Then(Steps(PollState(kDragStatePoller, GetDragActive()),
                      WaitForState(kDragStatePoller, true)))),
        MoveMouseTo(kTabToDragTo), RunScheduledLayout());
  }

  auto StartDragFromGroupToTab(int from_group_index, int to_tab_index) {
    const char kGroupToDragFrom[] = "Group to drag";
    const char kTabToDragTo[] = "Tab to drag to";
    return Steps(
        Log("Start drag from group" + base::NumberToString(from_group_index) +
            " to " + base::NumberToString(to_tab_index)),
        NameDescendantViewByType<VerticalTabGroupHeaderView>(
            kBrowserViewElementId, kGroupToDragFrom, from_group_index),
        NameTabViewAt(kTabToDragTo, to_tab_index),
        MoveMouseTo(kGroupToDragFrom),
        DragMouseTo(kTabToDragTo, CenterPoint(), /*release=*/false),
        RunScheduledLayout());
  }

  auto ContinueDragToTab(int to_tab_index) {
    const char kTabToDragTo[] = "Tab to drag to";
    return Steps(
        Log("Continue drag to tab at " + base::NumberToString(to_tab_index)),
        NameTabViewAt(kTabToDragTo, to_tab_index), MoveMouseTo(kTabToDragTo),
        RunScheduledLayout());
  }

  auto PressEscAsync() {
    return Do([&]() {
      ASSERT_TRUE(ui_controls::SendKeyPress(
          GetLatestBrowser().GetWindow()->GetNativeWindow(), ui::VKEY_ESCAPE,
          false, false, false, false));
    });
  }

  auto AddTabsToNewGroup(const std::vector<int>& indices) {
    return Do([&]() {
      browser()->GetTabStripModel()->AddToNewGroup(indices);
      views::test::RunScheduledLayout(&GetBrowserView());
    });
  }

  auto AddTabsToNewSplit(int index1, int index2) {
    return Do([&, index1, index2] {
      auto* tab_strip_model = browser()->GetTabStripModel();
      tab_strip_model->ActivateTabAt(
          index1, TabStripUserGestureDetails(
                      TabStripUserGestureDetails::GestureType::kOther));
      tab_strip_model->AddToNewSplit(
          {index2}, {}, split_tabs::SplitTabCreatedSource::kTabContextMenu);
      views::test::RunScheduledLayout(&GetBrowserView());
    });
  }

  auto SelectTabAt(int tab_index) {
    const char kTabToSelect[] = "Tab to select";
    return Steps(NameTabViewAt(kTabToSelect, tab_index),
                 MoveMouseTo(kTabToSelect),
                 ClickMouse(ui_controls::MouseButton::LEFT, /*release=*/true,
                            TabSelectModifier()));
  }

  auto PinTabAt(int tab_index) {
    return Do([&, tab_index]() {
      browser()->tab_strip_model()->SetTabPinned(tab_index, true);
      views::test::RunScheduledLayout(&GetBrowserView());
    });
  }

  auto GetScrollOffset() {
    return base::BindRepeating(
        [](VerticalTabDragTest* test) {
          auto* region_view = test->GetBrowserView()
                                  .vertical_tab_strip_region_view_for_testing();
          auto* tab_strip_view = static_cast<VerticalTabStripView*>(
              region_view->GetTabStripView());
          return tab_strip_view->unpinned_tabs_scroll_view()
              ->contents()
              ->GetVisibleBounds()
              .y();
        },
        base::Unretained(this));
  }

  auto NameScrollView(const char* name) {
    return NameView(name, base::BindLambdaForTesting([this]() {
                      return static_cast<views::View*>(
                          static_cast<VerticalTabStripView*>(
                              GetBrowserView()
                                  .vertical_tab_strip_region_view_for_testing()
                                  ->GetTabStripView())
                              ->unpinned_tabs_scroll_view());
                    }));
  }

  auto CollapseGroup(int group_index) {
    return Do([&, group_index]() {
      TabStripModel* model = browser()->tab_strip_model();
      std::vector<tab_groups::TabGroupId> groups =
          model->group_model()->ListTabGroups();
      ASSERT_LT(static_cast<size_t>(group_index), groups.size());
      TabGroup* group = model->group_model()->GetTabGroup(groups[group_index]);
      vertical_tab_strip_controller()->ToggleTabGroupCollapsedState(
          group, ToggleTabGroupCollapsedStateOrigin::kMenuAction);
      views::test::RunScheduledLayout(&GetBrowserView());
    });
  }

  BrowserView& GetBrowserView() {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    EXPECT_TRUE(browser_view != nullptr);
    return *browser_view;
  }

  BrowserWindowInterface& GetLatestBrowser() {
    CHECK(!GlobalBrowserCollection::GetInstance()->IsEmpty());
    BrowserWindowInterface* latest_browser = nullptr;
    GlobalBrowserCollection::GetInstance()->ForEach(
        [&latest_browser](BrowserWindowInterface* browser) {
          latest_browser = browser;
          return true;
        });
    CHECK(latest_browser);
    return *latest_browser;
  }

  gfx::AnimationTestApi::RenderModeResetter disable_animation_ =
      gfx::AnimationTestApi::SetRichAnimationRenderMode(
          gfx::Animation::RichAnimationRenderMode::FORCE_DISABLED);
};

IN_PROC_BROWSER_TEST_F(VerticalTabDragTest, DragWithinUnpinnedContainer) {
  TabStripModel* tab_strip_model = browser()->GetTabStripModel();
  ASSERT_NE(nullptr, tab_strip_model);
  RunTestSequence(
      AddInstrumentedTab(kSecondTab, GURL(chrome::kChromeUIBookmarksURL), 1),
      AddInstrumentedTab(kThirdTab, GURL(chrome::kChromeUISettingsURL), 2),

      StartDragBetweenTabs(2, 1), PollState(kDragStatePoller, GetDragActive()),
      WaitForState(kDragStatePoller, true),
      PollState(kTabOrderPoller, GetTabOrder(tab_strip_model)),
      WaitForState(kTabOrderPoller,
                   URLs({url::kAboutBlankURL, chrome::kChromeUISettingsURL,
                         chrome::kChromeUIBookmarksURL})),

      ContinueDragToTab(2),
      WaitForState(kTabOrderPoller,
                   URLs({url::kAboutBlankURL, chrome::kChromeUIBookmarksURL,
                         chrome::kChromeUISettingsURL})),

      ContinueDragToTab(0),
      WaitForState(kTabOrderPoller,
                   URLs({chrome::kChromeUISettingsURL, url::kAboutBlankURL,
                         chrome::kChromeUIBookmarksURL})),

      // Release the drag and ensure tab ordering remains.
      ReleaseMouse(), WaitForState(kDragStatePoller, false), Do([&]() {
        ASSERT_EQ(3, tab_strip_model->count());
        EXPECT_EQ(GURL(chrome::kChromeUISettingsURL),
                  tab_strip_model->GetWebContentsAt(0)->GetURL());
        EXPECT_EQ(GURL(url::kAboutBlankURL),
                  tab_strip_model->GetWebContentsAt(1)->GetURL());
        EXPECT_EQ(GURL(chrome::kChromeUIBookmarksURL),
                  tab_strip_model->GetWebContentsAt(2)->GetURL());
      }));
}

// This test uses an experimental API to replace mouse events with touch events.
// It is currently only supported on Ash Chrome.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_DragWithinUnpinnedContainerTouch DragWithinUnpinnedContainerTouch
#else
#define MAYBE_DragWithinUnpinnedContainerTouch \
  DISABLED_DragWithinUnpinnedContainerTouch
#endif
IN_PROC_BROWSER_TEST_F(VerticalTabDragTest,
                       MAYBE_DragWithinUnpinnedContainerTouch) {
  TabStripModel* tab_strip_model = browser()->GetTabStripModel();
  ASSERT_NE(nullptr, tab_strip_model);
  RunTestSequence(
      Check([this]() { return mouse_util().SetTouchMode(true); }),
      AddInstrumentedTab(kSecondTab, GURL(chrome::kChromeUIBookmarksURL), 1),
      AddInstrumentedTab(kThirdTab, GURL(chrome::kChromeUISettingsURL), 2),

      StartDragBetweenTabs(2, 1),
      PollState(kTabOrderPoller, GetTabOrder(tab_strip_model)),
      WaitForState(kTabOrderPoller,
                   URLs({url::kAboutBlankURL, chrome::kChromeUISettingsURL,
                         chrome::kChromeUIBookmarksURL})),

      ContinueDragToTab(2),
      WaitForState(kTabOrderPoller,
                   URLs({url::kAboutBlankURL, chrome::kChromeUIBookmarksURL,
                         chrome::kChromeUISettingsURL})),

      ContinueDragToTab(0),
      WaitForState(kTabOrderPoller,
                   URLs({chrome::kChromeUISettingsURL, url::kAboutBlankURL,
                         chrome::kChromeUIBookmarksURL})),

      // Release the drag and ensure tab ordering remains.
      ReleaseMouse(), WaitForState(kDragStatePoller, false), Do([&]() {
        ASSERT_EQ(3, tab_strip_model->count());
        EXPECT_EQ(GURL(chrome::kChromeUISettingsURL),
                  tab_strip_model->GetWebContentsAt(0)->GetURL());
        EXPECT_EQ(GURL(url::kAboutBlankURL),
                  tab_strip_model->GetWebContentsAt(1)->GetURL());
        EXPECT_EQ(GURL(chrome::kChromeUIBookmarksURL),
                  tab_strip_model->GetWebContentsAt(2)->GetURL());
      }));
}

IN_PROC_BROWSER_TEST_F(VerticalTabDragTest, CancelDragWithinUnpinnedContainer) {
  TabStripModel* tab_strip_model = browser()->GetTabStripModel();
  ASSERT_NE(nullptr, tab_strip_model);
  RunTestSequence(
      AddInstrumentedTab(kSecondTab, GURL(chrome::kChromeUIBookmarksURL), 1),
      AddInstrumentedTab(kThirdTab, GURL(chrome::kChromeUISettingsURL), 2),

      StartDragBetweenTabs(2, 1), PollState(kDragStatePoller, GetDragActive()),
      WaitForState(kDragStatePoller, true),
      PollState(kTabOrderPoller, GetTabOrder(tab_strip_model)),
      WaitForState(kTabOrderPoller,
                   URLs({url::kAboutBlankURL, chrome::kChromeUISettingsURL,
                         chrome::kChromeUIBookmarksURL})),

      PressEscAsync(), WaitForState(kDragStatePoller, false), Do([&]() {
        ASSERT_EQ(3, tab_strip_model->count());
        EXPECT_EQ(GURL(url::kAboutBlankURL),
                  tab_strip_model->GetWebContentsAt(0)->GetURL());
        EXPECT_EQ(GURL(chrome::kChromeUIBookmarksURL),
                  tab_strip_model->GetWebContentsAt(1)->GetURL());
        EXPECT_EQ(GURL(chrome::kChromeUISettingsURL),
                  tab_strip_model->GetWebContentsAt(2)->GetURL());
      }));
}

IN_PROC_BROWSER_TEST_F(VerticalTabDragTest, DragSplitTabs) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFourthTab);
  TabStripModel* tab_strip_model = browser()->GetTabStripModel();
  ASSERT_NE(nullptr, tab_strip_model);
  RunTestSequence(
      AddInstrumentedTab(kSecondTab, GURL(chrome::kChromeUIBookmarksURL), 1),
      AddInstrumentedTab(kThirdTab, GURL(chrome::kChromeUISettingsURL), 2),
      AddInstrumentedTab(kFourthTab, GURL(chrome::kChromeUIVersionURL), 3),
      AddTabsToNewSplit(2, 3),
      PollState(kTabOrderPoller, GetTabOrder(tab_strip_model)),
      WaitForState(kTabOrderPoller, URLs({
                                        url::kAboutBlankURL,
                                        chrome::kChromeUIBookmarksURL,
                                        chrome::kChromeUISettingsURL,
                                        chrome::kChromeUIVersionURL,
                                    })),
      StartDragBetweenTabs(2, 0), PollState(kDragStatePoller, GetDragActive()),
      WaitForState(kDragStatePoller, true),
      WaitForState(kTabOrderPoller, URLs({
                                        chrome::kChromeUISettingsURL,
                                        chrome::kChromeUIVersionURL,
                                        url::kAboutBlankURL,
                                        chrome::kChromeUIBookmarksURL,
                                    })),
      ContinueDragToTab(2),
      WaitForState(kTabOrderPoller, URLs({
                                        url::kAboutBlankURL,
                                        chrome::kChromeUISettingsURL,
                                        chrome::kChromeUIVersionURL,
                                        chrome::kChromeUIBookmarksURL,
                                    })),
      ReleaseMouse());
}

IN_PROC_BROWSER_TEST_F(VerticalTabDragTest, DragOverSplit) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFourthTab);
  TabStripModel* tab_strip_model = browser()->GetTabStripModel();
  ASSERT_NE(nullptr, tab_strip_model);
  RunTestSequence(
      AddInstrumentedTab(kSecondTab, GURL(chrome::kChromeUIBookmarksURL), 1),
      AddInstrumentedTab(kThirdTab, GURL(chrome::kChromeUISettingsURL), 2),
      AddInstrumentedTab(kFourthTab, GURL(chrome::kChromeUIVersionURL), 3),
      AddTabsToNewSplit(2, 3),
      PollState(kTabOrderPoller, GetTabOrder(tab_strip_model)),
      WaitForState(kTabOrderPoller, URLs({
                                        url::kAboutBlankURL,
                                        chrome::kChromeUIBookmarksURL,
                                        chrome::kChromeUISettingsURL,
                                        chrome::kChromeUIVersionURL,
                                    })),
      StartDragBetweenTabs(1, 0), PollState(kDragStatePoller, GetDragActive()),
      WaitForState(kDragStatePoller, true),
      WaitForState(kTabOrderPoller, URLs({
                                        chrome::kChromeUIBookmarksURL,
                                        url::kAboutBlankURL,
                                        chrome::kChromeUISettingsURL,
                                        chrome::kChromeUIVersionURL,
                                    })),
      // Dragging from index 0 to index 2 (split) should put the dragged tab to
      // index 3.
      ContinueDragToTab(2),
      WaitForState(kTabOrderPoller, URLs({
                                        url::kAboutBlankURL,
                                        chrome::kChromeUISettingsURL,
                                        chrome::kChromeUIVersionURL,
                                        chrome::kChromeUIBookmarksURL,
                                    })),
      ReleaseMouse());
}

IN_PROC_BROWSER_TEST_F(VerticalTabDragTest, DragOverSplitInGroup) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFourthTab);
  TabStripModel* tab_strip_model = browser()->GetTabStripModel();
  ASSERT_NE(nullptr, tab_strip_model);
  RunTestSequence(
      AddInstrumentedTab(kSecondTab, GURL(chrome::kChromeUIBookmarksURL), 1),
      AddInstrumentedTab(kThirdTab, GURL(chrome::kChromeUISettingsURL), 2),
      AddInstrumentedTab(kFourthTab, GURL(chrome::kChromeUIVersionURL), 3),
      AddTabsToNewGroup({0, 1, 2, 3}), AddTabsToNewSplit(2, 3),
      PollState(kTabOrderPoller, GetTabOrder(tab_strip_model)),
      WaitForState(kTabOrderPoller, URLs({
                                        TabGroupURLs({
                                            url::kAboutBlankURL,
                                            chrome::kChromeUIBookmarksURL,
                                            chrome::kChromeUISettingsURL,
                                            chrome::kChromeUIVersionURL,
                                        }),
                                    })),
      StartDragBetweenTabs(1, 0), PollState(kDragStatePoller, GetDragActive()),
      WaitForState(kDragStatePoller, true), ContinueDragToTab(2),
      WaitForState(kTabOrderPoller, URLs({
                                        TabGroupURLs({
                                            url::kAboutBlankURL,
                                            chrome::kChromeUISettingsURL,
                                            chrome::kChromeUIVersionURL,
                                            chrome::kChromeUIBookmarksURL,
                                        }),
                                    })),
      // Dragging from index 3 to index 2 (split) should put the dragged tab to
      // index 1.
      ContinueDragToTab(2),
      WaitForState(kTabOrderPoller, URLs({
                                        TabGroupURLs({
                                            url::kAboutBlankURL,
                                            chrome::kChromeUIBookmarksURL,
                                            chrome::kChromeUISettingsURL,
                                            chrome::kChromeUIVersionURL,
                                        }),
                                    })),
      ReleaseMouse());
}

// TODO(crbug.com/40249472): Fails on ChromeOS.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_DragMultipleTabs DISABLED_DragMultipleTabs
#else
#define MAYBE_DragMultipleTabs DragMultipleTabs
#endif
IN_PROC_BROWSER_TEST_F(VerticalTabDragTest, MAYBE_DragMultipleTabs) {
  TabStripModel* tab_strip_model = browser()->GetTabStripModel();
  ASSERT_NE(nullptr, tab_strip_model);
  RunTestSequence(
      AddInstrumentedTab(kSecondTab, GURL(chrome::kChromeUIBookmarksURL), 1),
      AddInstrumentedTab(kThirdTab, GURL(chrome::kChromeUISettingsURL), 2),
      SelectTabAt(1),
      CheckResult(
          [this]() { return browser()->tab_strip_model()->IsTabSelected(1); },
          true),
      CheckResult(
          [this]() { return browser()->tab_strip_model()->IsTabSelected(2); },
          true),
      StartDragBetweenTabs(2, 0),
      PollState(kTabOrderPoller, GetTabOrder(tab_strip_model)),
      WaitForState(kTabOrderPoller, URLs({
                                        chrome::kChromeUIBookmarksURL,
                                        chrome::kChromeUISettingsURL,
                                        url::kAboutBlankURL,
                                    })),
      ReleaseMouse());
}

// TODO(crbug.com/40249472): Fails on ChromeOS and Windows.
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
#define MAYBE_DragMultipleTabsInGroup DISABLED_DragMultipleTabsInGroup
#else
#define MAYBE_DragMultipleTabsInGroup DragMultipleTabsInGroup
#endif
IN_PROC_BROWSER_TEST_F(VerticalTabDragTest, MAYBE_DragMultipleTabsInGroup) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFourthTab);
  TabStripModel* tab_strip_model = browser()->GetTabStripModel();
  ASSERT_NE(nullptr, tab_strip_model);
  RunTestSequence(
      AddInstrumentedTab(kSecondTab, GURL(chrome::kChromeUIBookmarksURL), 1),
      AddTabsToNewGroup({0, 1}),
      AddInstrumentedTab(kThirdTab, GURL(chrome::kChromeUISettingsURL), 2),
      AddInstrumentedTab(kFourthTab, GURL(chrome::kChromeUIVersionURL), 3),
      PollState(kTabOrderPoller, GetTabOrder(tab_strip_model)),
      WaitForState(kTabOrderPoller, URLs({
                                        TabGroupURLs({
                                            url::kAboutBlankURL,
                                            chrome::kChromeUIBookmarksURL,
                                        }),
                                        chrome::kChromeUISettingsURL,
                                        chrome::kChromeUIVersionURL,
                                    })),
      SelectTabAt(2),
      CheckResult(
          [this]() { return browser()->tab_strip_model()->IsTabSelected(3); },
          true),
      CheckResult(
          [this]() { return browser()->tab_strip_model()->IsTabSelected(2); },
          true),
      StartDragBetweenTabs(2, 1),
      WaitForState(kTabOrderPoller, URLs({
                                        TabGroupURLs({
                                            url::kAboutBlankURL,
                                            chrome::kChromeUISettingsURL,
                                            chrome::kChromeUIVersionURL,
                                            chrome::kChromeUIBookmarksURL,
                                        }),
                                    })),
      ContinueDragToTab(0),
      WaitForState(kTabOrderPoller, URLs({
                                        TabGroupURLs({
                                            chrome::kChromeUISettingsURL,
                                            chrome::kChromeUIVersionURL,
                                            url::kAboutBlankURL,
                                            chrome::kChromeUIBookmarksURL,
                                        }),
                                    })),
      ReleaseMouse());
}

IN_PROC_BROWSER_TEST_F(VerticalTabDragTest, DragWithinGroup) {
  TabStripModel* tab_strip_model = browser()->GetTabStripModel();
  ASSERT_NE(nullptr, tab_strip_model);
  RunTestSequence(
      AddInstrumentedTab(kSecondTab, GURL(chrome::kChromeUIBookmarksURL), 1),
      AddInstrumentedTab(kThirdTab, GURL(chrome::kChromeUISettingsURL), 2),
      AddTabsToNewGroup({0, 1, 2}),
      PollState(kTabOrderPoller, GetTabOrder(tab_strip_model)),
      WaitForState(kTabOrderPoller, URLs({
                                        TabGroupURLs({
                                            url::kAboutBlankURL,
                                            chrome::kChromeUIBookmarksURL,
                                            chrome::kChromeUISettingsURL,
                                        }),
                                    })),
      StartDragBetweenTabs(2, 1),
      WaitForState(kTabOrderPoller, URLs({TabGroupURLs({
                                        url::kAboutBlankURL,
                                        chrome::kChromeUISettingsURL,
                                        chrome::kChromeUIBookmarksURL,
                                    })})),
      ContinueDragToTab(0),
      WaitForState(kTabOrderPoller, URLs({TabGroupURLs({
                                        chrome::kChromeUISettingsURL,
                                        url::kAboutBlankURL,
                                        chrome::kChromeUIBookmarksURL,
                                    })})),
      ContinueDragToTab(2),
      WaitForState(kTabOrderPoller, URLs({TabGroupURLs({
                                        url::kAboutBlankURL,
                                        chrome::kChromeUIBookmarksURL,
                                        chrome::kChromeUISettingsURL,
                                    })})),
      ReleaseMouse(), Do([&]() {
        ASSERT_EQ(3, tab_strip_model->count());
        EXPECT_EQ(GURL(url::kAboutBlankURL),
                  tab_strip_model->GetWebContentsAt(0)->GetURL());
        EXPECT_EQ(GURL(chrome::kChromeUIBookmarksURL),
                  tab_strip_model->GetWebContentsAt(1)->GetURL());
        EXPECT_EQ(GURL(chrome::kChromeUISettingsURL),
                  tab_strip_model->GetWebContentsAt(2)->GetURL());
      }));
}

IN_PROC_BROWSER_TEST_F(VerticalTabDragTest, DragOutOfGroup) {
  TabStripModel* tab_strip_model = browser()->GetTabStripModel();
  ASSERT_NE(nullptr, tab_strip_model);
  RunTestSequence(
      AddInstrumentedTab(kSecondTab, GURL(chrome::kChromeUIBookmarksURL), 1),
      AddTabsToNewGroup({0, 1}),
      AddInstrumentedTab(kThirdTab, GURL(chrome::kChromeUISettingsURL), 2),
      PollState(kTabOrderPoller, GetTabOrder(tab_strip_model)),
      WaitForState(kTabOrderPoller, URLs({
                                        TabGroupURLs({
                                            url::kAboutBlankURL,
                                            chrome::kChromeUIBookmarksURL,
                                        }),
                                        chrome::kChromeUISettingsURL,
                                    })),
      StartDragBetweenTabs(1, 2),
      WaitForState(kTabOrderPoller, URLs({
                                        TabGroupURLs({
                                            url::kAboutBlankURL,
                                        }),
                                        chrome::kChromeUISettingsURL,
                                        chrome::kChromeUIBookmarksURL,
                                    })),
      ReleaseMouse(), Do([&]() {
        ASSERT_EQ(3, tab_strip_model->count());
        EXPECT_EQ(GURL(url::kAboutBlankURL),
                  tab_strip_model->GetWebContentsAt(0)->GetURL());
        EXPECT_EQ(GURL(chrome::kChromeUISettingsURL),
                  tab_strip_model->GetWebContentsAt(1)->GetURL());
        EXPECT_EQ(GURL(chrome::kChromeUIBookmarksURL),
                  tab_strip_model->GetWebContentsAt(2)->GetURL());
      }));
}

// TODO(crbug.com/40249472): Fails on ChromeOS.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_DragMultiplePinnedTabsWithinContainer \
  DISABLED_DragMultiplePinnedTabsWithinContainer
#else
#define MAYBE_DragMultiplePinnedTabsWithinContainer \
  DragMultiplePinnedTabsWithinContainer
#endif
IN_PROC_BROWSER_TEST_F(VerticalTabDragTest,
                       MAYBE_DragMultiplePinnedTabsWithinContainer) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFourthTab);
  TabStripModel* tab_strip_model = browser()->GetTabStripModel();
  ASSERT_NE(nullptr, tab_strip_model);
  RunTestSequence(
      AddInstrumentedTab(kSecondTab, GURL(chrome::kChromeUIBookmarksURL), 1),
      AddInstrumentedTab(kThirdTab, GURL(chrome::kChromeUISettingsURL), 2),
      AddInstrumentedTab(kFourthTab, GURL(chrome::kChromeUIVersionURL), 3),
      PinTabAt(0), PinTabAt(1), PinTabAt(2), PinTabAt(3),
      PollState(kPinnedTabOrderPoller, GetPinnedTabOrder(tab_strip_model)),
      WaitForState(kPinnedTabOrderPoller, PinnedURLs({
                                              url::kAboutBlankURL,
                                              chrome::kChromeUIBookmarksURL,
                                              chrome::kChromeUISettingsURL,
                                              chrome::kChromeUIVersionURL,
                                          })),

      SelectTabAt(2), Log("Check tab 3 selected"),
      CheckResult(
          [this]() { return browser()->tab_strip_model()->IsTabSelected(3); },
          true),
      Log("Check tab 2 selected"),
      CheckResult(
          [this]() { return browser()->tab_strip_model()->IsTabSelected(2); },
          true),
      StartDragBetweenTabs(2, 0),
      WaitForState(kPinnedTabOrderPoller, PinnedURLs({
                                              chrome::kChromeUISettingsURL,
                                              chrome::kChromeUIVersionURL,
                                              url::kAboutBlankURL,
                                              chrome::kChromeUIBookmarksURL,
                                          })),
      ContinueDragToTab(2),
      WaitForState(kPinnedTabOrderPoller, PinnedURLs({
                                              url::kAboutBlankURL,
                                              chrome::kChromeUISettingsURL,
                                              chrome::kChromeUIVersionURL,
                                              chrome::kChromeUIBookmarksURL,
                                          })),
      ReleaseMouse());
}

IN_PROC_BROWSER_TEST_F(VerticalTabDragTest, DragSplitWithinPinnedContainer) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFourthTab);
  TabStripModel* tab_strip_model = browser()->GetTabStripModel();
  ASSERT_NE(nullptr, tab_strip_model);
  RunTestSequence(
      AddInstrumentedTab(kSecondTab, GURL(chrome::kChromeUIBookmarksURL), 1),
      AddInstrumentedTab(kThirdTab, GURL(chrome::kChromeUISettingsURL), 2),
      AddInstrumentedTab(kFourthTab, GURL(chrome::kChromeUIVersionURL), 3),
      PinTabAt(0), PinTabAt(1), PinTabAt(2), PinTabAt(3),
      AddTabsToNewSplit(2, 3), StartDragBetweenTabs(3, 0),
      PollState(kPinnedTabOrderPoller, GetPinnedTabOrder(tab_strip_model)),
      WaitForState(kPinnedTabOrderPoller, PinnedURLs({
                                              chrome::kChromeUISettingsURL,
                                              chrome::kChromeUIVersionURL,
                                              url::kAboutBlankURL,
                                              chrome::kChromeUIBookmarksURL,
                                          })),
      ContinueDragToTab(2),
      WaitForState(kPinnedTabOrderPoller, PinnedURLs({
                                              url::kAboutBlankURL,
                                              chrome::kChromeUISettingsURL,
                                              chrome::kChromeUIVersionURL,
                                              chrome::kChromeUIBookmarksURL,
                                          })),
      ReleaseMouse());
}

IN_PROC_BROWSER_TEST_F(VerticalTabDragTest,
                       DragFromPinnedToUnpinnedContainerNotAllowed) {
  TabStripModel* tab_strip_model = browser()->GetTabStripModel();
  ASSERT_NE(nullptr, tab_strip_model);
  RunTestSequence(
      AddInstrumentedTab(kSecondTab, GURL(chrome::kChromeUIBookmarksURL), 1),
      AddInstrumentedTab(kThirdTab, GURL(chrome::kChromeUISettingsURL), 2),
      PinTabAt(0),

      // Drag the detached pinned tab over the first unpinned tab - nothing
      // should happen.
      StartDragBetweenTabs(0, 1), PollState(kDragStatePoller, GetDragActive()),
      WaitForState(kDragStatePoller, true),
      PollState(kPinnedTabOrderPoller, GetPinnedTabOrder(tab_strip_model)),
      PollState(kTabOrderPoller, GetTabOrder(tab_strip_model)),
      WaitForState(kPinnedTabOrderPoller, PinnedURLs({url::kAboutBlankURL})),
      WaitForState(kTabOrderPoller, URLs({
                                        url::kAboutBlankURL,
                                        chrome::kChromeUIBookmarksURL,
                                        chrome::kChromeUISettingsURL,
                                    })),
      ReleaseMouse(),

      WaitForState(kPinnedTabOrderPoller, PinnedURLs({url::kAboutBlankURL})),
      WaitForState(kTabOrderPoller, URLs({
                                        url::kAboutBlankURL,
                                        chrome::kChromeUIBookmarksURL,
                                        chrome::kChromeUISettingsURL,
                                    })));
}

IN_PROC_BROWSER_TEST_F(VerticalTabDragTest, DragGroupHeader) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFourthTab);
  TabStripModel* tab_strip_model = browser()->GetTabStripModel();
  ASSERT_NE(nullptr, tab_strip_model);
  RunTestSequence(
      AddInstrumentedTab(kSecondTab, GURL(chrome::kChromeUIBookmarksURL), 1),
      AddInstrumentedTab(kThirdTab, GURL(chrome::kChromeUISettingsURL), 2),
      AddInstrumentedTab(kFourthTab, GURL(chrome::kChromeUIVersionURL), 3),
      AddTabsToNewGroup({1, 2}),
      PollState(kTabOrderPoller, GetTabOrder(tab_strip_model)),
      WaitForState(kTabOrderPoller,
                   URLs({url::kAboutBlankURL,
                         TabGroupURLs({chrome::kChromeUIBookmarksURL,
                                       chrome::kChromeUISettingsURL}),
                         chrome::kChromeUIVersionURL})),

      StartDragFromGroupToTab(0, 0),
      CheckResult(
          [&]() {
            TabStripModel* model = browser()->tab_strip_model();
            std::vector<tab_groups::TabGroupId> groups =
                model->group_model()->ListTabGroups();
            if (groups.empty()) {
              return false;
            }
            return model->group_model()
                ->GetTabGroup(groups[0])
                ->visual_data()
                ->is_collapsed();
          },
          true),
      WaitForState(kTabOrderPoller,
                   URLs({TabGroupURLs({chrome::kChromeUIBookmarksURL,
                                       chrome::kChromeUISettingsURL}),
                         url::kAboutBlankURL, chrome::kChromeUIVersionURL})),

      ContinueDragToTab(3),
      WaitForState(kTabOrderPoller,
                   URLs({url::kAboutBlankURL, chrome::kChromeUIVersionURL,
                         TabGroupURLs({chrome::kChromeUIBookmarksURL,
                                       chrome::kChromeUISettingsURL})})),
      ReleaseMouse(),
      CheckResult(
          [&]() {
            TabStripModel* model = browser()->tab_strip_model();
            std::vector<tab_groups::TabGroupId> groups =
                model->group_model()->ListTabGroups();
            if (groups.empty()) {
              return false;
            }
            return !model->group_model()
                        ->GetTabGroup(groups[0])
                        ->visual_data()
                        ->is_collapsed();
          },
          true));
}

IN_PROC_BROWSER_TEST_F(VerticalTabDragTest, DragCollapsedGroupStaysCollapsed) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFourthTab);
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  ASSERT_NE(nullptr, tab_strip_model);
  RunTestSequence(
      AddInstrumentedTab(kSecondTab, GURL(chrome::kChromeUIBookmarksURL), 1),
      AddInstrumentedTab(kThirdTab, GURL(chrome::kChromeUISettingsURL), 2),
      AddInstrumentedTab(kFourthTab, GURL(chrome::kChromeUIVersionURL), 3),
      AddTabsToNewGroup({0, 1}), CollapseGroup(0), Do([&]() {
        std::vector<tab_groups::TabGroupId> groups =
            tab_strip_model->group_model()->ListTabGroups();
        ASSERT_EQ(1u, groups.size());
        EXPECT_TRUE(tab_strip_model->group_model()
                        ->GetTabGroup(groups[0])
                        ->visual_data()
                        ->is_collapsed());
      }),
      StartDragFromGroupToTab(0, 2),
      PollState(kDragStatePoller, GetDragActive()),
      WaitForState(kDragStatePoller, true),
      // Dragging a collapsed group should keep it collapsed.
      Do([&]() {
        std::vector<tab_groups::TabGroupId> groups =
            tab_strip_model->group_model()->ListTabGroups();
        ASSERT_EQ(1u, groups.size());
        EXPECT_TRUE(tab_strip_model->group_model()
                        ->GetTabGroup(groups[0])
                        ->visual_data()
                        ->is_collapsed());
      }),
      ReleaseMouse());
}

IN_PROC_BROWSER_TEST_F(VerticalTabDragTest,
                       DragCollapsedGroupOverExpandedGroup) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFourthTab);
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  ASSERT_NE(nullptr, tab_strip_model);
  RunTestSequence(
      AddInstrumentedTab(kSecondTab, GURL(chrome::kChromeUIBookmarksURL), 1),
      AddInstrumentedTab(kThirdTab, GURL(chrome::kChromeUISettingsURL), 2),
      AddInstrumentedTab(kFourthTab, GURL(chrome::kChromeUIVersionURL), 3),
      AddTabsToNewGroup({0, 1}), AddTabsToNewGroup({2}), CollapseGroup(1),
      Do([&]() {
        std::vector<tab_groups::TabGroupId> groups =
            tab_strip_model->group_model()->ListTabGroups();
        ASSERT_EQ(2u, groups.size());
        EXPECT_TRUE(tab_strip_model->group_model()
                        ->GetTabGroup(groups[1])
                        ->visual_data()
                        ->is_collapsed());
      }),
      PollState(kTabOrderPoller, GetTabOrder(tab_strip_model)),
      WaitForState(kTabOrderPoller,
                   URLs({TabGroupURLs({url::kAboutBlankURL,
                                       chrome::kChromeUIBookmarksURL}),
                         TabGroupURLs({chrome::kChromeUISettingsURL}),
                         chrome::kChromeUIVersionURL})),

      StartDragFromGroupToTab(1, 0),
      PollState(kDragStatePoller, GetDragActive()),
      WaitForState(kDragStatePoller, true),
      WaitForState(kTabOrderPoller,
                   URLs({TabGroupURLs({chrome::kChromeUISettingsURL}),
                         TabGroupURLs({url::kAboutBlankURL,
                                       chrome::kChromeUIBookmarksURL}),
                         chrome::kChromeUIVersionURL})),
      ReleaseMouse());
}

IN_PROC_BROWSER_TEST_F(VerticalTabDragTest, DragToScroll) {
  const char kScrollViewName[] = "Scroll View";
  const char kFirstTabName[] = "First Tab";
  const int kTabCount = 50;

  RunTestSequence(
      // Add many tabs and ensure the first one is active so we start at the
      // top.
      Do([this]() {
        for (int i = 1; i < kTabCount; ++i) {
          std::unique_ptr<content::WebContents> contents =
              content::WebContents::Create(
                  content::WebContents::CreateParams(browser()->profile()));
          tab_strip_model()->InsertWebContentsAt(tab_strip_model()->count(),
                                                 std::move(contents),
                                                 ADD_INHERIT_OPENER);
        }
        tab_strip_model()->ActivateTabAt(
            0, TabStripUserGestureDetails(
                   TabStripUserGestureDetails::GestureType::kOther));
      }),
      RunScheduledLayout(), NameScrollView(kScrollViewName),
      PollState(kScrollOffsetPoller, GetScrollOffset()),
      WaitForState(kScrollOffsetPoller, 0),

      // Start dragging the first tab.
      NameTabViewAt(kFirstTabName, 0), MoveMouseTo(kFirstTabName),
      ClickMouse(ui_controls::LEFT, /*release=*/false),
      PollState(kDragStatePoller, GetDragActive()),
      WaitForState(kDragStatePoller, true),

      // Move mouse to the bottom edge of the scroll view to trigger scrolling
      // down.
      MoveMouseTo(
          kScrollViewName, base::BindOnce([](ui::TrackedElement* element) {
            const gfx::Rect bounds = element->GetScreenBounds();
            return gfx::Point(bounds.CenterPoint().x(), bounds.bottom() - 1);
          })),

      // Wait for scroll offset to increase.
      WaitForState(kScrollOffsetPoller, testing::Gt(0)),

      // Move mouse to the top edge of the scroll view to trigger scrolling up.
      MoveMouseTo(kScrollViewName,
                  base::BindOnce([](ui::TrackedElement* element) {
                    const gfx::Rect bounds = element->GetScreenBounds();
                    return gfx::Point(bounds.CenterPoint().x(), bounds.y() + 1);
                  })),

      // Wait for scroll offset to decrease back to 0.
      WaitForState(kScrollOffsetPoller, 0),

      ReleaseMouse());
}

// TODO(crbug.com/40249472): Widget DnD creates a blocking loop that isn't
// compatible with out the testing framework generates mouse events. As a
// workaround for Windows, we can send the input events asynchronously.
//
// All tests that involve detaching into a new window must use these custom
// verbs.
class VerticalTabDragDetachTest : public VerticalTabDragTest {
 public:
  VerticalTabDragDetachTest() = default;
  ~VerticalTabDragDetachTest() override = default;

  auto DragTabTo(int tab_index, const gfx::Point& point) {
    const char kTabToDrag[] = "Tab to drag";
    return Steps(
        NameDescendantViewByType<VerticalTabView>(kBrowserViewElementId,
                                                  kTabToDrag, tab_index),
        MoveMouseTo(kTabToDrag),
        ClickMouse(ui_controls::MouseButton::LEFT, /*release=*/false),
        Do([&]() {
          ASSERT_TRUE(ui_controls::SendMouseMove(point.x(), point.y()));
        }));
  }

  auto ReleaseMouseAsync() {
    return Do([&]() {
      ASSERT_TRUE(ui_controls::SendMouseEvents(
          ui_controls::MouseButton::LEFT, ui_controls::MouseButtonState::UP));
    });
  }

  auto MoveMouseToTabAsync(int tab_index, DragPosition position) {
    const char kTabToMoveMouseTo[] = "Tab to move mouse to";
    int offset = 5 * (position == DragPosition::kAbove ? -1 : 1);
    return Steps(NameTabViewAt(kTabToMoveMouseTo, tab_index),
                 WithView(kTabToMoveMouseTo,
                          base::BindOnce(
                              [](int offset, views::View* view) {
                                const gfx::Point point =
                                    view->GetBoundsInScreen().CenterPoint();
                                ASSERT_TRUE(ui_controls::SendMouseMove(
                                    point.x(), point.y() + offset));
                              },
                              offset)));
  }

  auto WaitForDetachedWindowVisible() {
    return Do([&]() {
      BrowserWindowInterface& latest = GetLatestBrowser();
      BrowserView* browser_view =
          BrowserView::GetBrowserViewForBrowser(static_cast<Browser*>(&latest));
      views::Widget* widget = browser_view->GetWidget();
      base::RunLoop run_loop;
      WidgetVisibilityWaiter waiter(widget, run_loop);
      // Wait for the detached window to become visible.
      // Note: We avoid using the `PollState` test verb to wait for visibility
      // unconditionally. On Wayland platforms without move loop support
      // (e.g., Weston), fallback system DnD is used and the production code
      // keeps the window hidden during the drag session. On such platforms,
      // the visibility condition would never be met. (Mutter passes because
      // it supports xdg_toplevel_drag_v1, which allows IsMoveLoopSupported()
      // to return true and allows regular dragging).
      // Thus, we only wait if move loops are supported.
      if (!widget->IsVisible() && widget->IsMoveLoopSupported()) {
        run_loop.Run();
      }
    });
  }
};

// TODO(crbug.com/40249472): Tab DnD tests not working on ChromeOS and Mac.
#if !BUILDFLAG(IS_MAC)
#define MAYBE_DragToDetachIntoNewWindow DragToDetachIntoNewWindow
#else
#define MAYBE_DragToDetachIntoNewWindow DISABLED_DragToDetachIntoNewWindow
#endif
IN_PROC_BROWSER_TEST_F(VerticalTabDragDetachTest,
                       MAYBE_DragToDetachIntoNewWindow) {
  if (base::FeatureList::IsEnabled(features::kInitialWebUI)) {
    GTEST_SKIP() << "Skipping test because it fails with InitialWebUI enabled. "
                    "See b/464087732.";
  }
  RunTestSequence(
      AddInstrumentedTab(kSecondTab, GURL(chrome::kChromeUIBookmarksURL), 1),
      AddInstrumentedTab(kThirdTab, GURL(chrome::kChromeUISettingsURL), 2),
      DragTabTo(1, GetBrowserView().GetBoundsInScreen().top_right() +
                       gfx::Vector2d(50, 50)),
      PollState(kBrowserCountPoller, GetBrowserCount()),
      WaitForState(kBrowserCountPoller, 2), WaitForDetachedWindowVisible(),
      ReleaseMouseAsync(), PollState(kDragStatePoller, GetDragActive()),
      WaitForState(kDragStatePoller, false), Do([&]() {
        TabStripModel* tab_strip_model = GetLatestBrowser().GetTabStripModel();
        ASSERT_NE(nullptr, tab_strip_model);
        EXPECT_EQ(1, tab_strip_model->count());
        EXPECT_EQ(GURL(chrome::kChromeUIBookmarksURL),
                  tab_strip_model->GetWebContentsAt(0)->GetURL());
        EXPECT_EQ(2, browser()->GetTabStripModel()->count());
      }));
}

// TODO(crbug.com/40249472): Tab DnD tests not working on ChromeOS and Mac, and
// flakes on Wayland. Fails on Windows.
#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_LINUX) && \
    !BUILDFLAG(IS_WIN)
#define MAYBE_DragToDetachIntoNewWindowWithVerticalTabsState \
  DragToDetachIntoNewWindowWithVerticalTabsState
#else
#define MAYBE_DragToDetachIntoNewWindowWithVerticalTabsState \
  DISABLED_DragToDetachIntoNewWindowWithVerticalTabsState
#endif
IN_PROC_BROWSER_TEST_F(VerticalTabDragDetachTest,
                       MAYBE_DragToDetachIntoNewWindowWithVerticalTabsState) {
  const int kInitialWidth = 250;
  vertical_tab_strip_state_controller()->RequestCollapse(true);
  vertical_tab_strip_state_controller()->SetUncollapsedWidth(kInitialWidth);

  RunTestSequence(
      AddInstrumentedTab(kSecondTab, GURL(chrome::kChromeUIBookmarksURL), 1),
      DragTabTo(1, GetBrowserView().GetBoundsInScreen().top_right() +
                       gfx::Vector2d(50, 50)),
      PollState(kBrowserCountPoller, GetBrowserCount()),
      WaitForState(kBrowserCountPoller, 2), ReleaseMouseAsync(),
      PollState(kDragStatePoller, GetDragActive()),
      WaitForState(kDragStatePoller, false), Do([&]() {
        BrowserWindowInterface& new_browser = GetLatestBrowser();
        auto* controller =
            tabs::VerticalTabStripStateController::From(&new_browser);
        ASSERT_NE(nullptr, controller);
        EXPECT_TRUE(controller->IsCollapsed());
        EXPECT_EQ(kInitialWidth, controller->GetUncollapsedWidth());
      }));
}

// TODO(crbug.com/40249472): Tab DnD tests not working on ChromeOS and Mac, and
// flakes on Wayland. Fails on Windows.
#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_LINUX)
#define MAYBE_DragToDetachThenCancel DragToDetachThenCancel
#else
#define MAYBE_DragToDetachThenCancel DISABLED_DragToDetachThenCancel
#endif
IN_PROC_BROWSER_TEST_F(VerticalTabDragDetachTest,
                       MAYBE_DragToDetachThenCancel) {
  if (base::FeatureList::IsEnabled(features::kInitialWebUI)) {
    GTEST_SKIP() << "Skipping test because it fails with InitialWebUI enabled. "
                    "See b/464087732.";
  }
  RunTestSequence(
      AddInstrumentedTab(kSecondTab, GURL(chrome::kChromeUIBookmarksURL), 1),
      AddInstrumentedTab(kThirdTab, GURL(chrome::kChromeUISettingsURL), 2),
      DragTabTo(1, GetBrowserView().GetBoundsInScreen().top_right() +
                       gfx::Vector2d(50, 50)),
      PollState(kBrowserCountPoller, GetBrowserCount()),
      WaitForState(kBrowserCountPoller, 2), PressEscAsync(),
      WaitForState(kBrowserCountPoller, 1),
      PollState(kDragStatePoller, GetDragActive()),
      WaitForState(kDragStatePoller, false), Do([&]() {
        TabStripModel* tab_strip_model = browser()->GetTabStripModel();
        ASSERT_NE(nullptr, tab_strip_model);
        ASSERT_EQ(3, tab_strip_model->count());
        EXPECT_EQ(GURL(chrome::kChromeUIBookmarksURL),
                  tab_strip_model->GetWebContentsAt(1)->GetURL());
      }));
}

// TODO(crbug.com/40249472): Tab DnD tests not working on ChromeOS and Mac, and
// flakes on Wayland
#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_LINUX)
#define MAYBE_DragToDetachThenReattach DragToDetachThenReattach
#else
#define MAYBE_DragToDetachThenReattach DISABLED_DragToDetachThenReattach
#endif
IN_PROC_BROWSER_TEST_F(VerticalTabDragDetachTest,
                       MAYBE_DragToDetachThenReattach) {
  RunTestSequence(
      AddInstrumentedTab(kSecondTab, GURL(chrome::kChromeUIBookmarksURL), 1),
      AddInstrumentedTab(kThirdTab, GURL(chrome::kChromeUISettingsURL), 2),
      DragTabTo(2, GetBrowserView().GetBoundsInScreen().top_right() +
                       gfx::Vector2d(50, 50)),
      PollState(kBrowserCountPoller, GetBrowserCount()),
      WaitForState(kBrowserCountPoller, 2),
      MoveMouseToTabAsync(1, DragPosition::kAbove),
      WaitForState(kBrowserCountPoller, 1), ReleaseMouseAsync(),
      PollState(kDragStatePoller, GetDragActive()),
      WaitForState(kDragStatePoller, false), Do([&]() {
        TabStripModel* tab_strip_model = browser()->GetTabStripModel();
        ASSERT_NE(nullptr, tab_strip_model);
        EXPECT_EQ(3, tab_strip_model->count());
      }));
}

#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_CHROMEOS)
#define MAYBE_DetachMultipleTabs DetachMultipleTabs
#else
#define MAYBE_DetachMultipleTabs DISABLED_DetachMultipleTabs
#endif
IN_PROC_BROWSER_TEST_F(VerticalTabDragDetachTest, MAYBE_DetachMultipleTabs) {
  if (base::FeatureList::IsEnabled(features::kInitialWebUI)) {
    GTEST_SKIP() << "Skipping test because it fails with InitialWebUI enabled. "
                    "See b/464087732.";
  }
  RunTestSequence(
      AddInstrumentedTab(kSecondTab, GURL(chrome::kChromeUIBookmarksURL), 1),
      AddInstrumentedTab(kThirdTab, GURL(chrome::kChromeUISettingsURL), 2),
      SelectTabAt(1),
      CheckResult(
          [this]() { return browser()->tab_strip_model()->IsTabSelected(1); },
          true),
      CheckResult(
          [this]() { return browser()->tab_strip_model()->IsTabSelected(2); },
          true),
      DragTabTo(1, GetBrowserView().GetBoundsInScreen().top_right() +
                       gfx::Vector2d(50, 50)),
      PollState(kBrowserCountPoller, GetBrowserCount()),
      WaitForState(kBrowserCountPoller, 2), WaitForDetachedWindowVisible(),
      ReleaseMouseAsync(), PollState(kDragStatePoller, GetDragActive()),
      WaitForState(kDragStatePoller, false), Do([&]() {
        TabStripModel* new_tab_strip_model =
            GetLatestBrowser().GetTabStripModel();
        ASSERT_NE(nullptr, new_tab_strip_model);
        EXPECT_EQ(2, new_tab_strip_model->count());
        EXPECT_EQ(GURL(chrome::kChromeUIBookmarksURL),
                  new_tab_strip_model->GetWebContentsAt(0)->GetURL());
        EXPECT_EQ(GURL(chrome::kChromeUISettingsURL),
                  new_tab_strip_model->GetWebContentsAt(1)->GetURL());
        EXPECT_EQ(1, browser()->GetTabStripModel()->count());
      }));
}

// TODO(crbug.com/40249472): Tab DnD tests not working on ChromeOS and Mac, and
// flakes on Wayland
#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_WIN)
#define MAYBE_DetachPinnedTab DetachPinnedTab
#else
#define MAYBE_DetachPinnedTab DISABLED_DetachPinnedTab
#endif
IN_PROC_BROWSER_TEST_F(VerticalTabDragDetachTest, MAYBE_DetachPinnedTab) {
  RunTestSequence(
      AddInstrumentedTab(kSecondTab, GURL(chrome::kChromeUIBookmarksURL), 1),
      AddInstrumentedTab(kThirdTab, GURL(chrome::kChromeUISettingsURL), 2),
      PinTabAt(0), PinTabAt(1),
      DragTabTo(1, GetBrowserView().GetBoundsInScreen().top_right() +
                       gfx::Vector2d(50, 50)),

      PollState(kBrowserCountPoller, GetBrowserCount()),
      WaitForState(kBrowserCountPoller, 2), ReleaseMouseAsync(),
      PollState(kDragStatePoller, GetDragActive()),
      WaitForState(kDragStatePoller, false), Do([&]() {
        TabStripModel* new_tab_strip_model =
            GetLatestBrowser().GetTabStripModel();
        ASSERT_NE(nullptr, new_tab_strip_model);
        EXPECT_EQ(GURL(chrome::kChromeUIBookmarksURL),
                  new_tab_strip_model->GetWebContentsAt(0)->GetURL());
        EXPECT_EQ(2, browser()->GetTabStripModel()->count());
      }));
}

// TODO(crbug.com/40249472): Tab DnD tests not working on ChromeOS and Mac, and
// flakes on Wayland
#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_LINUX)
#define MAYBE_DetachTabPreservesActiveTab DetachTabPreservesActiveTab
#else
#define MAYBE_DetachTabPreservesActiveTab DISABLED_DetachTabPreservesActiveTab
#endif
IN_PROC_BROWSER_TEST_F(VerticalTabDragDetachTest,
                       MAYBE_DetachTabPreservesActiveTab) {
  if (base::FeatureList::IsEnabled(features::kInitialWebUI)) {
    GTEST_SKIP() << "Skipping test because it fails with InitialWebUI enabled. "
                    "See b/464087732.";
  }
  RunTestSequence(
      AddInstrumentedTab(kSecondTab, GURL(chrome::kChromeUIBookmarksURL), 1),
      AddInstrumentedTab(kThirdTab, GURL(chrome::kChromeUISettingsURL), 2),
      Do([&]() {
        browser()->tab_strip_model()->ActivateTabAt(
            0, TabStripUserGestureDetails(
                   TabStripUserGestureDetails::GestureType::kOther));
      }),
      CheckResult(
          [this]() { return browser()->tab_strip_model()->active_index(); }, 0),
      DragTabTo(2, GetBrowserView().GetBoundsInScreen().top_right() +
                       gfx::Vector2d(50, 50)),
      PollState(kBrowserCountPoller, GetBrowserCount()),
      WaitForState(kBrowserCountPoller, 2), ReleaseMouseAsync(),
      PollState(kDragStatePoller, GetDragActive()),
      WaitForState(kDragStatePoller, false), Do([&]() {
        EXPECT_EQ(0, browser()->tab_strip_model()->active_index());
        EXPECT_EQ(2, browser()->tab_strip_model()->count());
      }));
}

// TODO(crbug.com/490650365): Add regression test once detach tests are working.
