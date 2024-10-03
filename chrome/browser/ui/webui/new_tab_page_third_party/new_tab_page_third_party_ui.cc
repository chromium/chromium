// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/new_tab_page_third_party/new_tab_page_third_party_ui.h"

#include <memory>
#include <utility>

#include "chrome/browser/browser_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/webui/cr_components/most_visited/most_visited_handler.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/new_tab_page_third_party/new_tab_page_third_party_handler.h"
#include "chrome/browser/ui/webui/ntp/ntp_resource_cache.h"
#include "chrome/browser/ui/webui/page_not_available_for_guest/page_not_available_for_guest_ui.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/browser/ui/webui/webui_util_desktop.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/new_tab_page_third_party_resources.h"
#include "chrome/grit/new_tab_page_third_party_resources_map.h"
#include "chrome/grit/theme_resources.h"
#include "components/favicon_base/favicon_url_parser.h"
#include "components/prefs/pref_service.h"
#include "components/search/ntp_features.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_utils.h"
#include "ui/resources/grit/webui_resources.h"
#include "url/url_util.h"

using content::BrowserContext;
using content::WebContents;

bool NewTabPageThirdPartyUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  return !profile->IsOffTheRecord();
}

std::unique_ptr<content::WebUIController>
NewTabPageThirdPartyUIConfig::CreateWebUIController(content::WebUI* web_ui,
                                                    const GURL& url) {
  Profile* profile = Profile::FromWebUI(web_ui);
  if (profile->IsGuestSession()) {
    return std::make_unique<PageNotAvailableForGuestUI>(
        web_ui, chrome::kChromeUINewTabPageThirdPartyHost);
  }
  return std::make_unique<NewTabPageThirdPartyUI>(web_ui);
}

namespace {
void CreateAndAddNewTabPageThirdPartyUiHtmlSource(Profile* profile,
                                                  WebContents* web_contents) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUINewTabPageThirdPartyHost);
  ui::Accelerator undo_accelerator(ui::VKEY_Z, ui::EF_PLATFORM_ACCELERATOR);
  source->AddString("undoDescription", l10n_util::GetStringFUTF16(
                                           IDS_UNDO_DESCRIPTION,
                                           undo_accelerator.GetShortcutText()));
  static constexpr webui::LocalizedString kStrings[] = {
      {"linkRemove", IDS_NTP_CUSTOM_LINKS_REMOVE},
      {"linkRemovedMsg", IDS_NTP_CONFIRM_MSG_SHORTCUT_REMOVED},
      {"restoreThumbnailsShort", IDS_NEW_TAB_RESTORE_THUMBNAILS_SHORT_LINK},
      {"title", IDS_NEW_TAB_TITLE},
      {"undo", IDS_NEW_TAB_UNDO_THUMBNAIL_REMOVE},
  };

  source->AddLocalizedStrings(kStrings);

  const ui::ThemeProvider* theme_provider =
      webui::GetThemeProviderDeprecated(web_contents);
  // TODO(crbug.com/40823895): Always mock theme provider in tests so that
  // `theme_provider` is never nullptr.
  if (theme_provider) {
    const ui::ColorProvider& color_provider = web_contents->GetColorProvider();
    source->AddString("backgroundPosition",
                      GetNewTabBackgroundPositionCSS(*theme_provider));
    source->AddString("backgroundTiling",
                      GetNewTabBackgroundTilingCSS(*theme_provider));
    source->AddString("colorBackground",
                      color_utils::SkColorToRgbaString(GetThemeColor(
                          webui::GetNativeThemeDeprecated(web_contents),
                          color_provider, kColorNewTabPageBackground)));
    // TODO(crbug.com/40120448): don't get theme id from profile.
    source->AddString("themeId",
                      profile->GetPrefs()->GetString(prefs::kCurrentThemeID));
    source->AddString("hascustombackground",
                      theme_provider->HasCustomImage(IDR_THEME_NTP_BACKGROUND)
                          ? "has-custom-background"
                          : "");
    source->AddString(
        "isdark",
        !color_utils::IsDark(color_provider.GetColor(kColorNewTabPageText))
            ? "dark"
            : "");
  } else {
    source->AddString("backgroundPosition", "");
    source->AddString("backgroundTiling", "");
    source->AddString("colorBackground", "");
    source->AddString("themeId", "");
    source->AddString("hascustombackground", "");
    source->AddString("isdark", "");
  }

  source->AddInteger(
      "prerenderStartTimeThreshold",
      features::kNewTabPagePrerenderStartDelayOnMouseHoverByMiliSeconds.Get());
  source->AddInteger(
      "preconnectStartTimeThreshold",
      features::kNewTabPagePreconnectStartDelayOnMouseHoverByMiliSeconds.Get());
  source->AddBoolean(
      "prerenderOnPressEnabled",
      base::FeatureList::IsEnabled(features::kNewTabPageTriggerForPrerender2) &&
          features::kPrerenderNewTabPageOnMousePressedTrigger.Get());
  source->AddBoolean(
      "prerenderOnHoverEnabled",
      base::FeatureList::IsEnabled(features::kNewTabPageTriggerForPrerender2) &&
          features::kPrerenderNewTabPageOnMouseHoverTrigger.Get());

  // Needed by <cr-most-visited> but not used in
  // chrome://new-tab-page-third-party/.
  source->AddString("addLinkTitle", "");
  source->AddString("editLinkTitle", "");
  source->AddString("invalidUrl", "");
  source->AddString("linkAddedMsg", "");
  source->AddString("linkCancel", "");
  source->AddString("linkCantCreate", "");
  source->AddString("linkCantEdit", "");
  source->AddString("linkDone", "");
  source->AddString("linkEditedMsg", "");
  source->AddString("shortcutMoreActions", "");
  source->AddString("nameField", "");
  source->AddString("restoreDefaultLinks", "");
  source->AddString("shortcutAlreadyExists", "");
  source->AddString("urlField", "");

  webui::SetupWebUIDataSource(
      source,
      base::make_span(kNewTabPageThirdPartyResources,
                      kNewTabPageThirdPartyResourcesSize),
      IDR_NEW_TAB_PAGE_THIRD_PARTY_NEW_TAB_PAGE_THIRD_PARTY_HTML);
}
}  // namespace

