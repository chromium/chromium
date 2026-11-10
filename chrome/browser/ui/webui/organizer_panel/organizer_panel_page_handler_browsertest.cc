// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/organizer_panel/organizer_panel_page_handler.h"

#include <optional>
#include <string>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/organizer/organizer_panel_controller.h"
#include "chrome/browser/ui/tabs/organizer/organizer_panel_utils.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/webui/organizer_panel/organizer_panel.mojom.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

auto IsSectionState(bool expanded, bool show_all) {
  return testing::Pointee(
      organizer_panel::mojom::SectionState(expanded, show_all));
}

class FakeOrganizerPanelPage : public organizer_panel::mojom::Page {
 public:
  mojo::PendingRemote<organizer_panel::mojom::Page> BindAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void OnVerticalTabsEnabledChanged(bool vertical_tabs_enabled) override {
    vertical_tabs_enabled_ = vertical_tabs_enabled;
    if (vertical_tabs_enabled_changed_quit_closure_) {
      std::move(vertical_tabs_enabled_changed_quit_closure_).Run();
    }
  }

  void OnIsInTrayViewChanged(bool in_tray_view) override {
    is_in_tray_view_ = in_tray_view;
    if (is_in_tray_view_changed_quit_closure_) {
      std::move(is_in_tray_view_changed_quit_closure_).Run();
    }
  }

  void OnSectionsStateChanged(
      base::flat_map<std::string, organizer_panel::mojom::SectionStatePtr>
          sections_state) override {
    sections_state_ = std::move(sections_state);
    if (quit_closure_) {
      std::move(quit_closure_).Run();
    }
  }

