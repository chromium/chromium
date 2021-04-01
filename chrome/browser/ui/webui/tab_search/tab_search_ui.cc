// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/tab_search/tab_search_ui.h"

#include "base/metrics/histogram_functions.h"
#include "base/numerics/ranges.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/tab_search_resources.h"
#include "chrome/grit/tab_search_resources_map.h"
#include "components/favicon_base/favicon_url_parser.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/views/style/platform_style.h"

TabSearchUI::TabSearchUI(content::WebUI* web_ui)
    : ui::MojoBubbleWebUIController(web_ui,
                                    true /* Needed for webui browser tests */),
      webui_load_timer_(web_ui->GetWebContents(),
                        "Tabs.TabSearch.WebUI.LoadDocumentTime",
                        "Tabs.TabSearch.WebUI.LoadCompletedTime") {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUITabSearchHost);
  static constexpr webui::LocalizedString kStrings[] = {
      {"clearSearch", IDS_CLEAR_SEARCH},
      {"searchTabs", IDS_TAB_SEARCH_SEARCH_TABS},
      {"noResultsFound", IDS_TAB_SEARCH_NO_RESULTS_FOUND},
      {"closeTab", IDS_TAB_SEARCH_CLOSE_TAB},
      {"a11yTabClosed", IDS_TAB_SEARCH_A11Y_TAB_CLOSED},
      {"a11yFoundTab", IDS_TAB_SEARCH_A11Y_FOUND_TAB},
      {"a11yFoundTabs", IDS_TAB_SEARCH_A11Y_FOUND_TABS},
      {"a11yFoundTabFor", IDS_TAB_SEARCH_A11Y_FOUND_TAB_FOR},
      {"a11yFoundTabsFor", IDS_TAB_SEARCH_A11Y_FOUND_TABS_FOR},
      {"openTabs", IDS_TAB_SEARCH_OPEN_TABS},
      {"recentlyClosedTabs", IDS_TAB_SEARCH_RECENTLY_CLOSED_TABS},
  };
  source->AddLocalizedStrings(kStrings);
  source->AddBoolean("useRipples", views::PlatformStyle::kUseRipples);

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

  source->AddBoolean("moveActiveTabToBottom",
                     features::kTabSearchMoveActiveTabToBottom.Get());
  source->AddLocalizedString("close", IDS_CLOSE);

  source->AddInteger(
      "recentlyClosedDefaultItemDisplayCount",
      features::kTabSearchRecentlyClosedDefaultItemDisplayCount.Get());

  ui::Accelerator accelerator(ui::VKEY_A,
                              ui::EF_SHIFT_DOWN | ui::EF_PLATFORM_ACCELERATOR);
  source->AddString("shortcutText", accelerator.GetShortcutText());

  webui::SetupWebUIDataSource(
      source, base::make_span(kTabSearchResources, kTabSearchResourcesSize),
      IDR_TAB_SEARCH_TAB_SEARCH_HTML);
  content::WebUIDataSource::Add(web_ui->GetWebContents()->GetBrowserContext(),
                                source);

  Profile* profile = Profile::FromWebUI(web_ui);
  content::URLDataSource::Add(
      profile, std::make_unique<FaviconSource>(
                   profile, chrome::FaviconUrlFormat::kFavicon2));

  page_handler_timer_ = base::ElapsedTimer();
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(
      "browser", "TabSearchPageHandlerConstructionDelay", this);
}

TabSearchUI::~TabSearchUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(TabSearchUI)

void TabSearchUI::BindInterface(
    mojo::PendingReceiver<tab_search::mojom::PageHandlerFactory> receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void TabSearchUI::CreatePageHandler(
    mojo::PendingRemote<tab_search::mojom::Page> page,
    mojo::PendingReceiver<tab_search::mojom::PageHandler> receiver) {
  DCHECK(page);

  // CreatePageHandler() can be called multiple times if reusing the same
  // WebUIController. For eg refreshing the page will create new PageHandler but
  // reuse TabSearchUI. Check to make sure |page_handler_timer_| is valid before
  // logging metrics.
  if (page_handler_timer_.has_value()) {
    TRACE_EVENT_NESTABLE_ASYNC_END0(
        "browser", "TabSearchPageHandlerConstructionDelay", this);
    UmaHistogramMediumTimes("Tabs.TabSearch.PageHandlerConstructionDelay",
                            page_handler_timer_->Elapsed());
    page_handler_timer_.reset();
  }

  // TODO(tluk): Investigate whether we can avoid recreating this multiple times
  // per instance of the TabSearchUI.
  page_handler_ = std::make_unique<TabSearchPageHandler>(
      std::move(receiver), std::move(page), web_ui(), this);
}
