// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_ui.h"

#include "base/memory/raw_ptr.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/contextual_search/contextual_search_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/channel_info.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/contextual_search/contextual_search_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"

class OmniboxPopupUITest : public ChromeRenderViewHostTestHarness {
 public:
  OmniboxPopupUITest() = default;
  ~OmniboxPopupUITest() override = default;

  void SetUp() override { ChromeRenderViewHostTestHarness::SetUp(); }

 private:
  variations::test::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
};

TEST_F(OmniboxPopupUITest, SafeWithNullContextualSearchService) {
  // Force ContextualSearchService to be null.
  ContextualSearchServiceFactory::GetInstance()->SetTestingFactory(
      profile(), base::BindRepeating(
                     [](content::BrowserContext* context)
                         -> std::unique_ptr<KeyedService> { return nullptr; }));

  EXPECT_EQ(ContextualSearchServiceFactory::GetForProfile(profile()), nullptr);

  content::TestWebUI web_ui;
  web_ui.set_web_contents(web_contents());

  auto omnibox_popup_ui = std::make_unique<OmniboxPopupUI>(&web_ui);

  mojo::PendingRemote<composebox::mojom::Page> pending_page;
  mojo::PendingReceiver<composebox::mojom::PageHandler> pending_page_handler;
  mojo::PendingRemote<searchbox::mojom::Page> pending_searchbox_page;
  mojo::PendingReceiver<searchbox::mojom::PageHandler>
      pending_searchbox_handler;

  {
    auto pipe_page = mojo::MessagePipe();
    pending_page = mojo::PendingRemote<composebox::mojom::Page>(
        std::move(pipe_page.handle0), 0);
  }

  {
    auto pipe_handler = mojo::MessagePipe();
    pending_page_handler =
        mojo::PendingReceiver<composebox::mojom::PageHandler>(
            std::move(pipe_handler.handle0));
  }

  {
    auto pipe_sb_page = mojo::MessagePipe();
    pending_searchbox_page = mojo::PendingRemote<searchbox::mojom::Page>(
        std::move(pipe_sb_page.handle0), 0);
  }

  {
    auto pipe_sb_handler = mojo::MessagePipe();
    pending_searchbox_handler =
        mojo::PendingReceiver<searchbox::mojom::PageHandler>(
            std::move(pipe_sb_handler.handle0));
  }

  omnibox_popup_ui->CreatePageHandler(
      std::move(pending_page), std::move(pending_page_handler),
      std::move(pending_searchbox_page), std::move(pending_searchbox_handler));

  mojo::PendingRemote<omnibox_popup::mojom::Page> pending_popup_page;
  mojo::PendingReceiver<omnibox_popup::mojom::PageHandler>
      pending_popup_handler;

  {
    auto pipe_popup_page = mojo::MessagePipe();
    pending_popup_page = mojo::PendingRemote<omnibox_popup::mojom::Page>(
        std::move(pipe_popup_page.handle0), 0);
  }

  {
    auto pipe_popup_handler = mojo::MessagePipe();
    pending_popup_handler =
        mojo::PendingReceiver<omnibox_popup::mojom::PageHandler>(
            std::move(pipe_popup_handler.handle0));
  }

  omnibox_popup_ui->CreatePageHandler(std::move(pending_popup_page),
                                      std::move(pending_popup_handler));

  EXPECT_NE(omnibox_popup_ui->popup_handler(), nullptr);
}
