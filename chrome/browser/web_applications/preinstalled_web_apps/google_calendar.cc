// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/preinstalled_web_apps/google_docs.h"

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/web_applications/components/external_app_install_features.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "chrome/browser/web_applications/preinstalled_web_apps/preinstalled_web_app_utils.h"
#include "chrome/grit/preinstalled_web_apps_resources.h"

namespace web_app {

namespace {

// clang-format off
constexpr Translation kNameTranslations[] = {
    {"af", u8"Google Kalender"},
    {"am", u8"Google ቀን መቁጠሪያ"},
    {"ar", u8"تقويم Google"},
    {"hy", u8"Google Օրացույց"},
    {"az", u8"Google Calendar"},
    {"eu", u8"Google Calendar"},
    {"be", u8"Google Каляндар"},
    {"bn", u8"Google Calendar"},
    {"bg", u8"Google Календар"},
    {"my", u8"Google Calendar"},
    {"ca", u8"Google Calendar"},
    {"zh-HK", u8"Google 日曆"},
    {"zh-CN", u8"Google 日历"},
    {"zh-TW", u8"Google 日曆"},
    {"hr", u8"Google kalendar"},
    {"cs", u8"Kalendář Google"},
    {"da", u8"Google Kalender"},
    {"nl", u8"Google Agenda"},
    {"en-GB", u8"Google Calendar"},
    {"et", u8"Google'i kalender"},
    {"fil", u8"Google Calendar"},
    {"fi", u8"Google Kalenteri"},
    {"fr-CA", u8"Google Agenda"},
    {"fr", u8"Google Agenda"},
    {"gl", u8"Google Calendar"},
    {"ka", u8"Google Calendar"},
    {"de", u8"Google Kalender"},
    {"el", u8"Ημερολόγιο Google"},
    {"gu", u8"Google Calendar"},
    {"iw", u8"יומן Google"},
    {"hi", u8"Google Calendar"},
    {"hu", u8"Google Naptár"},
    {"is", u8"Google dagatal"},
    {"id", u8"Google Kalender"},
    {"it", u8"Google Calendar"},
    {"ja", u8"Google カレンダー"},
    {"kn", u8"Google Calendar"},
    {"kk", u8"Google Calendar"},
    {"km", u8"Google ប្រតិទិន"},
    {"ko", u8"Google 캘린더"},
    {"lo", u8"Google ປະຕິທິນ"},
    {"lv", u8"Google kalendārs"},
    {"lt", u8"„Google“ kalendorius"},
    {"ms", u8"Kalendar Google"},
    {"ml", u8"Google Calendar"},
    {"mr", u8"Google Calendar"},
    {"mn", u8"Google Календарь"},
    {"ne", u8"Google पात्रो"},
    {"no", u8"Google Kalender"},
    {"or", u8"Google Calendar"},
    {"fa", u8"تقویم Google"},
    {"pl", u8"Kalendarz Google"},
    {"pt-BR", u8"Google Agenda"},
    {"pt-PT", u8"Calendário Google"},
    {"pa", u8"Google Calendar"},
    {"ro", u8"Google Calendar"},
    {"ru", u8"Google Календарь"},
    {"sr", u8"Google календар"},
    {"si", u8"Google දින දර්ශනය"},
    {"sk", u8"Kalendár Google"},
    {"sl", u8"Google Koledar"},
    {"es-419", u8"Calendario de Google"},
    {"es", u8"Google Calendar"},
    {"sw", u8"Kalenda ya Google"},
    {"sv", u8"Google Kalender"},
    {"ta", u8"Google Calendar"},
    {"te", u8"Google Calendar"},
    {"th", u8"Google ปฏิทิน"},
    {"tr", u8"Google Takvim"},
    {"uk", u8"Google Календар"},
    {"ur", u8"Google کیلنڈر"},
    {"vi", u8"Lịch Google"},
    {"cy", u8"Google Calendar"},
    {"zu", u8"Google Khalenda"},
};
// clang-format on

}  // namespace

ExternalInstallOptions GetConfigForGoogleCalendar() {
  ExternalInstallOptions options(
      /*install_url=*/GURL("https://calendar.google.com/calendar/"
                           "installwebapp?usp=chrome_default"),
#if BUILDFLAG(IS_CHROMEOS_ASH)
      /*user_display_mode=*/DisplayMode::kStandalone,
#else
      /*user_display_mode=*/DisplayMode::kBrowser,
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
      /*install_source=*/ExternalInstallSource::kExternalDefault);

  options.user_type_allowlist = {"unmanaged", "managed", "child"};
  options.gate_on_feature = kMigrateDefaultChromeAppToWebAppsGSuite.name;
  options.uninstall_and_replace.push_back("ejjicmeblgpmajnghnpcppodonldlgfn");
  options.disable_if_tablet_form_factor = true;
  options.load_and_await_service_worker_registration = false;
  options.launch_query_params = "usp=installed_webapp";

  options.only_use_app_info_factory = true;
  options.app_info_factory = base::BindRepeating([]() {
    auto info = std::make_unique<WebApplicationInfo>();
    info->title = base::UTF8ToUTF16(
        GetTranslatedName("Google Calendar", kNameTranslations));
    info->start_url = GURL("https://calendar.google.com/calendar/r");
    info->scope = GURL("https://calendar.google.com/calendar/");
    info->display_mode = DisplayMode::kStandalone;
    info->icon_bitmaps.any = LoadBundledIcons(
        {IDR_PREINSTALLED_WEB_APPS_GOOGLE_CALENDAR_ICON_192_PNG});
    return info;
  });

  return options;
}

}  // namespace web_app
