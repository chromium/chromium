// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/test/bind.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/vertical/root_tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_group_header_view.h"
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
#include "content/public/test/browser_test.h"
#include "ui/base/ozone_buildflags.h"
#include "ui/base/test/ui_controls.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/scoped_animation_duration_scale_mode.h"
#include "ui/views/interaction/interactive_views_test.h"
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
  return base::BindRepeating([]() { return chrome::GetTotalBrowserCount(); });
}

base::RepeatingCallback<bool()> GetDragActive() {
  return base::BindRepeating([]() { return TabDragController::IsActive(); });
}

}  // namespace

class VerticalTabDragHandlerTest
    : public VerticalTabsInteractiveTestMixin<InteractiveBrowserTest> {
 public:
  VerticalTabDragHandlerTest() = default;
  ~VerticalTabDragHandlerTest() override = default;

 protected:
  auto DragTabTo(int tab_index, const gfx::Point& point) {
    const char kTabToDrag[] = "Tab to drag";
    return Steps(
        NameDescendantViewByType<VerticalTabView>(kBrowserViewElementId,
                                                  kTabToDrag, tab_index),
        MoveMouseTo(kTabToDrag),
        ClickMouse(ui_controls::MouseButton::LEFT, /*release=*/false),
        Do([&]() {
          // TODO(crbug.com/40249472): Since DnD creates a blocking
          // loop, the initiating mouse movement must be executed
          // asynchronously.
          ASSERT_TRUE(ui_controls::SendMouseMove(point.x(), point.y()));
        }));
  }

  auto DragGroupHeaderTo(int group_index, const gfx::Point& point) {
    // DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kGroupToDrag);
    const char kGroupToDrag[] = "Group to drag";
    DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<bool>,
                                        kViewVisiblePoller);
    return Steps(
        NameDescendantViewByType<VerticalTabGroupHeaderView>(
            kBrowserViewElementId, kGroupToDrag, group_index),
        WaitForShow(kGroupToDrag),

        // Even though the view is showing, it animates in from 0 size.
        // Poll for it to have a non-empty size.
        PollState(
            kViewVisiblePoller,
            base::RepeatingCallback(base::BindLambdaForTesting([&,
                                                                group_index]() {
              TabStripModel* tab_strip_model = browser()->GetTabStripModel();
              auto groups = tab_strip_model->group_model()->ListTabGroups();
              if (groups.size() <= static_cast<size_t>(group_index)) {
                return false;
              }
              auto group_id = groups[group_index];
              RootTabCollectionNode* root_node =
                  GetBrowserView()
                      .vertical_tab_strip_region_view_for_testing()
                      ->root_node_for_testing();
              return !root_node
                          ->GetNodeForHandle(tab_strip_model->group_model()
                                                 ->GetTabGroup(group_id)
                                                 ->GetCollectionHandle())
                          ->view()
                          ->GetVisibleBounds()
                          .IsEmpty();
            }))),
        WaitForState(kViewVisiblePoller, true), MoveMouseTo(kGroupToDrag),
        ClickMouse(ui_controls::MouseButton::LEFT, /*release=*/false),
        Do([&]() {
          // TODO(crbug.com/40249472): Since DnD creates a blocking
          // loop, the initiating mouse movement must be executed
          // asynchronously.
          ASSERT_TRUE(ui_controls::SendMouseMove(point.x(), point.y()));
        }));
  }

  // TODO(crbug.com/40249472): Due to the nature of dragging, events for
  // ending the drag must be executed asynchronoulsy.
  auto ReleaseMouseAsync() {
    return Do([&]() {
      ASSERT_TRUE(ui_controls::SendMouseEvents(
          ui_controls::MouseButton::LEFT, ui_controls::MouseButtonState::UP));
    });
  }

  auto PressEscAsync() {
    return Do([&]() {
      ASSERT_TRUE(ui_controls::SendKeyPress(
          GetLatestBrowser().GetWindow()->GetNativeWindow(), ui::VKEY_ESCAPE,
          false, false, false, false));
    });
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

  auto AddTabsToNewGroup(const std::vector<int>& indices) {
    return Do([&]() { browser()->GetTabStripModel()->AddToNewGroup(indices); });
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

  gfx::ScopedAnimationDurationScaleMode disable_animation_{
      gfx::ScopedAnimationDurationScaleMode::ZERO_DURATION};
};

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

// TODO(crbug.com/40249472): Tab DnD tests not working on ChromeOS and Mac.
#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_CHROMEOS)
#define MAYBE_DragToDetachIntoNewWindow DragToDetachIntoNewWindow
#else
#define MAYBE_DragToDetachIntoNewWindow DISABLED_DragToDetachIntoNewWindow
#endif
IN_PROC_BROWSER_TEST_F(VerticalTabDragHandlerTest,
                       MAYBE_DragToDetachIntoNewWindow) {
  RunTestSequence(
      AddInstrumentedTab(kSecondTab, GURL(chrome::kChromeUIBookmarksURL), 1),
      AddInstrumentedTab(kThirdTab, GURL(chrome::kChromeUISettingsURL), 2),
      DragTabTo(1, GetBrowserView().GetBoundsInScreen().top_right() +
                       gfx::Vector2d(50, 50)),
      PollState(kBrowserCountPoller, GetBrowserCount()),
      WaitForState(kBrowserCountPoller, 2), ReleaseMouseAsync(),
      PollState(kDragStatePoller, GetDragActive()),
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
IN_PROC_BROWSER_TEST_F(VerticalTabDragHandlerTest,
                       MAYBE_DragToDetachIntoNewWindowWithVerticalTabsState) {
  const int kInitialWidth = 250;
  vertical_tab_strip_state_controller()->SetCollapsed(true);
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
#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_LINUX)
#define MAYBE_DragToDetachThenCancel DragToDetachThenCancel
#else
#define MAYBE_DragToDetachThenCancel DISABLED_DragToDetachThenCancel
#endif
IN_PROC_BROWSER_TEST_F(VerticalTabDragHandlerTest,
                       MAYBE_DragToDetachThenCancel) {
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
#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_LINUX)
#define MAYBE_DragToDetachThenReattach DragToDetachThenReattach
#else
#define MAYBE_DragToDetachThenReattach DISABLED_DragToDetachThenReattach
#endif
IN_PROC_BROWSER_TEST_F(VerticalTabDragHandlerTest,
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

// TODO(crbug.com/40249472): Tab DnD tests not working on ChromeOS and Mac, and
// flakes on Wayland
#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_LINUX)
#define MAYBE_DragWithinUnpinnedContainer DragWithinUnpinnedContainer
#else
#define MAYBE_DragWithinUnpinnedContainer DISABLED_DragWithinUnpinnedContainer
#endif
IN_PROC_BROWSER_TEST_F(VerticalTabDragHandlerTest,
                       MAYBE_DragWithinUnpinnedContainer) {
  TabStripModel* tab_strip_model = browser()->GetTabStripModel();
  ASSERT_NE(nullptr, tab_strip_model);
  RunTestSequence(
      AddInstrumentedTab(kSecondTab, GURL(chrome::kChromeUIBookmarksURL), 1),
      AddInstrumentedTab(kThirdTab, GURL(chrome::kChromeUISettingsURL), 2),
      DragTabTo(2, GetBrowserView().GetBoundsInScreen().top_right() +
                       gfx::Vector2d(50, 50)),
      PollState(kDragStatePoller, GetDragActive()),
      WaitForState(kDragStatePoller, true),
      PollState(kTabOrderPoller, GetTabOrder(tab_strip_model)),

      MoveMouseToTabAsync(1, DragPosition::kBelow),
      WaitForState(kTabOrderPoller,
                   URLs({url::kAboutBlankURL, chrome::kChromeUISettingsURL,
                         chrome::kChromeUIBookmarksURL})),

      MoveMouseToTabAsync(0, DragPosition::kAbove),
      WaitForState(kTabOrderPoller,
                   URLs({chrome::kChromeUISettingsURL, url::kAboutBlankURL,
                         chrome::kChromeUIBookmarksURL})),

      // Release the drag and ensure tab ordering remains.
      ReleaseMouseAsync(), WaitForState(kDragStatePoller, false), Do([&]() {
        ASSERT_EQ(3, tab_strip_model->count());
        EXPECT_EQ(GURL(chrome::kChromeUISettingsURL),
                  tab_strip_model->GetWebContentsAt(0)->GetURL());
        EXPECT_EQ(GURL(url::kAboutBlankURL),
                  tab_strip_model->GetWebContentsAt(1)->GetURL());
        EXPECT_EQ(GURL(chrome::kChromeUIBookmarksURL),
                  tab_strip_model->GetWebContentsAt(2)->GetURL());
      }));
}

// TODO(crbug.com/40249472): Tab DnD tests not working on ChromeOS and Mac, and
// flakes on Wayland
#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_LINUX)
#define MAYBE_CancelDragWithinUnpinnedContainer \
  CancelDragWithinUnpinnedContainer
#else
#define MAYBE_CancelDragWithinUnpinnedContainer \
  DISABLED_CancelDragWithinUnpinnedContainer
#endif
IN_PROC_BROWSER_TEST_F(VerticalTabDragHandlerTest,
                       MAYBE_CancelDragWithinUnpinnedContainer) {
  TabStripModel* tab_strip_model = browser()->GetTabStripModel();
  ASSERT_NE(nullptr, tab_strip_model);
  RunTestSequence(
      AddInstrumentedTab(kSecondTab, GURL(chrome::kChromeUIBookmarksURL), 1),
      AddInstrumentedTab(kThirdTab, GURL(chrome::kChromeUISettingsURL), 2),
      Log("Start dragging tab 2"),
      DragTabTo(2, GetBrowserView().GetBoundsInScreen().top_right() +
                       gfx::Vector2d(50, 50)),
      PollState(kDragStatePoller, GetDragActive()),
      PollState(kBrowserCountPoller, GetBrowserCount()),
      WaitForState(kBrowserCountPoller, 2),
      WaitForState(kDragStatePoller, true),

      // Move mouse over the last tab and check tab ordering.
      Log("Drag to tab at index 1"),
      MoveMouseToTabAsync(1, DragPosition::kAbove),
      WaitForState(kBrowserCountPoller, 1),
      PollState(kTabOrderPoller, GetTabOrder(tab_strip_model)),
      WaitForState(kTabOrderPoller,
                   URLs({url::kAboutBlankURL, chrome::kChromeUISettingsURL,
                         chrome::kChromeUIBookmarksURL})),
      Log("Cancel drag with Esc"), PressEscAsync(),
      WaitForState(kDragStatePoller, false), Do([&]() {
        ASSERT_EQ(3, tab_strip_model->count());
        EXPECT_EQ(GURL(url::kAboutBlankURL),
                  tab_strip_model->GetWebContentsAt(0)->GetURL());
        EXPECT_EQ(GURL(chrome::kChromeUIBookmarksURL),
                  tab_strip_model->GetWebContentsAt(1)->GetURL());
        EXPECT_EQ(GURL(chrome::kChromeUISettingsURL),
                  tab_strip_model->GetWebContentsAt(2)->GetURL());
      }));
}

// TODO(crbug.com/40249472): Disabled because this flakes on all platforms.
IN_PROC_BROWSER_TEST_F(VerticalTabDragHandlerTest, DISABLED_DragSplitTabs) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFourthTab);
  TabStripModel* tab_strip_model = browser()->GetTabStripModel();
  ASSERT_NE(nullptr, tab_strip_model);
  RunTestSequence(
      AddInstrumentedTab(kSecondTab, GURL(chrome::kChromeUIBookmarksURL), 1),
      AddInstrumentedTab(kThirdTab, GURL(chrome::kChromeUISettingsURL), 2),
      AddInstrumentedTab(kFourthTab, GURL(chrome::kChromeUIVersionURL), 3),
      Do([&]() {
        tab_strip_model->ActivateTabAt(
            2, TabStripUserGestureDetails(
                   TabStripUserGestureDetails::GestureType::kOther));
        tab_strip_model->AddToNewSplit(
            {3}, {}, split_tabs::SplitTabCreatedSource::kTabContextMenu);
      }),
      PollState(kTabOrderPoller, GetTabOrder(tab_strip_model)),
      WaitForState(kTabOrderPoller, URLs({
                                        url::kAboutBlankURL,
                                        chrome::kChromeUIBookmarksURL,
                                        chrome::kChromeUISettingsURL,
                                        chrome::kChromeUIVersionURL,
                                    })),
      DragTabTo(2, GetBrowserView().GetBoundsInScreen().top_right() +
                       gfx::Vector2d(50, 50)),
      PollState(kDragStatePoller, GetDragActive()),
      MoveMouseToTabAsync(0, DragPosition::kAbove),
      WaitForState(kTabOrderPoller, URLs({
                                        chrome::kChromeUISettingsURL,
                                        chrome::kChromeUIVersionURL,
                                        url::kAboutBlankURL,
                                        chrome::kChromeUIBookmarksURL,
                                    })),
      MoveMouseToTabAsync(2, DragPosition::kAbove),
      WaitForState(kTabOrderPoller, URLs({
                                        url::kAboutBlankURL,
                                        chrome::kChromeUISettingsURL,
                                        chrome::kChromeUIVersionURL,
                                        chrome::kChromeUIBookmarksURL,
                                    })),
      ReleaseMouseAsync());
}

// TODO(crbug.com/40249472): Disabled because this flakes on all platforms.
IN_PROC_BROWSER_TEST_F(VerticalTabDragHandlerTest, DISABLED_DragOverSplit) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFourthTab);
  TabStripModel* tab_strip_model = browser()->GetTabStripModel();
  ASSERT_NE(nullptr, tab_strip_model);
  RunTestSequence(
      AddInstrumentedTab(kSecondTab, GURL(chrome::kChromeUIBookmarksURL), 1),
      AddInstrumentedTab(kThirdTab, GURL(chrome::kChromeUISettingsURL), 2),
      AddInstrumentedTab(kFourthTab, GURL(chrome::kChromeUIVersionURL), 3),
      Do([&]() {
        tab_strip_model->ActivateTabAt(
            2, TabStripUserGestureDetails(
                   TabStripUserGestureDetails::GestureType::kOther));
        tab_strip_model->AddToNewSplit(
            {3}, {}, split_tabs::SplitTabCreatedSource::kTabContextMenu);
      }),
      PollState(kTabOrderPoller, GetTabOrder(tab_strip_model)),
      WaitForState(kTabOrderPoller, URLs({
                                        url::kAboutBlankURL,
                                        chrome::kChromeUIBookmarksURL,
                                        chrome::kChromeUISettingsURL,
                                        chrome::kChromeUIVersionURL,
                                    })),
      DragTabTo(1, GetBrowserView().GetBoundsInScreen().top_right() +
                       gfx::Vector2d(50, 50)),
      PollState(kDragStatePoller, GetDragActive()),
      MoveMouseToTabAsync(0, DragPosition::kAbove),
      WaitForState(kTabOrderPoller, URLs({
                                        chrome::kChromeUIBookmarksURL,
                                        url::kAboutBlankURL,
                                        chrome::kChromeUISettingsURL,
                                        chrome::kChromeUIVersionURL,
                                    })),
      // Dragging from index 0 to index 2 (split) should put the dragged tab to
      // index 3.
      MoveMouseToTabAsync(2, DragPosition::kAbove),
      WaitForState(kTabOrderPoller, URLs({
                                        url::kAboutBlankURL,
                                        chrome::kChromeUISettingsURL,
                                        chrome::kChromeUIVersionURL,
                                        chrome::kChromeUIBookmarksURL,
                                    })),
      // Dragging from index 3 to index 2 (split) should put the dragged tab to
      // index 1.
      MoveMouseToTabAsync(2, DragPosition::kAbove),
      WaitForState(kTabOrderPoller, URLs({
                                        url::kAboutBlankURL,
                                        chrome::kChromeUIBookmarksURL,
                                        chrome::kChromeUISettingsURL,
                                        chrome::kChromeUIVersionURL,
                                    })),
      ReleaseMouseAsync());
}

// TODO(crbug.com/40249472): Disabled because this flakes on all platforms.
IN_PROC_BROWSER_TEST_F(VerticalTabDragHandlerTest,
                       DISABLED_DragOverSplitInGroup) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFourthTab);
  TabStripModel* tab_strip_model = browser()->GetTabStripModel();
  ASSERT_NE(nullptr, tab_strip_model);
  RunTestSequence(
      AddInstrumentedTab(kSecondTab, GURL(chrome::kChromeUIBookmarksURL), 1),
      AddInstrumentedTab(kThirdTab, GURL(chrome::kChromeUISettingsURL), 2),
      AddInstrumentedTab(kFourthTab, GURL(chrome::kChromeUIVersionURL), 3),
      AddTabsToNewGroup({2, 3}), Do([&]() {
        tab_strip_model->ActivateTabAt(
            2, TabStripUserGestureDetails(
                   TabStripUserGestureDetails::GestureType::kOther));
        tab_strip_model->AddToNewSplit(
            {3}, {}, split_tabs::SplitTabCreatedSource::kTabContextMenu);
      }),
      PollState(kTabOrderPoller, GetTabOrder(tab_strip_model)),
      WaitForState(kTabOrderPoller, URLs({
                                        url::kAboutBlankURL,
                                        chrome::kChromeUIBookmarksURL,
                                        TabGroupURLs({
                                            chrome::kChromeUISettingsURL,
                                            chrome::kChromeUIVersionURL,
                                        }),
                                    })),
      DragTabTo(1, GetBrowserView().GetBoundsInScreen().top_right() +
                       gfx::Vector2d(50, 50)),
      PollState(kDragStatePoller, GetDragActive()),
      MoveMouseToTabAsync(0, DragPosition::kAbove),
      WaitForState(kTabOrderPoller, URLs({
                                        chrome::kChromeUIBookmarksURL,
                                        url::kAboutBlankURL,
                                        TabGroupURLs({
                                            chrome::kChromeUISettingsURL,
                                            chrome::kChromeUIVersionURL,
                                        }),
                                    })),
      // Dragging from index 0 to index 2 (split) should put the dragged tab to
      // index 3.
      MoveMouseToTabAsync(2, DragPosition::kAbove),
      WaitForState(kTabOrderPoller, URLs({
                                        url::kAboutBlankURL,
                                        TabGroupURLs({
                                            chrome::kChromeUISettingsURL,
                                            chrome::kChromeUIVersionURL,
                                            chrome::kChromeUIBookmarksURL,
                                        }),
                                    })),
      // Dragging from index 3 to index 2 (split) should put the dragged tab to
      // index 1.
      MoveMouseToTabAsync(2, DragPosition::kAbove),
      WaitForState(kTabOrderPoller, URLs({
                                        url::kAboutBlankURL,
                                        TabGroupURLs({
                                            chrome::kChromeUIBookmarksURL,
                                            chrome::kChromeUISettingsURL,
                                            chrome::kChromeUIVersionURL,
                                        }),
                                    })),
      ReleaseMouseAsync());
}

