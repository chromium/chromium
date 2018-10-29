// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/welcome/nux/email_handler.h"

#include "base/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/webui/welcome/nux/bookmark_item.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/favicon/core/favicon_service.h"
#include "components/grit/components_resources.h"
#include "components/grit/components_scaled_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/resource/resource_bundle.h"

namespace nux {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class EmailProviders {
  kGmail = 0,
  kYahoo = 1,
  kOutlook = 2,
  kAol = 3,
  kiCloud = 4,
  kCount,
};

const char* kEmailInteractionHistogram =
    "FirstRun.NewUserExperience.EmailInteraction";

// Strings in costants not translated because this is an experiment.
// Translate before wide release.
const BookmarkItem kEmail[] = {
    {static_cast<int>(EmailProviders::kGmail), "Gmail", "gmail",
     "https://accounts.google.com/b/0/AddMailService", IDR_NUX_EMAIL_GMAIL_1X},
    {static_cast<int>(EmailProviders::kYahoo), "Yahoo", "yahoo",
     "https://mail.yahoo.com", IDR_NUX_EMAIL_YAHOO_1X},
    {static_cast<int>(EmailProviders::kOutlook), "Outlook", "outlook",
     "https://login.live.com/login.srf?", IDR_NUX_EMAIL_OUTLOOK_1X},
    {static_cast<int>(EmailProviders::kAol), "AOL", "aol",
     "https://mail.aol.com", IDR_NUX_EMAIL_AOL_1X},
    {static_cast<int>(EmailProviders::kiCloud), "iCloud", "icloud",
     "https://www.icloud.com/mail", IDR_NUX_EMAIL_ICLOUD_1X},
};

constexpr const int kEmailIconSize = 48;  // Pixels.

static_assert(base::size(kEmail) == (size_t)EmailProviders::kCount,
              "names and histograms must match");

EmailHandler::EmailHandler(favicon::FaviconService* favicon_service)
    : favicon_service_(favicon_service) {}

EmailHandler::~EmailHandler() {}

void EmailHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "cacheEmailIcon", base::BindRepeating(&EmailHandler::HandleCacheEmailIcon,
                                            base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "getEmailList", base::BindRepeating(&EmailHandler::HandleGetEmailList,
                                          base::Unretained(this)));
}

void EmailHandler::HandleCacheEmailIcon(const base::ListValue* args) {
  int emailId;
  args->GetInteger(0, &emailId);

  const BookmarkItem* selectedEmail = NULL;
  for (size_t i = 0; i < base::size(kEmail); i++) {
    if (static_cast<int>(kEmail[i].id) == emailId) {
      selectedEmail = &kEmail[i];
      break;
    }
  }
  CHECK(selectedEmail);  // WebUI should not be able to pass non-existent ID.

  // Preload the favicon cache with Chrome-bundled images. Otherwise, the
  // pre-populated bookmarks don't have favicons and look bad. Favicons are
  // updated automatically when a user visits a site.
  GURL app_url = GURL(selectedEmail->url);
  favicon_service_->MergeFavicon(
      app_url, app_url, favicon_base::IconType::kFavicon,
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
          selectedEmail->icon),
      gfx::Size(kEmailIconSize, kEmailIconSize));
}

void EmailHandler::HandleGetEmailList(const base::ListValue* args) {
  AllowJavascript();
  CHECK_EQ(1U, args->GetSize());
  const base::Value* callback_id;
  CHECK(args->Get(0, &callback_id));
  ResolveJavascriptCallback(
      *callback_id, bookmarkItemsToListValue(kEmail, base::size(kEmail)));
}

void EmailHandler::AddSources(content::WebUIDataSource* html_source) {
  // Add icons
  html_source->AddResourcePath("email/aol_1x.png", IDR_NUX_EMAIL_AOL_1X);
  html_source->AddResourcePath("email/aol_2x.png", IDR_NUX_EMAIL_AOL_2X);
  html_source->AddResourcePath("email/gmail_1x.png", IDR_NUX_EMAIL_GMAIL_1X);
  html_source->AddResourcePath("email/gmail_2x.png", IDR_NUX_EMAIL_GMAIL_2X);
  html_source->AddResourcePath("email/icloud_1x.png", IDR_NUX_EMAIL_ICLOUD_1X);
  html_source->AddResourcePath("email/icloud_2x.png", IDR_NUX_EMAIL_ICLOUD_2X);
  html_source->AddResourcePath("email/outlook_1x.png",
                               IDR_NUX_EMAIL_OUTLOOK_1X);
  html_source->AddResourcePath("email/outlook_2x.png",
                               IDR_NUX_EMAIL_OUTLOOK_2X);
  html_source->AddResourcePath("email/yahoo_1x.png", IDR_NUX_EMAIL_YAHOO_1X);
  html_source->AddResourcePath("email/yahoo_2x.png", IDR_NUX_EMAIL_YAHOO_2X);
}

}  // namespace nux
