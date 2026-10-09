// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_ui.h"

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/contextual_search/contextual_search_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/webui/omnibox_popup/full_webui_omnibox_layout_helper.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_web_contents_helper.h"
#include "chrome/browser/ui/webui/theme_colors_source_manager.h"
#include "chrome/browser/ui/webui/theme_colors_source_manager_factory.h"
#include "chrome/common/channel_info.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/contextual_search/contextual_search_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/omnibox/browser/test_omnibox_client.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/test/test_web_ui.h"
#include "content/public/test/test_web_ui_data_source.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/loader/local_resource_loader_config.mojom.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/test/test_layout_provider.h"
#include "url/gurl.h"
#include "url/origin.h"

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

  auto omnibox_controller = std::make_unique<OmniboxController>(
      std::make_unique<TestOmniboxClient>());
  OmniboxPopupWebContentsHelper::CreateForWebContents(web_contents());
  OmniboxPopupWebContentsHelper::FromWebContents(web_contents())
      ->set_omnibox_controller(omnibox_controller.get());

  content::TestWebUI web_ui;
  web_ui.set_web_contents(web_contents());

  auto omnibox_popup_ui = std::make_unique<OmniboxPopupUI>(&web_ui);

  mojo::PendingReceiver<composebox::mojom::PageHandler> pending_page_handler;
  mojo::PendingRemote<searchbox::mojom::Page> pending_searchbox_page;
  mojo::PendingReceiver<searchbox::mojom::PageHandler>
      pending_searchbox_handler;

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

  omnibox_popup_ui->CreatePageHandler(std::move(pending_page_handler),
                                      std::move(pending_searchbox_page),
                                      std::move(pending_searchbox_handler));

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

  OmniboxPopupWebContentsHelper::FromWebContents(web_contents())
      ->set_omnibox_controller(nullptr);
}

TEST_F(OmniboxPopupUITest, PopulateLocalResourceLoaderConfig) {
  ui::ColorProvider color_provider;
  auto* theme_colors_manager =
      ThemeColorsSourceManagerFactory::GetForProfile(profile());
  ASSERT_NE(theme_colors_manager, nullptr);
  theme_colors_manager->SetColorProviderForTesting(&color_provider);

  content::TestWebUI web_ui;
  web_ui.set_web_contents(web_contents());
  auto omnibox_popup_ui = std::make_unique<OmniboxPopupUI>(&web_ui);

  blink::mojom::LocalResourceLoaderConfig config;
  omnibox_popup_ui->PopulateLocalResourceLoaderConfig(
      &config, url::Origin::Create(GURL("chrome://omnibox-popup.top-chrome/")));

  auto source_it =
      config.sources.find(url::Origin::Create(GURL("chrome://theme/")));
  ASSERT_NE(source_it, config.sources.end());
  auto resource_it =
      source_it->second->path_to_resource_map.find("colors.css?sets=ui,chrome");
  ASSERT_NE(resource_it, source_it->second->path_to_resource_map.end());
  EXPECT_TRUE(resource_it->second->is_response_body());

  theme_colors_manager->SetColorProviderForTesting(nullptr);
}

// Every window builds popups, so the session is created on first use rather
// than with the popup.
TEST_F(OmniboxPopupUITest, CreatesContextualSessionLazily) {
  ASSERT_TRUE(ContextualSearchServiceFactory::GetForProfile(profile()));

  content::TestWebUI web_ui;
  web_ui.set_web_contents(web_contents());
  auto omnibox_popup_ui = std::make_unique<OmniboxPopupUI>(&web_ui);
  EXPECT_FALSE(omnibox_popup_ui->HasContextualSessionHandleForTesting());

  EXPECT_TRUE(omnibox_popup_ui->GetOrCreateContextualSessionHandle());
  EXPECT_TRUE(omnibox_popup_ui->HasContextualSessionHandleForTesting());
}

