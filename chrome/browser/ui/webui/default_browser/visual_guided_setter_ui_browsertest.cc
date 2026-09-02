// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/default_browser/visual_guided_setter_ui.h"

#include <memory>
#include <utility>

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/default_browser/visual_guided_setter.mojom.h"
#include "chrome/browser/ui/webui/default_browser/visual_guided_setter_page_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

namespace {

class MockPage : public visual_guided_setter::mojom::Page {
 public:
  MockPage() = default;
  ~MockPage() override = default;

  void SetErrorState(bool has_error) override {}

  mojo::PendingRemote<visual_guided_setter::mojom::Page> BindAndGetRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

 private:
  mojo::Receiver<visual_guided_setter::mojom::Page> receiver_{this};
};

}  // namespace

class VisualGuidedSetterUIBrowserTest : public InProcessBrowserTest {
 protected:
  VisualGuidedSetterUIBrowserTest() = default;
  ~VisualGuidedSetterUIBrowserTest() override = default;
};

IN_PROC_BROWSER_TEST_F(VisualGuidedSetterUIBrowserTest, LoadPageAndCallMojo) {
  GURL url(std::string("chrome://") +
           chrome::kChromeUIDefaultBrowserVisualGuidedSetterHost);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->GetTabStripModel()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  content::WebUI* web_ui = web_contents->GetWebUI();
  ASSERT_TRUE(web_ui);

  VisualGuidedSetterUI* controller =
      web_ui->GetController()->GetAs<VisualGuidedSetterUI>();
  ASSERT_TRUE(controller);

  mojo::Remote<visual_guided_setter::mojom::PageHandlerFactory> factory;
  controller->BindInterface(factory.BindNewPipeAndPassReceiver());

  MockPage page;
  mojo::Remote<visual_guided_setter::mojom::PageHandler> handler;

  factory->CreatePageHandler(page.BindAndGetRemote(),
                             handler.BindNewPipeAndPassReceiver());
  factory.FlushForTesting();
  EXPECT_TRUE(handler.is_connected());

  handler->SetAnchorRect(gfx::Rect(10, 20, 600, 360));
  handler.FlushForTesting();
}
