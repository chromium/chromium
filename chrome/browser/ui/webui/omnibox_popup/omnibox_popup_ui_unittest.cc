// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_ui.h"

#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/contextual_search/contextual_search_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/webui/cr_components/searchbox/searchbox_handler.h"
#include "chrome/browser/ui/webui/omnibox_popup/full_webui_omnibox_layout_helper.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_web_contents_helper.h"
#include "chrome/browser/ui/webui/theme_colors_source_manager.h"
#include "chrome/browser/ui/webui/theme_colors_source_manager_factory.h"
#include "chrome/browser/ui/webui/webui_toolbar/webui_toolbar_layout_css_helper.h"
#include "chrome/common/channel_info.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/contextual_search/contextual_search_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/omnibox/browser/test_omnibox_client.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/content_features.h"
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
  EXPECT_EQ(OmniboxPopupUI::GetProfileLoadTimeData(profile()).FindBool(
                "composeboxContextDragAndDropEnabled"),
            false);

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

// The in-process theme colors depend on kWebUIInProcessResourceLoadingV2,
// which some bots disable on the command line, so pin it either way.
class OmniboxPopupUIInProcessColorsTest
    : public OmniboxPopupUITest,
      public testing::WithParamInterface<bool> {
 public:
  OmniboxPopupUIInProcessColorsTest() {
    feature_list_.InitWithFeatureState(
        features::kWebUIInProcessResourceLoadingV2, GetParam());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         OmniboxPopupUIInProcessColorsTest,
                         testing::Bool());

TEST_P(OmniboxPopupUIInProcessColorsTest, PopulateLocalResourceLoaderConfig) {
  ui::ColorProvider color_provider;
  auto* theme_colors_manager =
      ThemeColorsSourceManagerFactory::GetForProfile(profile());
  ASSERT_EQ(theme_colors_manager != nullptr, GetParam());
  if (theme_colors_manager) {
    theme_colors_manager->SetColorProviderForTesting(&color_provider);
  }

  content::TestWebUI web_ui;
  web_ui.set_web_contents(web_contents());
  auto omnibox_popup_ui = std::make_unique<OmniboxPopupUI>(&web_ui);

  blink::mojom::LocalResourceLoaderConfig config;
  omnibox_popup_ui->PopulateLocalResourceLoaderConfig(
      &config, url::Origin::Create(GURL("chrome://omnibox-popup.top-chrome/")));

  auto source_it =
      config.sources.find(url::Origin::Create(GURL("chrome://theme/")));
  if (!GetParam()) {
    // Without the feature the stylesheet is fetched over the network.
    EXPECT_EQ(source_it, config.sources.end());
    return;
  }
  ASSERT_NE(source_it, config.sources.end());
  auto resource_it =
      source_it->second->path_to_resource_map.find("colors.css?sets=ui,chrome");
  ASSERT_NE(resource_it, source_it->second->path_to_resource_map.end());
  EXPECT_TRUE(resource_it->second->is_response_body());

  theme_colors_manager->SetColorProviderForTesting(nullptr);
}

class OmniboxPopupUIInProcessLayoutCssTest
    : public OmniboxPopupUITest,
      public testing::WithParamInterface<bool> {
 public:
  OmniboxPopupUIInProcessLayoutCssTest() {
    feature_list_.InitWithFeatureState(
        omnibox::internal::kWebUIOmniboxFullPopup, GetParam());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  views::test::TestLayoutProvider layout_provider_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         OmniboxPopupUIInProcessLayoutCssTest,
                         testing::Bool());

TEST_P(OmniboxPopupUIInProcessLayoutCssTest,
       PopulateLocalResourceLoaderConfig) {
  content::TestWebUI web_ui;
  web_ui.set_web_contents(web_contents());
  auto omnibox_popup_ui = std::make_unique<OmniboxPopupUI>(&web_ui);

  const url::Origin popup_origin =
      url::Origin::Create(GURL("chrome://omnibox-popup.top-chrome/"));
  blink::mojom::LocalResourceLoaderConfig config;
  omnibox_popup_ui->PopulateLocalResourceLoaderConfig(&config, popup_origin);

  auto popup_source_it = config.sources.find(popup_origin);
  if (!GetParam()) {
    EXPECT_EQ(popup_source_it, config.sources.end());
    return;
  }
  ASSERT_NE(popup_source_it, config.sources.end());
  auto layout_css_it = popup_source_it->second->path_to_resource_map.find(
      "layout_constants_v0.css");
  ASSERT_NE(layout_css_it, popup_source_it->second->path_to_resource_map.end());
  ASSERT_TRUE(layout_css_it->second->is_response_body());
  EXPECT_EQ(layout_css_it->second->get_response_body(),
            WebUIToolbarLayoutCssHelper::GenerateLayoutConstantsCss());
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
  EXPECT_FALSE(dict.contains("locationBarHeight"));
  EXPECT_FALSE(dict.contains("locationBarPageInfoIconVerticalPadding"));
  EXPECT_FALSE(dict.contains("locationBarIconSize"));
  EXPECT_FALSE(dict.contains("locationBarFontSize"));
}

TEST_F(OmniboxPopupUITest, GetProfileLoadTimeData) {
  base::DictValue dict = OmniboxPopupUI::GetProfileLoadTimeData(profile());

  // The searchbox values that depend on the profile.
  for (const auto item :
       SearchboxHandler::GetProfileDependentWebUIDataSourceDict(profile(),
                                                                {})) {
    EXPECT_TRUE(dict.contains(item.first)) << item.first;
  }
  // The popup's own values that depend on the profile.
  for (std::string_view key :
       {"searchboxComposeButtonIcon", "searchboxDefaultIcon",
        "composeboxShowChip", "composeboxShowCurrentTabChip"}) {
    EXPECT_TRUE(dict.contains(key)) << key;
  }
}
