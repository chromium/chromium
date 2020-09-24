// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/tab_search/tab_search_ui.h"

#include "base/numerics/ranges.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/favicon_base/favicon_url_parser.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/webui/web_ui_util.h"

#if BUILDFLAG(ENABLE_TAB_SEARCH)
#include "chrome/grit/tab_search_resources.h"
#include "chrome/grit/tab_search_resources_map.h"
#endif  // BUILDFLAG(ENABLE_TAB_SEARCH)

#if BUILDFLAG(ENABLE_TAB_SEARCH)
namespace {
constexpr char kGeneratedPath[] =
    "@out_folder@/gen/chrome/browser/resources/tab_search/";
}
#endif  // BUILDFLAG(ENABLE_TAB_SEARCH)

TabSearchUI::TabSearchUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui,
                              true /* Needed for webui browser tests */),
      webui_load_timer_(web_ui->GetWebContents(),
                        "Tabs.TabSearch.WebUI.LoadDocumentTime",
                        "Tabs.TabSearch.WebUI.LoadCompletedTime") {
#if BUILDFLAG(ENABLE_TAB_SEARCH)
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUITabSearchHost);
  static constexpr webui::LocalizedString kStrings[] = {
      {"clearSearch", IDS_CLEAR_SEARCH},
      {"searchTabs", IDS_TAB_SEARCH_SEARCH_TABS},
      {"noResultsFound", IDS_TAB_SEARCH_NO_RESULTS_FOUND},
      {"closeTab", IDS_TAB_SEARCH_CLOSE_TAB},
      {"submitFeedback", IDS_TAB_SEARCH_SUBMIT_FEEDBACK},
      {"a11yTabClosed", IDS_TAB_SEARCH_A11Y_TAB_CLOSED},
      {"a11yFoundTab", IDS_TAB_SEARCH_A11Y_FOUND_TAB},
      {"a11yFoundTabs", IDS_TAB_SEARCH_A11Y_FOUND_TABS},
      {"a11yFoundTabFor", IDS_TAB_SEARCH_A11Y_FOUND_TAB_FOR},
      {"a11yFoundTabsFor", IDS_TAB_SEARCH_A11Y_FOUND_TABS_FOR},
  };
  AddLocalizedStringsBulk(source, kStrings);

  source->AddBoolean(
      "submitFeedbackEnabled",
      base::FeatureList::IsEnabled(features::kTabSearchFeedback));

  // Add the configuration parameters for fuzzy search.
  source->AddBoolean("searchIgnoreLocation",
                     features::kTabSearchSearchIgnoreLocation.Get());
  source->AddInteger("searchDistance",
                     features::kTabSearchSearchDistance.Get());
  source->AddDouble(
      "searchThreshold",
      base::ClampToRange<double>(features::kTabSearchSearchThreshold.Get(),
                                 features::kTabSearchSearchThresholdMin,
                                 features::kTabSearchSearchThresholdMax));
  source->AddDouble("searchTitleToHostnameWeightRatio",
                    features::kTabSearchTitleToHostnameWeightRatio.Get());

  source->AddLocalizedString("close", IDS_CLOSE);
  source->AddResourcePath("tab_search.mojom-lite.js",
                          IDR_TAB_SEARCH_MOJO_LITE_JS);
  source->AddResourcePath("fuse.js", IDR_FUSE_JS);
  webui::SetupWebUIDataSource(
      source, base::make_span(kTabSearchResources, kTabSearchResourcesSize),
      kGeneratedPath, IDR_TAB_SEARCH_PAGE_HTML);
  content::WebUIDataSource::Add(web_ui->GetWebContents()->GetBrowserContext(),
                                source);

  Profile* profile = Profile::FromWebUI(web_ui);
  content::URLDataSource::Add(
      profile, std::make_unique<FaviconSource>(
                   profile, chrome::FaviconUrlFormat::kFavicon2));
#endif  // BUILDFLAG(ENABLE_TAB_SEARCH)
}

TabSearchUI::~TabSearchUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(TabSearchUI)

void TabSearchUI::BindInterface(
    mojo::PendingReceiver<tab_search::mojom::PageHandlerFactory> receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void TabSearchUI::SetEmbedder(TabSearchUIEmbedder* embedder) {
  // Setting the embedder must be done before the page handler is created.
  DCHECK(!embedder || !page_handler_);
  embedder_ = embedder;
}

void TabSearchUI::ShowUI() {
  if (embedder_)
    embedder_->ShowBubble();
}

void TabSearchUI::CloseUI() {
  if (embedder_)
    embedder_->CloseBubble();
}

void TabSearchUI::CreatePageHandler(
    mojo::PendingRemote<tab_search::mojom::Page> page,
    mojo::PendingReceiver<tab_search::mojom::PageHandler> receiver) {
  DCHECK(page);
  page_handler_ = std::make_unique<TabSearchPageHandler>(
      std::move(receiver), std::move(page), web_ui(), this);
}
