// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/organizer_panel/organizer_panel_page_handler.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/organizer/organizer_panel_controller.h"
#include "chrome/browser/ui/tabs/organizer/organizer_panel_utils.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/organizer_panel/organizer_panel.mojom.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller_test_support.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_preload_manager.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_preload_manager_test_api.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_web_ui.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

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
    Notify(vertical_tabs_enabled_changed_quit_closure_);
  }

  void OnIsInTrayViewChanged(bool in_tray_view) override {
    is_in_tray_view_ = in_tray_view;
    Notify(is_in_tray_view_changed_quit_closure_);
  }

  void OnSectionsStateChanged(
      base::flat_map<std::string, organizer_panel::mojom::SectionStatePtr>
          sections_state) override {
    sections_state_ = std::move(sections_state);
    Notify(sections_state_changed_quit_closure_);
  }

  void WaitForVerticalTabsEnabledChanged() {
    WaitFor(vertical_tabs_enabled_changed_quit_closure_);
  }

  void WaitForIsInTrayViewChanged() {
    WaitFor(is_in_tray_view_changed_quit_closure_);
  }

  void WaitForSectionsStateChanged() {
    WaitFor(sections_state_changed_quit_closure_);
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
  static void Notify(base::OnceClosure& quit_closure) {
    if (quit_closure) {
      std::move(quit_closure).Run();
    }
  }

  static void WaitFor(base::OnceClosure& quit_closure) {
    base::RunLoop run_loop;
    quit_closure = run_loop.QuitClosure();
    run_loop.Run();
  }

  mojo::Receiver<organizer_panel::mojom::Page> receiver_{this};
  std::optional<bool> vertical_tabs_enabled_;
  std::optional<bool> is_in_tray_view_;
  base::flat_map<std::string, organizer_panel::mojom::SectionStatePtr>
      sections_state_;
  base::OnceClosure vertical_tabs_enabled_changed_quit_closure_;
  base::OnceClosure is_in_tray_view_changed_quit_closure_;
  base::OnceClosure sections_state_changed_quit_closure_;
};

class TestTopChromeWebUIController : public TopChromeWebUIController {
 public:
  explicit TestTopChromeWebUIController(content::WebUI* web_ui)
      : TopChromeWebUIController(web_ui) {}
};

class TestHandlerHarness {
 public:
  explicit TestHandlerHarness(content::WebContents* web_contents,
                              TestEmbedder* embedder = nullptr) {
    web_ui_.set_web_contents(web_contents);
    webui_controller_.emplace(&web_ui_);
    if (embedder) {
      webui_controller_->set_embedder(embedder->GetWeakPtr());
    }
    handler_.emplace(handler_remote_.BindNewPipeAndPassReceiver(),
                     page_.BindAndPassRemote(), web_contents,
                     &*webui_controller_);
  }

  FakeOrganizerPanelPage& page() { return page_; }
  mojo::Remote<organizer_panel::mojom::PageHandler>& handler_remote() {
    return handler_remote_;
  }

  organizer_panel::mojom::SectionStatePtr GetSectionState(
      const std::string& section_id) {
    base::test::TestFuture<organizer_panel::mojom::SectionStatePtr> future;
    handler_remote_->GetSectionState(section_id, future.GetCallback());
    return future.Take();
  }

 private:
  FakeOrganizerPanelPage page_;
  mojo::Remote<organizer_panel::mojom::PageHandler> handler_remote_;
  content::TestWebUI web_ui_;
  std::optional<TestTopChromeWebUIController> webui_controller_;
  std::optional<OrganizerPanelPageHandler> handler_;
};

