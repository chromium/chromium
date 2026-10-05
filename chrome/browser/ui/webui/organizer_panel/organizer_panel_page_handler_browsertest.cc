// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/organizer_panel/organizer_panel_page_handler.h"

#include <string>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/organizer/organizer_panel_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/tabs/organizer/organizer_panel_utils.h"
#include "chrome/browser/ui/webui/organizer_panel/organizer_panel.mojom.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class FakeOrganizerPanelPage : public organizer_panel::mojom::Page {
 public:
  mojo::PendingRemote<organizer_panel::mojom::Page> BindAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void OnSectionsExpandedChanged(
      const base::flat_map<std::string, bool>& sections_expanded) override {
    sections_expanded_ = sections_expanded;
    if (quit_closure_) {
      std::move(quit_closure_).Run();
    }
  }

  void WaitForSectionsExpandedChanged() {
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  const base::flat_map<std::string, bool>& sections_expanded() const {
    return sections_expanded_;
  }

 private:
  mojo::Receiver<organizer_panel::mojom::Page> receiver_{this};
  base::flat_map<std::string, bool> sections_expanded_;
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
}

IN_PROC_BROWSER_TEST_F(OrganizerPanelPageHandlerBrowserTest,
                       GetAndSetSectionExpandedUpdatesPrefAndNotifiesPage) {
  FakeOrganizerPanelPage page;
  mojo::Remote<organizer_panel::mojom::PageHandler> handler_remote;
  OrganizerPanelPageHandler handler(
      handler_remote.BindNewPipeAndPassReceiver(), page.BindAndPassRemote(),
      browser()->GetTabStripModel()->GetActiveWebContents());

  {
    base::test::TestFuture<bool> future;
    handler_remote->IsSectionExpanded("open-tabs", future.GetCallback());
    EXPECT_TRUE(future.Get());
  }

  handler_remote->SetSectionExpanded("open-tabs", false);
  page.WaitForSectionsExpandedChanged();

  const base::DictValue& sections_expanded =
      browser()->GetProfile()->GetPrefs()->GetDict(
          prefs::kOrganizerPanelSectionsExpanded);
  EXPECT_EQ(false, sections_expanded.FindBool("open-tabs"));
  EXPECT_THAT(page.sections_expanded(),
              testing::ElementsAre(testing::Pair("open-tabs", false)));

  {
    base::test::TestFuture<bool> future;
    handler_remote->IsSectionExpanded("open-tabs", future.GetCallback());
    EXPECT_FALSE(future.Get());
  }
  {
    base::test::TestFuture<bool> future;
    handler_remote->IsSectionExpanded("tab-groups", future.GetCallback());
    EXPECT_TRUE(future.Get());
  }

  {
    ScopedDictPrefUpdate update(browser()->GetProfile()->GetPrefs(),
                                prefs::kOrganizerPanelSectionsExpanded);
    update->Set("tab-groups", false);
  }
  page.WaitForSectionsExpandedChanged();
  EXPECT_THAT(page.sections_expanded(),
              testing::ElementsAre(testing::Pair("open-tabs", false),
                                   testing::Pair("tab-groups", false)));
}

}  // namespace