#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_CHROMEOS)
#define MAYBE_DetachMultipleTabs DetachMultipleTabs
#else
#define MAYBE_DetachMultipleTabs DISABLED_DetachMultipleTabs
#endif
IN_PROC_BROWSER_TEST_F(VerticalTabDragHandlerTest, MAYBE_DetachMultipleTabs) {
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
      WaitForState(kBrowserCountPoller, 2), ReleaseMouseAsync(),
      PollState(kDragStatePoller, GetDragActive()),
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
      }),
      ReleaseMouseAsync());
}

#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_LINUX)
#define MAYBE_DragMultipleTabs DragMultipleTabs
#else
#define MAYBE_DragMultipleTabs DISABLED_DragMultipleTabs
#endif
IN_PROC_BROWSER_TEST_F(VerticalTabDragHandlerTest, MAYBE_DragMultipleTabs) {
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
      DragTabTo(2, GetBrowserView().GetBoundsInScreen().top_right() +
                       gfx::Vector2d(50, 50)),
      MoveMouseToTabAsync(0, DragPosition::kAbove),
      PollState(kTabOrderPoller, GetTabOrder(tab_strip_model)),
      WaitForState(kTabOrderPoller, URLs({
                                        chrome::kChromeUIBookmarksURL,
                                        chrome::kChromeUISettingsURL,
                                        url::kAboutBlankURL,
                                    })),
      ReleaseMouseAsync());
}