class VisibilityWaiter : public content::WebContentsObserver {
 public:
  explicit VisibilityWaiter(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {}

  void WaitForVisibility(content::Visibility target) {
    if (web_contents()->GetVisibility() == target) {
      return;
    }
    target_ = target;
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void OnVisibilityChanged(content::Visibility visibility) override {
    if (visibility == target_ && quit_closure_) {
      std::move(quit_closure_).Run();
    }
  }

 private:
  content::Visibility target_ = content::Visibility::HIDDEN;
  base::OnceClosure quit_closure_;
};

class OrganizerPanelPageHandlerBrowserTest : public InProcessBrowserTest {
 public:
  OrganizerPanelPageHandlerBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(organizer_panel::kOrganizerPanel);
  }

 protected:
  content::WebContents* active_web_contents() {
    return browser()->GetTabStripModel()->GetActiveWebContents();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(OrganizerPanelPageHandlerBrowserTest,
                       ClosePanelHidesOrganizerPanelAndRecordsNoAction) {
  base::HistogramTester histogram_tester;
  auto* controller = OrganizerPanelController::From(browser());
  ASSERT_TRUE(controller);

  controller->SetOrganizerVisible(true);
  ASSERT_TRUE(controller->IsOrganizerPanelVisible());

  TestHandlerHarness harness(active_web_contents());

  harness.handler_remote()->ClosePanel();
  harness.handler_remote().FlushForTesting();

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
  TestHandlerHarness harness(active_web_contents());

  EXPECT_EQ(organizer_panel::mojom::SectionState::New(true, false),
            harness.GetSectionState("open-tabs"));

  harness.handler_remote()->SetSectionExpanded("open-tabs", false);
  harness.page().WaitForSectionsStateChanged();

  const base::DictValue& sections_expanded =
      browser()->GetProfile()->GetPrefs()->GetDict(
          prefs::kOrganizerPanelSectionsExpanded);
  EXPECT_EQ(false, sections_expanded.FindBool("open-tabs"));
  EXPECT_THAT(
      harness.page().sections_state(),
      testing::ElementsAre(
          testing::Pair("cross-device-tabs", IsSectionState(true, false)),
          testing::Pair("open-tabs", IsSectionState(false, false)),
          testing::Pair("recently-closed", IsSectionState(true, false)),
          testing::Pair("tab-groups", IsSectionState(true, false))));

  EXPECT_EQ(organizer_panel::mojom::SectionState::New(false, false),
            harness.GetSectionState("open-tabs"));
  EXPECT_EQ(organizer_panel::mojom::SectionState::New(true, false),
            harness.GetSectionState("tab-groups"));

  {
    ScopedDictPrefUpdate update(browser()->GetProfile()->GetPrefs(),
                                prefs::kOrganizerPanelSectionsExpanded);
    update->Set("tab-groups", false);
  }
  harness.page().WaitForSectionsStateChanged();
  EXPECT_THAT(
      harness.page().sections_state(),
      testing::ElementsAre(
          testing::Pair("cross-device-tabs", IsSectionState(true, false)),
          testing::Pair("open-tabs", IsSectionState(false, false)),
          testing::Pair("recently-closed", IsSectionState(true, false)),
          testing::Pair("tab-groups", IsSectionState(false, false))));
}

IN_PROC_BROWSER_TEST_F(OrganizerPanelPageHandlerBrowserTest,
                       GetAndSetSectionShowAllUpdatesPrefAndNotifiesPage) {
  TestHandlerHarness harness(active_web_contents());

  harness.handler_remote()->SetSectionShowAll("open-tabs", true);
  harness.page().WaitForSectionsStateChanged();

  const base::DictValue& sections_show_all =
      browser()->GetProfile()->GetPrefs()->GetDict(
          prefs::kOrganizerPanelSectionsShowAll);
  EXPECT_EQ(true, sections_show_all.FindBool("open-tabs"));
  EXPECT_THAT(
      harness.page().sections_state(),
      testing::ElementsAre(
          testing::Pair("cross-device-tabs", IsSectionState(true, false)),
          testing::Pair("open-tabs", IsSectionState(true, true)),
          testing::Pair("recently-closed", IsSectionState(true, false)),
          testing::Pair("tab-groups", IsSectionState(true, false))));

  EXPECT_EQ(organizer_panel::mojom::SectionState::New(true, true),
            harness.GetSectionState("open-tabs"));

  {
    ScopedDictPrefUpdate update(browser()->GetProfile()->GetPrefs(),
                                prefs::kOrganizerPanelSectionsShowAll);
    update->Set("tab-groups", true);
  }
  harness.page().WaitForSectionsStateChanged();
  EXPECT_THAT(
      harness.page().sections_state(),
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

  TestHandlerHarness harness(active_web_contents());

  harness.page().WaitForVerticalTabsEnabledChanged();
  EXPECT_EQ(false, harness.page().vertical_tabs_enabled());

  vt_controller->SetVerticalTabsEnabled(true);
  harness.page().WaitForVerticalTabsEnabledChanged();
  EXPECT_EQ(true, harness.page().vertical_tabs_enabled());

  vt_controller->SetVerticalTabsEnabled(false);
  harness.page().WaitForVerticalTabsEnabledChanged();
  EXPECT_EQ(false, harness.page().vertical_tabs_enabled());
}

IN_PROC_BROWSER_TEST_F(OrganizerPanelPageHandlerBrowserTest,
                       OnIsInTrayViewChangedNotifiesPage) {
  auto* vt_controller = tabs::VerticalTabStripStateController::From(browser());
  ASSERT_TRUE(vt_controller);
  vt_controller->SetVerticalTabsEnabled(false);

  TestHandlerHarness harness(active_web_contents());

  harness.page().WaitForIsInTrayViewChanged();
  EXPECT_EQ(true, harness.page().is_in_tray_view());

  vt_controller->SetVerticalTabsEnabled(true);
  harness.page().WaitForIsInTrayViewChanged();
  EXPECT_EQ(false, harness.page().is_in_tray_view());

  vt_controller->SetExpandOnHoverEnabled(false);
  vt_controller->RequestCollapse(true);
  harness.page().WaitForIsInTrayViewChanged();
  EXPECT_EQ(true, harness.page().is_in_tray_view());
}

IN_PROC_BROWSER_TEST_F(OrganizerPanelPageHandlerBrowserTest,
                       PreloadedContentsAttachesBrowserWindowInterface) {
  auto* vt_controller = tabs::VerticalTabStripStateController::From(browser());
  ASSERT_TRUE(vt_controller);
  vt_controller->SetVerticalTabsEnabled(true);

  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(browser()->GetProfile()));
  TestHandlerHarness harness(web_contents.get());

  // Initially not attached to any BrowserWindowInterface, so page was not
  // notified.
  EXPECT_FALSE(harness.page().vertical_tabs_enabled().has_value());
  EXPECT_FALSE(harness.page().is_in_tray_view().has_value());

  // Attach BrowserWindowInterface to simulated preloaded contents.
  webui::SetBrowserWindowInterface(web_contents.get(), browser());
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return harness.page().vertical_tabs_enabled().has_value() &&
           harness.page().is_in_tray_view().has_value();
  }));
  EXPECT_EQ(true, harness.page().vertical_tabs_enabled());
  EXPECT_EQ(false, harness.page().is_in_tray_view());

