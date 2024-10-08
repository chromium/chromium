// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/web_app_id_constants.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/preinstalled_app_install_features.h"
#include "chrome/browser/web_applications/preinstalled_web_apps/google_docs.h"
#include "chrome/browser/web_applications/preinstalled_web_apps/preinstalled_web_app_definition_utils.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/grit/preinstalled_web_apps_resources.h"

namespace web_app {

namespace {

// clang-format off
constexpr Translation kNameTranslations[] = {
    {"af", "Google Kalender"},
    {"am", "Google ቀን መቁጠሪያ"},
    {"ar", "تقويم Google"},
    {"hy", "Google Օրացույց"},
    {"az", "Google Calendar"},
    {"eu", "Google Calendar"},
    {"be", "Google Каляндар"},
    {"bn", "Google Calendar"},
    {"bg", "Google Календар"},
    {"my", "Google Calendar"},
    {"ca", "Google Calendar"},
    {"zh-HK", "Google 日曆"},
    {"zh-CN", "Google 日历"},
    {"zh-TW", "Google 日曆"},
    {"hr", "Google kalendar"},
    {"cs", "Kalendář Google"},
    {"da", "Google Kalender"},
    {"nl", "Google Agenda"},
    {"en-GB", "Google Calendar"},
    {"et", "Google'i kalender"},
    {"fil", "Google Calendar"},
    {"fi", "Google Kalenteri"},
    {"fr-CA", "Google Agenda"},
    {"fr", "Google Agenda"},
    {"gl", "Google Calendar"},
    {"ka", "Google Calendar"},
    {"de", "Google Kalender"},
    {"el", "Ημερολόγιο Google"},
    {"gu", "Google Calendar"},
    {"iw", "יומן Google"},
    {"hi", "Google Calendar"},
    {"hu", "Google Naptár"},
    {"is", "Google dagatal"},
    {"id", "Google Kalender"},
    {"it", "Google Calendar"},
    {"ja", "Google カレンダー"},
    {"kn", "Google Calendar"},
    {"kk", "Google Calendar"},
    {"km", "Google ប្រតិទិន"},
    {"ko", "Google 캘린더"},
    {"lo", "Google ປະຕິທິນ"},
    {"lv", "Google kalendārs"},
    {"lt", "„Google“ kalendorius"},
    {"ms", "Kalendar Google"},
    {"ml", "Google Calendar"},
    {"mr", "Google Calendar"},
    {"mn", "Google Календарь"},
    {"ne", "Google पात्रो"},
    {"no", "Google Kalender"},
    {"or", "Google Calendar"},
    {"fa", "تقویم Google"},
    {"pl", "Kalendarz Google"},
    {"pt-BR", "Google Agenda"},
    {"pt-PT", "Calendário Google"},
    {"pa", "Google Calendar"},
    {"ro", "Google Calendar"},
    {"ru", "Google Календарь"},
    {"sr", "Google календар"},
    {"si", "Google දින දර්ශනය"},
    {"sk", "Kalendár Google"},
    {"sl", "Google Koledar"},
    {"es-419", "Calendario de Google"},
    {"es", "Google Calendar"},
    {"sw", "Kalenda ya Google"},
    {"sv", "Google Kalender"},
    {"ta", "Google Calendar"},
    {"te", "Google Calendar"},
    {"th", "Google ปฏิทิน"},
    {"tr", "Google Takvim"},
    {"uk", "Google Календар"},
    {"ur", "Google کیلنڈر"},
    {"vi", "Lịch Google"},
    {"cy", "Google Calendar"},
    {"zu", "Google Khalenda"},
};
// clang-format on

}  // namespace

ExternalInstallOptions GetConfigForGoogleCalendar() {
  ExternalInstallOptions options(
      /*install_url=*/GURL("https://calendar.google.com/calendar/"
                           "installwebapp?usp=chrome_default"),
#if BUILDFLAG(IS_CHROMEOS)
      /*user_display_mode=*/mojom::UserDisplayMode::kStandalone,
#else
      /*user_display_mode=*/mojom::UserDisplayMode::kBrowser,
#endif  // BUILDFLAG(IS_CHROMEOS)
      /*install_source=*/ExternalInstallSource::kExternalDefault);

  options.user_type_allowlist = {"unmanaged", "managed", "child"};
  options.uninstall_and_replace.push_back("ejjicmeblgpmajnghnpcppodonldlgfn");
  options.disable_if_tablet_form_factor = true;
  options.load_and_await_service_worker_registration = false;
  options.launch_query_params = "usp=installed_webapp";

  options.only_use_app_info_factory = true;
  options.app_info_factory = base::BindRepeating([]() {
    GURL start_url = GURL("https://calendar.google.com/calendar/r");
    // `manifest_id` must remain fixed even if start_url changes.
    webapps::ManifestId manifest_id = GenerateManifestIdFromStartUrlOnly(
        GURL("https://calendar.google.com/calendar/r"));
    auto info = std::make_unique<WebAppInstallInfo>(manifest_id, start_url);
    info->title = base::UTF8ToUTF16(
        GetTranslatedName("Google Calendar", kNameTranslations));
    info->scope = GURL("https://calendar.google.com/calendar/");
    info->display_mode = DisplayMode::kStandalone;
    info->icon_bitmaps.any = LoadBundledIcons(
        {IDR_PREINSTALLED_WEB_APPS_GOOGLE_CALENDAR_ICON_192_PNG});
    return info;
  });
  options.expected_app_id = kGoogleCalendarAppId;

  return options;
}

}  // namespace web_app
