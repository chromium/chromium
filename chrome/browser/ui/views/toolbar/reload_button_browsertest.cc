// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/reload_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "content/public/browser/weak_document_ptr.h"
#include "content/public/test/browser_test.h"

class ReloadButtonBrowserTest : public InProcessBrowserTest {
 public:
  ReloadButtonBrowserTest() = default;

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }
};

// TODO(crbug.com/40853146): Fix flakiness on Win and Mac.
// TODO(crbug.com/41481789): Fix consistent failing on Linux.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#define MAYBE_AllowExternalProtocols DISABLED_AllowExternalProtocols
#else
#define MAYBE_AllowExternalProtocols AllowExternalProtocols
#endif
IN_PROC_BROWSER_TEST_F(ReloadButtonBrowserTest, MAYBE_AllowExternalProtocols) {
  const char fake_protocol[] = "fake";

  // Call LaunchUrl once to trigger the blocked state.
  GURL url("fake://example.test");
  ExternalProtocolHandler::LaunchUrl(
      url,
      base::BindRepeating(&ReloadButtonBrowserTest::GetWebContents,
                          base::Unretained(this)),
      ui::PAGE_TRANSITION_LINK, /*has_user_gesture=*/true,
      /*is_in_fenced_frame_tree=*/false, url::Origin::Create(url),
      content::WeakDocumentPtr());
  ASSERT_EQ(ExternalProtocolHandler::BLOCK,
            ExternalProtocolHandler::GetBlockState(fake_protocol, nullptr,
                                                   browser()->profile()));

  // Clicking the reload button should remove the blocked state.
  ui_test_utils::ClickOnView(browser(), VIEW_ID_RELOAD_BUTTON);
  ASSERT_NE(ExternalProtocolHandler::BLOCK,
            ExternalProtocolHandler::GetBlockState(fake_protocol, nullptr,
                                                   browser()->profile()));
}