TEST_F(OmniboxPopupUITest, FullWebUIOmniboxLayoutHelperStandardMode) {
  ui::TouchUiController::TouchUiScoperForTesting touch_ui_scoper(false);
  EXPECT_EQ(FullWebUIOmniboxLayoutHelper::GetLocationBarHeight(), 34);
  EXPECT_EQ(
      FullWebUIOmniboxLayoutHelper::GetLocationBarPageInfoIconVerticalPadding(),
      5);
  EXPECT_EQ(FullWebUIOmniboxLayoutHelper::GetLocationBarIconSize(), 16);
  EXPECT_EQ(FullWebUIOmniboxLayoutHelper::GetFontSize(), 14);
#if BUILDFLAG(IS_MAC)
  EXPECT_EQ(FullWebUIOmniboxLayoutHelper::GetLocationBarAlignmentInsets(),
            gfx::Insets::TLBR(5, 5, 4, 5));
#else
  EXPECT_EQ(FullWebUIOmniboxLayoutHelper::GetLocationBarAlignmentInsets(),
            gfx::Insets::TLBR(5, 6, 5, 6));
#endif
}

TEST_F(OmniboxPopupUITest, FullWebUIOmniboxLayoutHelperTouchUiMode) {
  ui::TouchUiController::TouchUiScoperForTesting touch_ui_scoper(true);
  EXPECT_EQ(FullWebUIOmniboxLayoutHelper::GetLocationBarHeight(), 36);
  EXPECT_EQ(
      FullWebUIOmniboxLayoutHelper::GetLocationBarPageInfoIconVerticalPadding(),
      3);
  EXPECT_EQ(FullWebUIOmniboxLayoutHelper::GetLocationBarIconSize(), 20);
  EXPECT_EQ(FullWebUIOmniboxLayoutHelper::GetFontSize(), 15);
  EXPECT_EQ(FullWebUIOmniboxLayoutHelper::GetLocationBarAlignmentInsets(),
            gfx::Insets::TLBR(6, 1, 5, 1));
}

TEST_F(OmniboxPopupUITest, FullWebUIOmniboxLayoutHelperPopulateLoadTimeData) {
  auto test_source =
      content::TestWebUIDataSource::Create("test-omnibox-source");
  FullWebUIOmniboxLayoutHelper::PopulateLoadTimeData(
      test_source->GetWebUIDataSource());

  const base::DictValue& dict = test_source->GetLocalizedStrings();
  EXPECT_EQ(
      dict.FindInt("alignmentInsetTop"),
      FullWebUIOmniboxLayoutHelper::GetLocationBarAlignmentInsets().top());
  EXPECT_EQ(
      dict.FindInt("alignmentInsetHorizontal"),
      FullWebUIOmniboxLayoutHelper::GetLocationBarAlignmentInsets().left());
  EXPECT_EQ(
      dict.FindInt("alignmentInsetBottom"),
      FullWebUIOmniboxLayoutHelper::GetLocationBarAlignmentInsets().bottom());
  EXPECT_EQ(dict.FindInt("locationBarHeight"),
            FullWebUIOmniboxLayoutHelper::GetLocationBarHeight());
  EXPECT_EQ(dict.FindInt("locationBarPageInfoIconVerticalPadding"),
            FullWebUIOmniboxLayoutHelper::
                GetLocationBarPageInfoIconVerticalPadding());
  EXPECT_EQ(dict.FindInt("locationBarIconSize"),
            FullWebUIOmniboxLayoutHelper::GetLocationBarIconSize());
  EXPECT_EQ(dict.FindInt("locationBarFontSize"),
            FullWebUIOmniboxLayoutHelper::GetFontSize());
}

TEST_F(OmniboxPopupUITest,
       FullWebUIOmniboxLayoutHelperFontSizeWithLayoutProvider) {
  views::test::TestLayoutProvider layout_provider;
  layout_provider.SetFontDetails(
      CONTEXT_OMNIBOX_PRIMARY, views::style::STYLE_PRIMARY,
      ui::ResourceBundle::FontDetails("Roboto", /*size_delta=*/2,
                                      /*weight=*/gfx::Font::Weight::NORMAL));
  EXPECT_EQ(FullWebUIOmniboxLayoutHelper::GetFontSize(),
            views::TypographyProvider::Get()
                .GetFont(CONTEXT_OMNIBOX_PRIMARY, views::style::STYLE_PRIMARY)
                .GetFontSize());
}
