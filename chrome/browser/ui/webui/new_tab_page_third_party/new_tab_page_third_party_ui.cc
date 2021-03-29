// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page_third_party/new_tab_page_third_party_ui.h"

#include <memory>
#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/ui/webui/customize_themes/chrome_customize_themes_handler.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/new_tab_page_third_party/new_tab_page_third_party_handler.h"
#include "chrome/browser/ui/webui/ntp/ntp_resource_cache.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/new_tab_page_third_party_resources.h"
#include "chrome/grit/new_tab_page_third_party_resources_map.h"
#include "chrome/grit/theme_resources.h"
#include "components/favicon_base/favicon_url_parser.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_utils.h"
#include "ui/resources/grit/webui_generated_resources.h"
#include "url/url_util.h"

using content::BrowserContext;
using content::WebContents;

namespace {

content::WebUIDataSource* CreateNewTabPageThirdPartyUiHtmlSource(
    Profile* profile,
    WebContents* web_contents) {
  content::WebUIDataSource* source = content::WebUIDataSource::Create(
      chrome::kChromeUINewTabPageThirdPartyHost);
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

  const ui::ThemeProvider& theme_provider =
      ThemeService::GetThemeProviderForProfile(profile);
  source->AddString("backgroundPosition",
                    GetNewTabBackgroundPositionCSS(theme_provider));
  source->AddString("backgroundTiling",
                    GetNewTabBackgroundTilingCSS(theme_provider));
  source->AddString("colorBackground",
                    color_utils::SkColorToRgbaString(GetThemeColor(
                        webui::GetNativeTheme(web_contents), theme_provider,
                        ThemeProperties::COLOR_NTP_BACKGROUND)));
  source->AddString("themeId",
                    profile->GetPrefs()->GetString(prefs::kCurrentThemeID));
  source->AddString("hascustombackground",
                    theme_provider.HasCustomImage(IDR_THEME_NTP_BACKGROUND)
                        ? "has-custom-background"
                        : "");
  source->AddString("isdark", !color_utils::IsDark(theme_provider.GetColor(
                                  ThemeProperties::COLOR_NTP_TEXT))
                                  ? "dark"
                                  : "");

  webui::SetupWebUIDataSource(
      source,
      base::make_span(kNewTabPageThirdPartyResources,
                      kNewTabPageThirdPartyResourcesSize),
      IDR_NEW_TAB_PAGE_THIRD_PARTY_NEW_TAB_PAGE_THIRD_PARTY_HTML);

  return source;
}
}  // namespace

NewTabPageThirdPartyUI::NewTabPageThirdPartyUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, /*enable_chrome_send=*/false),
      page_factory_receiver_(this),
      profile_(Profile::FromWebUI(web_ui)),
      web_contents_(web_ui->GetWebContents()),
      navigation_start_time_(base::Time::Now()) {
  auto* source =
      CreateNewTabPageThirdPartyUiHtmlSource(profile_, web_contents_);
  content::WebUIDataSource::Add(profile_, source);
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
  return url.GetOrigin() ==
         GURL(chrome::kChromeUINewTabPageThirdPartyURL).GetOrigin();
}

void NewTabPageThirdPartyUI::BindInterface(
    mojo::PendingReceiver<new_tab_page_third_party::mojom::PageHandlerFactory>
        pending_receiver) {
  if (page_factory_receiver_.is_bound()) {
    page_factory_receiver_.reset();
  }

  page_factory_receiver_.Bind(std::move(pending_receiver));
}

void NewTabPageThirdPartyUI::CreatePageHandler(
    mojo::PendingRemote<new_tab_page_third_party::mojom::Page> pending_page,
    mojo::PendingReceiver<new_tab_page_third_party::mojom::PageHandler>
        pending_page_handler) {
  DCHECK(pending_page.is_valid());

  page_handler_ = std::make_unique<NewTabPageThirdPartyHandler>(
      std::move(pending_page_handler), std::move(pending_page), profile_,
      web_contents_, navigation_start_time_);
}