  vt_controller->SetVerticalTabsEnabled(false);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return harness.page().vertical_tabs_enabled() == false &&
           harness.page().is_in_tray_view() == true;
  }));
  EXPECT_EQ(false, harness.page().vertical_tabs_enabled());
  EXPECT_EQ(true, harness.page().is_in_tray_view());
}

IN_PROC_BROWSER_TEST_F(OrganizerPanelPageHandlerBrowserTest,
                       ShowUICallsEmbedderShowUI) {
  TestEmbedder embedder;
  TestHandlerHarness harness(active_web_contents(), &embedder);

  harness.handler_remote()->ShowUI();
  harness.handler_remote().FlushForTesting();
  EXPECT_TRUE(embedder.ui_shown());
}

class OrganizerPanelPreloadBrowserTest : public InProcessBrowserTest {
 public:
  OrganizerPanelPreloadBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {organizer_panel::kOrganizerPanel, features::kPreloadTopChromeWebUI,
         features::kPreloadTopChromeWebUIHideWhenReady},
        {});
  }

 protected:
  content::WebContents* PreloadAndWaitUntilHidden(
      WebUIContentsPreloadManagerTestAPI& preload_api) {
    preload_api.PreloadUrl(browser()->GetProfile(),
                           GURL(chrome::kChromeUIOrganizerPanelURL));
    content::WebContents* const preloaded_contents =
        preload_api.GetPreloadedWebContents();
    if (!preloaded_contents) {
      ADD_FAILURE() << "Expected preloaded WebContents";
      return nullptr;
    }
    VisibilityWaiter(preloaded_contents)
        .WaitForVisibility(content::Visibility::HIDDEN);
    return preloaded_contents;
  }

  static void MergeChildProcessHistograms() {
    content::FetchHistogramsFromChildProcesses();
    metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  }

  static void ExpectNoTabSearchCloseHistograms(
      const base::HistogramTester& histogram_tester) {
    histogram_tester.ExpectTotalCount("Tabs.TabSearch.CloseAction2", 0);
    histogram_tester.ExpectTotalCount("Tabs.TabSearch.NumTabsClosedPerInstance",
                                      0);
  }

  static void ExpectNoCloseHistograms(
      const base::HistogramTester& histogram_tester) {
    histogram_tester.ExpectTotalCount("Tabs.Organizer.CloseAction", 0);
    histogram_tester.ExpectTotalCount("Tabs.Organizer.NumTabsClosedPerInstance",
                                      0);
    ExpectNoTabSearchCloseHistograms(histogram_tester);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(OrganizerPanelPreloadBrowserTest,
                       PreloadMarksWebContentsReadyToShowAndHidesIt) {
  base::HistogramTester histogram_tester;
  WebUIContentsPreloadManagerTestAPI preload_api;
  content::WebContents* const preloaded_contents =
      PreloadAndWaitUntilHidden(preload_api);
  ASSERT_TRUE(preloaded_contents);

  ASSERT_EQ("hidden", content::EvalJs(preloaded_contents, R"(
    new Promise(resolve => {
      if (document.visibilityState === 'hidden') {
        resolve(document.visibilityState);
        return;
      }
      document.addEventListener(
          'visibilitychange', () => resolve(document.visibilityState),
          {once: true});
    });
  )"));

  MergeChildProcessHistograms();
  ExpectNoCloseHistograms(histogram_tester);

  WebUIContentsPreloadManager::RequestResult result =
      preload_api.preload_manager()->Request(
          GURL(chrome::kChromeUIOrganizerPanelURL), browser()->GetProfile());
  EXPECT_EQ(preloaded_contents, result.web_contents.get());
  EXPECT_TRUE(result.is_ready_to_show);
}

IN_PROC_BROWSER_TEST_F(OrganizerPanelPreloadBrowserTest,
                       PreloadOpenAndCloseRecordsOrganizerMetricsOnly) {
  base::HistogramTester histogram_tester;
  WebUIContentsPreloadManagerTestAPI preload_api;
  content::WebContents* const preloaded_contents =
      PreloadAndWaitUntilHidden(preload_api);
  ASSERT_TRUE(preloaded_contents);

  auto* const controller = OrganizerPanelController::From(browser());
  ASSERT_TRUE(controller);
  controller->SetOrganizerVisible(true, /*immediate=*/true);
  EXPECT_EQ(preloaded_contents, controller->web_contents());
  VisibilityWaiter visibility_waiter(preloaded_contents);
  visibility_waiter.WaitForVisibility(content::Visibility::VISIBLE);

  MergeChildProcessHistograms();
  ExpectNoCloseHistograms(histogram_tester);

  // Hiding the WebContents directly (e.g. window minimize) while the panel
  // remains open must not record close metrics.
  preloaded_contents->WasHidden();
  visibility_waiter.WaitForVisibility(content::Visibility::HIDDEN);
  MergeChildProcessHistograms();
  ExpectNoCloseHistograms(histogram_tester);

  controller->SetOrganizerVisible(false, /*immediate=*/true);

  MergeChildProcessHistograms();
  histogram_tester.ExpectUniqueSample("Tabs.Organizer.CloseAction",
                                      OrganizerPanelCloseAction::kNoAction, 1);
  histogram_tester.ExpectUniqueSample("Tabs.Organizer.NumTabsClosedPerInstance",
                                      0, 1);
  ExpectNoTabSearchCloseHistograms(histogram_tester);
}
}  // namespace