// TODO(crbug.com/40249472): Disabled because this flakes on all platforms.
IN_PROC_BROWSER_TEST_F(VerticalTabDragHandlerTest,
                       DISABLED_DragMultipleTabsInGroup) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFourthTab);
  TabStripModel* tab_strip_model = browser()->GetTabStripModel();
  ASSERT_NE(nullptr, tab_strip_model);
  RunTestSequence(
      AddInstrumentedTab(kSecondTab, GURL(chrome::kChromeUIBookmarksURL), 1),
      AddInstrumentedTab(kThirdTab, GURL(chrome::kChromeUISettingsURL), 2),
      AddInstrumentedTab(kFourthTab, GURL(chrome::kChromeUIVersionURL), 3),
      AddTabsToNewGroup({1}),
      PollState(kTabOrderPoller, GetTabOrder(tab_strip_model)),
      WaitForState(kTabOrderPoller, URLs({
                                        url::kAboutBlankURL,
                                        TabGroupURLs({
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
      DragTabTo(2, GetBrowserView().GetBoundsInScreen().top_right() +
                       gfx::Vector2d(50, 50)),
      MoveMouseToTabAsync(1, DragPosition::kAbove),
      WaitForState(kTabOrderPoller, URLs({
                                        url::kAboutBlankURL,
                                        TabGroupURLs({
                                            chrome::kChromeUISettingsURL,
                                            chrome::kChromeUIVersionURL,
                                            chrome::kChromeUIBookmarksURL,
                                        }),
                                    })),
      MoveMouseToTabAsync(0, DragPosition::kAbove),
      WaitForState(kTabOrderPoller, URLs({
                                        chrome::kChromeUISettingsURL,
                                        chrome::kChromeUIVersionURL,
                                        url::kAboutBlankURL,
                                        TabGroupURLs({
                                            chrome::kChromeUIBookmarksURL,
                                        }),
                                    })),
      ReleaseMouseAsync());
}

// TODO(crbug.com/40249472): Tab DnD tests not working on ChromeOS and Mac, and
// flakes on Wayland
#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_LINUX)
#define MAYBE_DragInGroup DragInGroup
#else
#define MAYBE_DragInGroup DISABLED_DragInGroup
#endif
IN_PROC_BROWSER_TEST_F(VerticalTabDragHandlerTest, MAYBE_DragInGroup) {
  TabStripModel* tab_strip_model = browser()->GetTabStripModel();
  ASSERT_NE(nullptr, tab_strip_model);
  RunTestSequence(
      AddInstrumentedTab(kSecondTab, GURL(chrome::kChromeUIBookmarksURL), 1),
      AddTabsToNewGroup({0, 1}),
      AddInstrumentedTab(kThirdTab, GURL(chrome::kChromeUISettingsURL), 2),
      PollState(kTabOrderPoller, GetTabOrder(tab_strip_model)),
      WaitForState(kTabOrderPoller,
                   URLs({
                       TabGroupURLs({url::kAboutBlankURL,
                                     chrome::kChromeUIBookmarksURL}),
                       chrome::kChromeUISettingsURL,
                   })),
      DragTabTo(2, GetBrowserView().GetBoundsInScreen().top_right() +
                       gfx::Vector2d(50, 50)),
      PollState(kDragStatePoller, GetDragActive()),
      PollState(kBrowserCountPoller, GetBrowserCount()),
      WaitForState(kDragStatePoller, true),
      WaitForState(kBrowserCountPoller, 2),

      MoveMouseToTabAsync(1, DragPosition::kAbove),
      WaitForState(kTabOrderPoller, URLs({TabGroupURLs({
                                        url::kAboutBlankURL,
                                        chrome::kChromeUISettingsURL,
                                        chrome::kChromeUIBookmarksURL,
                                    })})),
      ReleaseMouseAsync(), WaitForState(kDragStatePoller, false), Do([&]() {
        ASSERT_EQ(3, tab_strip_model->count());
        EXPECT_EQ(GURL(url::kAboutBlankURL),
                  tab_strip_model->GetWebContentsAt(0)->GetURL());
        EXPECT_EQ(GURL(chrome::kChromeUISettingsURL),
                  tab_strip_model->GetWebContentsAt(1)->GetURL());
        EXPECT_EQ(GURL(chrome::kChromeUIBookmarksURL),
                  tab_strip_model->GetWebContentsAt(2)->GetURL());
      }));
}

// TODO(crbug.com/40249472): Disabled because this flakes on all platforms.
IN_PROC_BROWSER_TEST_F(VerticalTabDragHandlerTest, DISABLED_DragOutOfGroup) {
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
      DragTabTo(1, GetBrowserView().GetBoundsInScreen().top_right() +
                       gfx::Vector2d(50, 50)),
      PollState(kDragStatePoller, GetDragActive()),
      WaitForState(kDragStatePoller, true),
      MoveMouseToTabAsync(2, DragPosition::kAbove),
      WaitForState(kTabOrderPoller, URLs({
                                        TabGroupURLs({
                                            url::kAboutBlankURL,
                                        }),
                                        chrome::kChromeUISettingsURL,
                                        chrome::kChromeUIBookmarksURL,
                                    })),
      ReleaseMouseAsync(), WaitForState(kDragStatePoller, false), Do([&]() {
        ASSERT_EQ(3, tab_strip_model->count());
        EXPECT_EQ(GURL(url::kAboutBlankURL),
                  tab_strip_model->GetWebContentsAt(0)->GetURL());
        EXPECT_EQ(GURL(chrome::kChromeUISettingsURL),
                  tab_strip_model->GetWebContentsAt(1)->GetURL());
        EXPECT_EQ(GURL(chrome::kChromeUIBookmarksURL),
                  tab_strip_model->GetWebContentsAt(2)->GetURL());
      }));
}

// TODO(crbug.com/40249472): Tab DnD tests not working on ChromeOS and Mac, and
// flakes on Wayland
#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_LINUX) && \
    !BUILDFLAG(IS_WIN)
#define MAYBE_DragPinnedTabWithinContainer DragPinnedTabWithinContainer
#else
#define MAYBE_DragPinnedTabWithinContainer DISABLED_DragPinnedTabWithinContainer
#endif
IN_PROC_BROWSER_TEST_F(VerticalTabDragHandlerTest,
                       MAYBE_DragPinnedTabWithinContainer) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFourthTab);
  TabStripModel* tab_strip_model = browser()->GetTabStripModel();
  ASSERT_NE(nullptr, tab_strip_model);
  RunTestSequence(
      AddInstrumentedTab(kSecondTab, GURL(chrome::kChromeUIBookmarksURL), 1),
      AddInstrumentedTab(kThirdTab, GURL(chrome::kChromeUISettingsURL), 2),
      AddInstrumentedTab(kFourthTab, GURL(chrome::kChromeUIVersionURL), 3),
      PinTabAt(0), PinTabAt(1), PinTabAt(2), PinTabAt(3), SelectTabAt(0),
      SelectTabAt(2),
      CheckResult(
          [this]() { return browser()->tab_strip_model()->IsTabSelected(3); },
          true),
      CheckResult(
          [this]() { return browser()->tab_strip_model()->IsTabSelected(2); },
          true),
      DragTabTo(2, GetBrowserView().GetBoundsInScreen().top_right() +
                       gfx::Vector2d(50, 50)),
      PollState(kDragStatePoller, GetDragActive()),
      WaitForState(kDragStatePoller, true),

      PollState(kPinnedTabOrderPoller, GetPinnedTabOrder(tab_strip_model)),

      MoveMouseToTabAsync(0, DragPosition::kAbove),
      WaitForState(kPinnedTabOrderPoller, PinnedURLs({
                                              chrome::kChromeUISettingsURL,
                                              chrome::kChromeUIVersionURL,
                                              url::kAboutBlankURL,
                                              chrome::kChromeUIBookmarksURL,
                                          })),
      MoveMouseToTabAsync(2, DragPosition::kAbove),
      WaitForState(kPinnedTabOrderPoller, PinnedURLs({
                                              url::kAboutBlankURL,
                                              chrome::kChromeUISettingsURL,
                                              chrome::kChromeUIVersionURL,
                                              chrome::kChromeUIBookmarksURL,
                                          })),
      ReleaseMouseAsync());
}

// TODO(crbug.com/40249472): Tab DnD tests not working on ChromeOS and Mac, and
// flakes on Wayland
#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_LINUX) && \
    !BUILDFLAG(IS_WIN)
#define MAYBE_DragSplitWithinPinnedContainer DragSplitWithinPinnedContainer
#else
#define MAYBE_DragSplitWithinPinnedContainer \
  DISABLED_DragSplitWithinPinnedContainer
#endif
IN_PROC_BROWSER_TEST_F(VerticalTabDragHandlerTest,
                       MAYBE_DragSplitWithinPinnedContainer) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFourthTab);
  TabStripModel* tab_strip_model = browser()->GetTabStripModel();
  ASSERT_NE(nullptr, tab_strip_model);
  RunTestSequence(
      AddInstrumentedTab(kSecondTab, GURL(chrome::kChromeUIBookmarksURL), 1),
      AddInstrumentedTab(kThirdTab, GURL(chrome::kChromeUISettingsURL), 2),
      AddInstrumentedTab(kFourthTab, GURL(chrome::kChromeUIVersionURL), 3),
      PinTabAt(0), PinTabAt(1), PinTabAt(2), PinTabAt(3), Do([&]() {
        tab_strip_model->ActivateTabAt(
            2, TabStripUserGestureDetails(
                   TabStripUserGestureDetails::GestureType::kOther));
        tab_strip_model->AddToNewSplit(
            {3}, {}, split_tabs::SplitTabCreatedSource::kTabContextMenu);
      }),
      DragTabTo(3, GetBrowserView().GetBoundsInScreen().top_right() +
                       gfx::Vector2d(50, 50)),
      PollState(kDragStatePoller, GetDragActive()),
      WaitForState(kDragStatePoller, true),

      PollState(kPinnedTabOrderPoller, GetPinnedTabOrder(tab_strip_model)),
      MoveMouseToTabAsync(0, DragPosition::kAbove),
      WaitForState(kPinnedTabOrderPoller, PinnedURLs({
                                              chrome::kChromeUISettingsURL,
                                              chrome::kChromeUIVersionURL,
                                              url::kAboutBlankURL,
                                              chrome::kChromeUIBookmarksURL,
                                          })),
      MoveMouseToTabAsync(2, DragPosition::kAbove),
      WaitForState(kPinnedTabOrderPoller, PinnedURLs({
                                              url::kAboutBlankURL,
                                              chrome::kChromeUISettingsURL,
                                              chrome::kChromeUIVersionURL,
                                              chrome::kChromeUIBookmarksURL,
                                          })),
      ReleaseMouseAsync());
}

