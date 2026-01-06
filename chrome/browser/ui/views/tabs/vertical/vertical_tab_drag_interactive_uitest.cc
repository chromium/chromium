// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_view.h"
#include "chrome/browser/ui/views/test/vertical_tabs_interactive_test_mixin.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "ui/base/ozone_buildflags.h"
#include "ui/base/test/ui_controls.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/view.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace {

using URLs = std::vector<std::string_view>;

base::RepeatingCallback<size_t()> GetBrowserCount() {
  return base::BindRepeating([]() { return chrome::GetTotalBrowserCount(); });
}

base::RepeatingCallback<bool()> GetDragActive() {
  return base::BindRepeating([]() { return TabDragController::IsActive(); });
}

base::RepeatingCallback<URLs()> GetTabOrder(TabStripModel* model) {
  return base::BindRepeating(
      [](TabStripModel* model) {
        URLs urls;
        for (auto i = 0; i < model->count(); ++i) {
          urls.push_back(model->GetWebContentsAt(i)->GetURL().spec());
        }
        return urls;
      },
      model);
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

  auto MoveMouseToTabAsync(int tab_index) {
    const char kTabToMoveMouseTo[] = "Tab to move mouse to";
    return Steps(
        NameDescendantViewByType<VerticalTabView>(kBrowserViewElementId,
                                                  kTabToMoveMouseTo, tab_index),
        WithView(
            kTabToMoveMouseTo, base::BindOnce([](views::View* view) {
              const gfx::Point point = view->GetBoundsInScreen().CenterPoint();
              ASSERT_TRUE(ui_controls::SendMouseMove(point.x(), point.y()));
            })));
  }

  BrowserView& GetBrowserView() {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    EXPECT_TRUE(browser_view != nullptr);
    return *browser_view;
  }

  BrowserWindowInterface& GetLatestBrowser() {
    CHECK(!GlobalBrowserCollection::GetInstance()->IsEmpty());
    BrowserWindowInterface* browser = *(--BrowserList::GetInstance()->end());
    CHECK(browser);
    return *browser;
  }
};

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTab);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kThirdTab);
DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<size_t>,
                                    kBrowserCountPoller);
DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<bool>,
                                    kDragStatePoller);
DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<URLs>,
                                    kTabOrderPoller);

// TODO(crbug.com/40249472): Tab DnD tests not working on ChromeOS and Mac, and
// flakes on Wayland
#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_CHROMEOS) && \
    !BUILDFLAG(IS_OZONE_WAYLAND)
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
// flakes on Wayland
#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_CHROMEOS) && \
    !BUILDFLAG(IS_OZONE_WAYLAND)
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
#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_CHROMEOS) && \
    !BUILDFLAG(IS_OZONE_WAYLAND)
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
      WaitForState(kBrowserCountPoller, 2), MoveMouseToTabAsync(1),
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
#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_CHROMEOS) && \
    !BUILDFLAG(IS_OZONE_WAYLAND)
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

      MoveMouseToTabAsync(1),
      WaitForState(kTabOrderPoller,
                   URLs({url::kAboutBlankURL, chrome::kChromeUISettingsURL,
                         chrome::kChromeUIBookmarksURL})),

      MoveMouseToTabAsync(0),
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
#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_CHROMEOS) && \
    !BUILDFLAG(IS_OZONE_WAYLAND)
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
      DragTabTo(2, GetBrowserView().GetBoundsInScreen().top_right() +
                       gfx::Vector2d(50, 50)),
      PollState(kDragStatePoller, GetDragActive()),
      WaitForState(kDragStatePoller, true),

      // Move mouse over the last tab and check tab ordering.
      MoveMouseToTabAsync(1), PollState(kBrowserCountPoller, GetBrowserCount()),
      WaitForState(kBrowserCountPoller, 1),
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
