// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"

#include <stdint.h>

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/contextual_search/searchbox_context_data.h"
#include "chrome/browser/ui/omnibox/chrome_omnibox_client.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_state_manager.h"
#include "chrome/browser/ui/omnibox/test_omnibox_edit_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_id.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "url/gurl.h"

using OmniboxEditModelBrowserTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(OmniboxEditModelBrowserTest,
                       OpenComposeboxForAskGPopulatesContext) {
  const GURL expected_url("https://example.com/test-page");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), expected_url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_TRUE(tab);
  int32_t expected_tab_handle = tab->GetHandle().raw_value();
  SessionID session_id = sessions::SessionTabHelper::IdForTab(web_contents);

  // Use a dedicated OmniboxController with ChromeOmniboxClient bound to
  // browser(). This isolates the test from live LocationBarView UI popup
  // presenters that drain pending context on popup state changes.
  auto omnibox_client = std::make_unique<ChromeOmniboxClient>(
      /*location_bar=*/nullptr, browser(), browser()->GetProfile());
  auto controller =
      std::make_unique<OmniboxController>(std::move(omnibox_client));
  auto edit_model = std::make_unique<TestOmniboxEditModel>(
      controller.get(), browser()->GetProfile()->GetPrefs());
  OmniboxEditModel* model = edit_model.get();
  controller->SetEditModelForTesting(std::move(edit_model));

  // Trigger the AskG flow.
  model->OpenComposeboxForAskG();

  // Verify the popup state is correct.
  EXPECT_EQ(controller->popup_state_manager()->popup_state(),
            OmniboxPopupState::kAim);

  // Verify context was populated correctly with TabHandle (not SessionID).
  SearchboxContextData* context_data = SearchboxContextData::From(browser());
  ASSERT_TRUE(context_data);

  std::unique_ptr<SearchboxContextData::Context> context =
      context_data->TakePendingContext();
  ASSERT_TRUE(context);
  ASSERT_EQ(context->file_infos.size(), 1u);

  const auto& attachment = context->file_infos[0];
  ASSERT_TRUE(attachment->is_tab_attachment());

  const auto& tab_attachment = attachment->get_tab_attachment();
  EXPECT_EQ(tab_attachment->tab_id, expected_tab_handle);
  EXPECT_NE(tab_attachment->tab_id, session_id.id());
  EXPECT_EQ(tab_attachment->url, expected_url);
}
