// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/app_launcher_login_handler.h"

#include <stddef.h>

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_promo_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/webui/profile_info_watcher.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "net/base/escape.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/skia_util.h"

namespace {

SkBitmap GetGAIAPictureForNTP(const gfx::Image& image) {
  // This value must match the width and height value of login-status-icon
  // in new_tab.css.
  const int kLength = 27;
  SkBitmap bmp = skia::ImageOperations::Resize(*image.ToSkBitmap(),
      skia::ImageOperations::RESIZE_BEST, kLength, kLength);
  SkCanvas canvas(bmp);

  // Draw a gray border on the inside of the icon.
  SkPaint paint;
  paint.setColor(SkColorSetARGB(83, 0, 0, 0));
  paint.setStyle(SkPaint::kStroke_Style);
  canvas.drawRect(gfx::RectToSkRect(gfx::Rect(kLength - 1, kLength - 1)),
                  paint);
  return bmp;
}

// Puts the |content| into an element with the given CSS class.
base::string16 CreateElementWithClass(const base::string16& content,
                                      const std::string& tag_name,
                                      const std::string& css_class,
                                      const std::string& extends_tag) {
  base::string16 start_tag = base::ASCIIToUTF16("<" + tag_name +
      " class='" + css_class + "' is='" + extends_tag + "'>");
  base::string16 end_tag = base::ASCIIToUTF16("</" + tag_name + ">");
  return start_tag + net::EscapeForHTML(content) + end_tag;
}

}  // namespace

AppLauncherLoginHandler::AppLauncherLoginHandler() {}

AppLauncherLoginHandler::~AppLauncherLoginHandler() {}

void AppLauncherLoginHandler::RegisterMessages() {
  profile_info_watcher_ = std::make_unique<ProfileInfoWatcher>(
      Profile::FromWebUI(web_ui()),
      base::Bind(&AppLauncherLoginHandler::UpdateLogin,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "initializeSyncLogin",
      base::BindRepeating(&AppLauncherLoginHandler::HandleInitializeSyncLogin,
                          base::Unretained(this)));
#if !defined(OS_CHROMEOS)
  web_ui()->RegisterMessageCallback(
      "showSyncLoginUI",
      base::BindRepeating(&AppLauncherLoginHandler::HandleShowSyncLoginUI,
                          base::Unretained(this)));
#endif
}

void AppLauncherLoginHandler::HandleInitializeSyncLogin(
    const base::ListValue* args) {
  UpdateLogin();
}

#if !defined(OS_CHROMEOS)
void AppLauncherLoginHandler::HandleShowSyncLoginUI(
    const base::ListValue* args) {
  Profile* profile = Profile::FromWebUI(web_ui());
  if (!signin::ShouldShowPromo(profile))
    return;

  std::string username = IdentityManagerFactory::GetForProfile(profile)
                             ->GetPrimaryAccountInfo()
                             .email;
  if (!username.empty())
    return;

  content::WebContents* web_contents = web_ui()->GetWebContents();
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (!browser)
    return;

  // The user isn't signed in, show the sign in promo.
  signin_metrics::AccessPoint access_point =
      web_contents->GetURL().spec() == chrome::kChromeUIAppsURL
          ? signin_metrics::AccessPoint::ACCESS_POINT_APPS_PAGE_LINK
          : signin_metrics::AccessPoint::ACCESS_POINT_NTP_LINK;
  chrome::ShowBrowserSignin(browser, access_point);
  RecordInHistogram(NTP_SIGN_IN_PROMO_CLICKED);
}
#endif

void AppLauncherLoginHandler::RecordInHistogram(NTPSignInPromoBuckets type) {
  DCHECK(type >= NTP_SIGN_IN_PROMO_VIEWED &&
         type < NTP_SIGN_IN_PROMO_BUCKET_BOUNDARY);
  UMA_HISTOGRAM_ENUMERATION("SyncPromo.NTPPromo", type,
                            NTP_SIGN_IN_PROMO_BUCKET_BOUNDARY);
}

void AppLauncherLoginHandler::UpdateLogin() {
  std::string username = profile_info_watcher_->GetAuthenticatedUsername();
  base::string16 header, sub_header;
  std::string icon_url;
  Profile* profile = Profile::FromWebUI(web_ui());
  if (!username.empty()) {
    ProfileAttributesStorage& storage =
        g_browser_process->profile_manager()->GetProfileAttributesStorage();
    ProfileAttributesEntry* entry;
    if (storage.GetProfileAttributesWithPath(profile->GetPath(), &entry)) {
      // Only show the profile picture and full name for the single profile
      // case. In the multi-profile case the profile picture is visible in the
      // title bar and the full name can be ambiguous.
      if (storage.GetNumberOfProfiles() == 1) {
        base::string16 name = entry->GetGAIAName();
        if (!name.empty())
          header = CreateElementWithClass(name, "span", "profile-name", "");
        const gfx::Image* image = entry->GetGAIAPicture();
        if (image)
          icon_url = webui::GetBitmapDataUrl(GetGAIAPictureForNTP(*image));
      }
      if (header.empty()) {
        header = CreateElementWithClass(base::UTF8ToUTF16(username), "span",
                                        "profile-name", "");
      }
    }
  } else {
#if !defined(OS_CHROMEOS)
    // Chromeos does not show this status header.
    bool is_signin_allowed =
        profile->GetOriginalProfile()->GetPrefs()->GetBoolean(
            prefs::kSigninAllowed);
    if (!profile->IsLegacySupervised() && is_signin_allowed) {
      base::string16 signed_in_link = l10n_util::GetStringUTF16(
          IDS_SYNC_PROMO_NOT_SIGNED_IN_STATUS_LINK);
      signed_in_link =
          CreateElementWithClass(signed_in_link, "a", "", "action-link");
      header = l10n_util::GetStringFUTF16(
          IDS_SYNC_PROMO_NOT_SIGNED_IN_STATUS_HEADER,
          l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME));
      sub_header = l10n_util::GetStringFUTF16(
          IDS_SYNC_PROMO_NOT_SIGNED_IN_STATUS_SUB_HEADER, signed_in_link);

      signin_metrics::RecordSigninImpressionUserActionForAccessPoint(
          web_ui()->GetWebContents()->GetURL().spec() ==
                  chrome::kChromeUIAppsURL
              ? signin_metrics::AccessPoint::ACCESS_POINT_APPS_PAGE_LINK
              : signin_metrics::AccessPoint::ACCESS_POINT_NTP_LINK);
      // Record that the user was shown the promo.
      RecordInHistogram(NTP_SIGN_IN_PROMO_VIEWED);
    }
#endif
  }

  base::Value header_value(header);
  base::Value sub_header_value(sub_header);
  base::Value icon_url_value(icon_url);
  base::Value is_user_signed_in(!username.empty());
  web_ui()->CallJavascriptFunctionUnsafe("ntp.updateLogin", header_value,
                                         sub_header_value, icon_url_value,
                                         is_user_signed_in);
}

// static
bool AppLauncherLoginHandler::ShouldShow(Profile* profile) {
#if defined(OS_CHROMEOS)
  // For now we don't care about showing sync status on Chrome OS. The promo
  // UI and the avatar menu don't exist on that platform.
  return false;
#else
  bool is_signin_allowed =
      profile->GetPrefs()->GetBoolean(prefs::kSigninAllowed);
  return !profile->IsOffTheRecord() && is_signin_allowed;
#endif
}