  void WaitForVerticalTabsEnabledChanged() {
    base::RunLoop run_loop;
    vertical_tabs_enabled_changed_quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void WaitForIsInTrayViewChanged() {
    base::RunLoop run_loop;
    is_in_tray_view_changed_quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void WaitForSectionsStateChanged() {
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  std::optional<bool> vertical_tabs_enabled() const {
    return vertical_tabs_enabled_;
  }

  std::optional<bool> is_in_tray_view() const { return is_in_tray_view_; }

  const base::flat_map<std::string, organizer_panel::mojom::SectionStatePtr>&
  sections_state() const {
    return sections_state_;
  }

 private:
  mojo::Receiver<organizer_panel::mojom::Page> receiver_{this};
  std::optional<bool> vertical_tabs_enabled_;
  std::optional<bool> is_in_tray_view_;
  base::flat_map<std::string, organizer_panel::mojom::SectionStatePtr>
      sections_state_;
  base::OnceClosure vertical_tabs_enabled_changed_quit_closure_;
  base::OnceClosure is_in_tray_view_changed_quit_closure_;
  base::OnceClosure quit_closure_;
};

class OrganizerPanelPageHandlerBrowserTest : public InProcessBrowserTest {
 public:
  OrganizerPanelPageHandlerBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(organizer_panel::kOrganizerPanel);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(OrganizerPanelPageHandlerBrowserTest,
                       ClosePanelHidesOrganizerPanel) {
  base::HistogramTester histogram_tester;
  auto* controller = OrganizerPanelController::From(browser());
  ASSERT_TRUE(controller);

  controller->SetOrganizerVisible(true);
  ASSERT_TRUE(controller->IsOrganizerPanelVisible());

  FakeOrganizerPanelPage page;
  mojo::Remote<organizer_panel::mojom::PageHandler> handler_remote;
  OrganizerPanelPageHandler handler(
      handler_remote.BindNewPipeAndPassReceiver(), page.BindAndPassRemote(),
      browser()->GetTabStripModel()->GetActiveWebContents());

  handler_remote->ClosePanel();
  handler_remote.FlushForTesting();

  EXPECT_FALSE(controller->IsOrganizerPanelVisible());
  histogram_tester.ExpectUniqueSample("Tabs.Organizer.CloseAction",
                                      OrganizerPanelCloseAction::kNoAction, 1);
  histogram_tester.ExpectUniqueSample("Tabs.Organizer.NumTabsClosedPerInstance",
                                      0, 1);
}

IN_PROC_BROWSER_TEST_F(OrganizerPanelPageHandlerBrowserTest,
                       ClosingBrowserWindowWhilePanelOpenRecordsMetrics) {
  base::HistogramTester histogram_tester;
  BrowserWindowInterface* const second_browser =
      CreateBrowser(browser()->GetProfile());
  ASSERT_TRUE(second_browser);

  auto* const controller = OrganizerPanelController::From(second_browser);
  ASSERT_TRUE(controller);
  controller->SetOrganizerVisible(true, /*immediate=*/true);
  ASSERT_TRUE(controller->IsOrganizerPanelVisible());
  controller->SetCloseAction(OrganizerPanelCloseAction::kClosedTab);
  controller->IncrementNumTabsClosed(2);

  histogram_tester.ExpectTotalCount("Tabs.Organizer.CloseAction", 0);
  histogram_tester.ExpectTotalCount("Tabs.Organizer.NumTabsClosedPerInstance",
                                    0);

  CloseBrowserSynchronously(second_browser);

  histogram_tester.ExpectUniqueSample("Tabs.Organizer.CloseAction",
                                      OrganizerPanelCloseAction::kClosedTab, 1);
  histogram_tester.ExpectUniqueSample("Tabs.Organizer.NumTabsClosedPerInstance",
                                      2, 1);
}

IN_PROC_BROWSER_TEST_F(OrganizerPanelPageHandlerBrowserTest,
                       GetAndSetSectionExpandedUpdatesPrefAndNotifiesPage) {
  FakeOrganizerPanelPage page;
  mojo::Remote<organizer_panel::mojom::PageHandler> handler_remote;
  OrganizerPanelPageHandler handler(
      handler_remote.BindNewPipeAndPassReceiver(), page.BindAndPassRemote(),
      browser()->GetTabStripModel()->GetActiveWebContents());

  {
    base::test::TestFuture<organizer_panel::mojom::SectionStatePtr> future;
    handler_remote->GetSectionState("open-tabs", future.GetCallback());
    EXPECT_EQ(organizer_panel::mojom::SectionState::New(true, false),
              future.Get());
  }

  handler_remote->SetSectionExpanded("open-tabs", false);
  page.WaitForSectionsStateChanged();

  const base::DictValue& sections_expanded =
      browser()->GetProfile()->GetPrefs()->GetDict(
          prefs::kOrganizerPanelSectionsExpanded);
  EXPECT_EQ(false, sections_expanded.FindBool("open-tabs"));
  EXPECT_THAT(
      page.sections_state(),
      testing::ElementsAre(
          testing::Pair("cross-device-tabs", IsSectionState(true, false)),
          testing::Pair("open-tabs", IsSectionState(false, false)),
          testing::Pair("recently-closed", IsSectionState(true, false)),
          testing::Pair("tab-groups", IsSectionState(true, false))));

  {
    base::test::TestFuture<organizer_panel::mojom::SectionStatePtr> future;
    handler_remote->GetSectionState("open-tabs", future.GetCallback());
    EXPECT_EQ(organizer_panel::mojom::SectionState::New(false, false),
              future.Get());
  }
  {
    base::test::TestFuture<organizer_panel::mojom::SectionStatePtr> future;
    handler_remote->GetSectionState("tab-groups", future.GetCallback());
    EXPECT_EQ(organizer_panel::mojom::SectionState::New(true, false),
              future.Get());
  }

  {
    ScopedDictPrefUpdate update(browser()->GetProfile()->GetPrefs(),
                                prefs::kOrganizerPanelSectionsExpanded);
    update->Set("tab-groups", false);
  }
  page.WaitForSectionsStateChanged();
  EXPECT_THAT(
      page.sections_state(),
      testing::ElementsAre(
          testing::Pair("cross-device-tabs", IsSectionState(true, false)),
          testing::Pair("open-tabs", IsSectionState(false, false)),
          testing::Pair("recently-closed", IsSectionState(true, false)),
          testing::Pair("tab-groups", IsSectionState(false, false))));
}

IN_PROC_BROWSER_TEST_F(OrganizerPanelPageHandlerBrowserTest,
                       GetAndSetSectionShowAllUpdatesPrefAndNotifiesPage) {
  FakeOrganizerPanelPage page;
  mojo::Remote<organizer_panel::mojom::PageHandler> handler_remote;
  OrganizerPanelPageHandler handler(
      handler_remote.BindNewPipeAndPassReceiver(), page.BindAndPassRemote(),
      browser()->GetTabStripModel()->GetActiveWebContents());

  handler_remote->SetSectionShowAll("open-tabs", true);
  page.WaitForSectionsStateChanged();

  const base::DictValue& sections_show_all =
      browser()->GetProfile()->GetPrefs()->GetDict(
          prefs::kOrganizerPanelSectionsShowAll);
  EXPECT_EQ(true, sections_show_all.FindBool("open-tabs"));
  EXPECT_THAT(
      page.sections_state(),
      testing::ElementsAre(
          testing::Pair("cross-device-tabs", IsSectionState(true, false)),
          testing::Pair("open-tabs", IsSectionState(true, true)),
          testing::Pair("recently-closed", IsSectionState(true, false)),
          testing::Pair("tab-groups", IsSectionState(true, false))));

  {
    base::test::TestFuture<organizer_panel::mojom::SectionStatePtr> future;
    handler_remote->GetSectionState("open-tabs", future.GetCallback());
    EXPECT_EQ(organizer_panel::mojom::SectionState::New(true, true),
              future.Get());
  }

  {
    ScopedDictPrefUpdate update(browser()->GetProfile()->GetPrefs(),
                                prefs::kOrganizerPanelSectionsShowAll);
    update->Set("tab-groups", true);
  }
  page.WaitForSectionsStateChanged();
  EXPECT_THAT(
      page.sections_state(),
      testing::ElementsAre(
          testing::Pair("cross-device-tabs", IsSectionState(true, false)),
          testing::Pair("open-tabs", IsSectionState(true, true)),
          testing::Pair("recently-closed", IsSectionState(true, false)),
          testing::Pair("tab-groups", IsSectionState(true, true))));
}

IN_PROC_BROWSER_TEST_F(OrganizerPanelPageHandlerBrowserTest,
                       OnVerticalTabsEnabledChangedNotifiesPage) {
  auto* vt_controller = tabs::VerticalTabStripStateController::From(browser());
  ASSERT_TRUE(vt_controller);
  vt_controller->SetVerticalTabsEnabled(false);

  FakeOrganizerPanelPage page;
  mojo::Remote<organizer_panel::mojom::PageHandler> handler_remote;
  OrganizerPanelPageHandler handler(
      handler_remote.BindNewPipeAndPassReceiver(), page.BindAndPassRemote(),
      browser()->GetTabStripModel()->GetActiveWebContents());

  page.WaitForVerticalTabsEnabledChanged();
  EXPECT_EQ(false, page.vertical_tabs_enabled());

  vt_controller->SetVerticalTabsEnabled(true);
  page.WaitForVerticalTabsEnabledChanged();
  EXPECT_EQ(true, page.vertical_tabs_enabled());

  vt_controller->SetVerticalTabsEnabled(false);
  page.WaitForVerticalTabsEnabledChanged();
  EXPECT_EQ(false, page.vertical_tabs_enabled());
}

IN_PROC_BROWSER_TEST_F(OrganizerPanelPageHandlerBrowserTest,
                       OnIsInTrayViewChangedNotifiesPage) {
  auto* vt_controller = tabs::VerticalTabStripStateController::From(browser());
  ASSERT_TRUE(vt_controller);
  vt_controller->SetVerticalTabsEnabled(false);

  FakeOrganizerPanelPage page;
  mojo::Remote<organizer_panel::mojom::PageHandler> handler_remote;
  OrganizerPanelPageHandler handler(
      handler_remote.BindNewPipeAndPassReceiver(), page.BindAndPassRemote(),
      browser()->GetTabStripModel()->GetActiveWebContents());

  page.WaitForIsInTrayViewChanged();
  EXPECT_EQ(true, page.is_in_tray_view());

  vt_controller->SetVerticalTabsEnabled(true);
  page.WaitForIsInTrayViewChanged();
  EXPECT_EQ(false, page.is_in_tray_view());

  vt_controller->SetExpandOnHoverEnabled(false);
  vt_controller->RequestCollapse(true);
  page.WaitForIsInTrayViewChanged();
  EXPECT_EQ(true, page.is_in_tray_view());
}

IN_PROC_BROWSER_TEST_F(OrganizerPanelPageHandlerBrowserTest,
                       PreloadedContentsAttachesBrowserWindowInterface) {
  auto* vt_controller = tabs::VerticalTabStripStateController::From(browser());
  ASSERT_TRUE(vt_controller);
  vt_controller->SetVerticalTabsEnabled(true);

  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(browser()->GetProfile()));

  FakeOrganizerPanelPage page;
  mojo::Remote<organizer_panel::mojom::PageHandler> handler_remote;
  OrganizerPanelPageHandler handler(handler_remote.BindNewPipeAndPassReceiver(),
                                    page.BindAndPassRemote(),
                                    web_contents.get());

  // Initially not attached to any BrowserWindowInterface, so page was not
  // notified.
  EXPECT_FALSE(page.vertical_tabs_enabled().has_value());
  EXPECT_FALSE(page.is_in_tray_view().has_value());

  // Attach BrowserWindowInterface to simulated preloaded contents.
  webui::SetBrowserWindowInterface(web_contents.get(), browser());
  page.WaitForVerticalTabsEnabledChanged();
  if (!page.is_in_tray_view().has_value()) {
    page.WaitForIsInTrayViewChanged();
  }
  EXPECT_EQ(true, page.vertical_tabs_enabled());
  EXPECT_EQ(false, page.is_in_tray_view());

  vt_controller->SetVerticalTabsEnabled(false);
  page.WaitForVerticalTabsEnabledChanged();
  if (page.is_in_tray_view() != true) {
    page.WaitForIsInTrayViewChanged();
  }
  EXPECT_EQ(false, page.vertical_tabs_enabled());
  EXPECT_EQ(true, page.is_in_tray_view());
}
}  // namespace