NewTabPageThirdPartyUI::NewTabPageThirdPartyUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, /*enable_chrome_send=*/false),
      page_factory_receiver_(this),
      most_visited_page_factory_receiver_(this),
      profile_(Profile::FromWebUI(web_ui)),
      web_contents_(web_ui->GetWebContents()),
      navigation_start_time_(base::Time::Now()) {
  CreateAndAddNewTabPageThirdPartyUiHtmlSource(profile_, web_contents_);
  content::URLDataSource::Add(
      profile_, std::make_unique<FaviconSource>(
                    profile_, chrome::FaviconUrlFormat::kFavicon2));
  content::URLDataSource::Add(profile_,
                              std::make_unique<ThemeSource>(profile_));
}

WEB_UI_CONTROLLER_TYPE_IMPL(NewTabPageThirdPartyUI)

NewTabPageThirdPartyUI::~NewTabPageThirdPartyUI() = default;

// static
bool NewTabPageThirdPartyUI::IsNewTabPageOrigin(const GURL& url) {
  return url.DeprecatedGetOriginAsURL() ==
         GURL(chrome::kChromeUINewTabPageThirdPartyURL)
             .DeprecatedGetOriginAsURL();
}

void NewTabPageThirdPartyUI::BindInterface(
    mojo::PendingReceiver<new_tab_page_third_party::mojom::PageHandlerFactory>
        pending_receiver) {
  if (page_factory_receiver_.is_bound()) {
    page_factory_receiver_.reset();
  }

  page_factory_receiver_.Bind(std::move(pending_receiver));
}

void NewTabPageThirdPartyUI::BindInterface(
    mojo::PendingReceiver<most_visited::mojom::MostVisitedPageHandlerFactory>
        pending_receiver) {
  if (most_visited_page_factory_receiver_.is_bound()) {
    most_visited_page_factory_receiver_.reset();
  }

  most_visited_page_factory_receiver_.Bind(std::move(pending_receiver));
}

void NewTabPageThirdPartyUI::CreatePageHandler(
    mojo::PendingRemote<new_tab_page_third_party::mojom::Page> pending_page,
    mojo::PendingReceiver<new_tab_page_third_party::mojom::PageHandler>
        pending_page_handler) {
  DCHECK(pending_page.is_valid());

  page_handler_ = std::make_unique<NewTabPageThirdPartyHandler>(
      std::move(pending_page_handler), std::move(pending_page), profile_,
      web_contents_);
}

void NewTabPageThirdPartyUI::CreatePageHandler(
    mojo::PendingRemote<most_visited::mojom::MostVisitedPage> pending_page,
    mojo::PendingReceiver<most_visited::mojom::MostVisitedPageHandler>
        pending_page_handler) {
  DCHECK(pending_page.is_valid());

  most_visited_page_handler_ = std::make_unique<MostVisitedHandler>(
      std::move(pending_page_handler), std::move(pending_page), profile_,
      web_contents_, GURL(chrome::kChromeUINewTabPageThirdPartyURL),
      navigation_start_time_);
  most_visited_page_handler_->EnableCustomLinks(false);
}