// TODO(crbug.com/40249472): Tab DnD tests not working on ChromeOS and Mac, and
// flakes on Wayland
#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_LINUX) && \
    !BUILDFLAG(IS_WIN)
#define MAYBE_DetachPinnedTab DetachPinnedTab
#else
#define MAYBE_DetachPinnedTab DISABLED_DetachPinnedTab
#endif
IN_PROC_BROWSER_TEST_F(VerticalTabDragHandlerTest, MAYBE_DetachPinnedTab) {
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
#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_LINUX) && \
    !BUILDFLAG(IS_WIN)
#define MAYBE_DragFromPinnedToUnpinnedContainer \
  DragFromPinnedToUnpinnedContainer
#else
#define MAYBE_DragFromPinnedToUnpinnedContainer \
  DISABLED_DragFromPinnedToUnpinnedContainer
#endif
IN_PROC_BROWSER_TEST_F(VerticalTabDragHandlerTest,
                       MAYBE_DragFromPinnedToUnpinnedContainer) {
  TabStripModel* tab_strip_model = browser()->GetTabStripModel();
  ASSERT_NE(nullptr, tab_strip_model);
  RunTestSequence(
      AddInstrumentedTab(kSecondTab, GURL(chrome::kChromeUIBookmarksURL), 1),
      AddInstrumentedTab(kThirdTab, GURL(chrome::kChromeUISettingsURL), 2),
      PinTabAt(0),
      DragTabTo(0, GetBrowserView().GetBoundsInScreen().top_right() +
                       gfx::Vector2d(50, 50)),

      PollState(kBrowserCountPoller, GetBrowserCount()),
      WaitForState(kBrowserCountPoller, 2),

      // Drag the detached pinned tab over the second unpinned tab in the
      // original window, the pinned tab should remain pinned.
      MoveMouseToTabAsync(1, DragPosition::kAbove),
      PollState(kPinnedTabOrderPoller, GetPinnedTabOrder(tab_strip_model)),
      PollState(kTabOrderPoller, GetTabOrder(tab_strip_model)),
      WaitForState(kPinnedTabOrderPoller, PinnedURLs({url::kAboutBlankURL})),
      WaitForState(kTabOrderPoller, URLs({
                                        url::kAboutBlankURL,
                                        chrome::kChromeUIBookmarksURL,
                                        chrome::kChromeUISettingsURL,
                                    })),
      ReleaseMouseAsync());
}

// TODO(crbug.com/40249472): Tab DnD tests not working on ChromeOS and Mac, and
// flakes on Wayland
#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_CHROMEOS) && \
    !BUILDFLAG(SUPPORTS_OZONE_WAYLAND)
#define MAYBE_DragGroupHeader DragGroupHeader
#else
#define MAYBE_DragGroupHeader DISABLED_DragGroupHeader
#endif
IN_PROC_BROWSER_TEST_F(VerticalTabDragHandlerTest, MAYBE_DragGroupHeader) {
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

      Log("Starting drag"),
      DragGroupHeaderTo(0, GetBrowserView().GetBoundsInScreen().top_right() +
                               gfx::Vector2d(50, 50)),
      PollState(kDragStatePoller, GetDragActive()),
      WaitForState(kDragStatePoller, true),

      Log("Moving tabs to index 0"),
      MoveMouseToTabAsync(0, DragPosition::kAbove),
      WaitForState(kTabOrderPoller,
                   URLs({TabGroupURLs({chrome::kChromeUIBookmarksURL,
                                       chrome::kChromeUISettingsURL}),
                         url::kAboutBlankURL, chrome::kChromeUIVersionURL})),

      Log("Moving tabs to index 3"),
      MoveMouseToTabAsync(3, DragPosition::kAbove),
      WaitForState(kTabOrderPoller,
                   URLs({url::kAboutBlankURL, chrome::kChromeUIVersionURL,
                         TabGroupURLs({chrome::kChromeUIBookmarksURL,
                                       chrome::kChromeUISettingsURL})})),
      ReleaseMouseAsync());
}
