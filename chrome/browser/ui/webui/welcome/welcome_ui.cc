// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/welcome/welcome_ui.h"

#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/browser/ui/webui/welcome/bookmark_handler.h"
#include "chrome/browser/ui/webui/welcome/google_apps_handler.h"
#include "chrome/browser/ui/webui/welcome/helpers.h"
#include "chrome/browser/ui/webui/welcome/ntp_background_handler.h"
#include "chrome/browser/ui/webui/welcome/set_as_default_handler.h"
#include "chrome/browser/ui/webui/welcome/welcome_handler.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/welcome_resources.h"
#include "chrome/grit/welcome_resources_map.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/strings/grit/components_strings.h"
#include "net/base/url_util.h"
#include "ui/base/webui/web_ui_util.h"

namespace {

const char kPreviewBackgroundPath[] = "preview-background.jpg";

bool ShouldHandleRequestCallback(base::WeakPtr<WelcomeUI> weak_ptr,
                                 const std::string& path) {
  if (!base::StartsWith(path, kPreviewBackgroundPath,
                        base::CompareCase::SENSITIVE)) {
    return false;
  }

  std::string index_param = path.substr(path.find_first_of("?") + 1);
  int background_index = -1;
  if (!base::StringToInt(index_param, &background_index) ||
      background_index < 0) {
    return false;
  }

  return !!weak_ptr;
}

void HandleRequestCallback(base::WeakPtr<WelcomeUI> weak_ptr,
                           const std::string& path,
                           content::WebUIDataSource::GotDataCallback callback) {
  DCHECK(ShouldHandleRequestCallback(weak_ptr, path));

  std::string index_param = path.substr(path.find_first_of("?") + 1);
  int background_index = -1;
  CHECK(base::StringToInt(index_param, &background_index) ||
        background_index < 0);

  DCHECK(weak_ptr);
  weak_ptr->CreateBackgroundFetcher(background_index, std::move(callback));
}

void AddStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      // Shared strings.
      {"bookmarkAdded", IDS_WELCOME_BOOKMARK_ADDED},
      {"bookmarksAdded", IDS_WELCOME_BOOKMARKS_ADDED},
      {"bookmarkRemoved", IDS_WELCOME_BOOKMARK_REMOVED},
      {"bookmarksRemoved", IDS_WELCOME_BOOKMARKS_REMOVED},
      {"defaultBrowserChanged", IDS_DEFAULT_BROWSER_CHANGED},
      {"headerText", IDS_WELCOME_HEADER},
      {"next", IDS_WELCOME_NEXT},
      {"noThanks", IDS_NO_THANKS},
      {"skip", IDS_WELCOME_SKIP},
      {"stepsLabel", IDS_WELCOME_STEPS},

      // Sign-in view strings.
      {"signInHeader", IDS_WELCOME_SIGNIN_VIEW_HEADER},
      {"signInSubHeader", IDS_WELCOME_SIGNIN_VIEW_SUB_HEADER},
      {"signIn", IDS_WELCOME_SIGNIN_VIEW_SIGNIN},

      // Google apps module strings.
      {"googleAppsDescription", IDS_WELCOME_GOOGLE_APPS_DESCRIPTION},

      // New Tab Page background module strings.
      {"ntpBackgroundDescription", IDS_WELCOME_NTP_BACKGROUND_DESCRIPTION},
      {"ntpBackgroundDefault", IDS_WELCOME_NTP_BACKGROUND_DEFAULT_TITLE},
      {"ntpBackgroundPreviewUpdated",
       IDS_WELCOME_NTP_BACKGROUND_PREVIEW_UPDATED},
      {"ntpBackgroundReset", IDS_WELCOME_NTP_BACKGROUND_RESET},

      // Set as default module strings.
      {"setDefaultHeader", IDS_WELCOME_SET_AS_DEFAULT_HEADER},
      {"setDefaultSubHeader", IDS_WELCOME_SET_AS_DEFAULT_SUB_HEADER},
      {"setDefaultConfirm", IDS_WELCOME_SET_AS_DEFAULT_SET_AS_DEFAULT},

      // Landing view strings.
      {"landingTitle", IDS_WELCOME_LANDING_TITLE},
      {"landingDescription", IDS_WELCOME_LANDING_DESCRIPTION},
      {"landingNewUser", IDS_WELCOME_LANDING_NEW_USER},
      {"landingExistingUser", IDS_WELCOME_LANDING_EXISTING_USER},
      {"landingPauseAnimations", IDS_WELCOME_LANDING_PAUSE_ANIMATIONS},
      {"landingPlayAnimations", IDS_WELCOME_LANDING_PLAY_ANIMATIONS},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);
}

}  // namespace

bool WelcomeUIConfig::IsWebUIEnabled(content::BrowserContext* browser_context) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  return welcome::IsEnabled(profile);
}

std::unique_ptr<content::WebUIController>
WelcomeUIConfig::CreateWebUIController(content::WebUI* web_ui,
                                       const GURL& url) {
  return std::make_unique<WelcomeUI>(web_ui, url);
}

WelcomeUI::WelcomeUI(content::WebUI* web_ui, const GURL& url)
    : content::WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);

  // This page is not shown to incognito or guest profiles. If one should end up
  // here, we return, causing a 404-like page.
  if (!profile || profile->IsOffTheRecord()) {
    return;
  }

  StorePageSeen(profile);

  web_ui->AddMessageHandler(std::make_unique<WelcomeHandler>(web_ui));

  content::WebUIDataSource* html_source =
      content::WebUIDataSource::CreateAndAdd(profile, url.host());
  webui::SetupWebUIDataSource(
      html_source, base::make_span(kWelcomeResources, kWelcomeResourcesSize),
      IDR_WELCOME_WELCOME_HTML);

  // Add welcome strings.
  AddStrings(html_source);

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  html_source->AddResourcePath("images/background_svgs/logo.svg",
                               IDR_PRODUCT_LOGO_128PX_SVG);
#endif

#if BUILDFLAG(IS_WIN)
  html_source->AddBoolean("is_win10", true);
#endif

  // Add the shared bookmark handler for welcome modules.
  web_ui->AddMessageHandler(
      std::make_unique<welcome::BookmarkHandler>(profile->GetPrefs()));

  // Add google apps bookmarking module.
  web_ui->AddMessageHandler(std::make_unique<welcome::GoogleAppsHandler>());

  // Add NTP custom background module.
  web_ui->AddMessageHandler(std::make_unique<welcome::NtpBackgroundHandler>());

  // Add set-as-default module.
  web_ui->AddMessageHandler(std::make_unique<welcome::SetAsDefaultHandler>());

  html_source->AddString(
      "newUserModules",
      welcome::GetModules(profile).Find("new-user")->GetString());
  html_source->AddString(
      "returningUserModules",
      welcome::GetModules(profile).Find("returning-user")->GetString());
  html_source->AddBoolean(
      "signinAllowed", profile->GetPrefs()->GetBoolean(prefs::kSigninAllowed));
  html_source->SetRequestFilter(
      base::BindRepeating(&ShouldHandleRequestCallback,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(&HandleRequestCallback,
                          weak_ptr_factory_.GetWeakPtr()));
}

WelcomeUI::~WelcomeUI() {}

void WelcomeUI::CreateBackgroundFetcher(
    size_t background_index,
    content::WebUIDataSource::GotDataCallback callback) {
  background_fetcher_ = std::make_unique<welcome::NtpBackgroundFetcher>(
      background_index, std::move(callback));
}

void WelcomeUI::StorePageSeen(Profile* profile) {
  // Store that this profile has been shown the Welcome page.
  profile->GetPrefs()->SetBoolean(prefs::kHasSeenWelcomePage, true);
}
