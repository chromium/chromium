// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/preinstalled_web_apps/google_sheets.h"

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
    {"af", u8"Blaaie"},
    {"am", u8"ሉሆች"},
    {"ar", u8"جداول البيانات"},
    {"hy", u8"Աղյուսակներ"},
    {"az", u8"Cədvəl"},
    {"eu", u8"Kalkulu-orriak"},
    {"be", u8"Табліцы"},
    {"bn", u8"Sheets"},
    {"bg", u8"Таблици"},
    {"my", u8"စာမျက်နှာများ"},
    {"ca", u8"Fulls de càlcul"},
    {"zh-HK", u8"試算表"},
    {"zh-CN", u8"表格"},
    {"zh-TW", u8"試算表"},
    {"hr", u8"Listovi"},
    {"cs", u8"Tabulky"},
    {"da", u8"Regneark"},
    {"nl", u8"Spreadsheets"},
    {"en-GB", u8"Sheets"},
    {"et", u8"Lehed"},
    {"fil", u8"Mga Sheet"},
    {"fi", u8"Sheets"},
    {"fr", u8"Feuilles de calcul"},
    {"fr-CA", u8"Feuilles de calcul"},
    {"gl", u8"Follas de cálculo"},
    {"ka", u8"Sheets"},
    {"de", u8"Tabellen"},
    {"el", u8"Υπολογιστικά φύλλα"},
    {"gu", u8"Sheets"},
    {"iw", u8"Sheets"},
    {"hi", u8"Sheets"},
    {"hu", u8"Táblázatok"},
    {"is", u8"Töflureiknar"},
    {"id", u8"Spreadsheet"},
    {"it", u8"Fogli"},
    {"ja", u8"スプレッドシート"},
    {"kn", u8"Sheets"},
    {"kk", u8"Sheets"},
    {"km", u8"បញ្ជី"},
    {"ko", u8"스프레드시트"},
    {"lo", u8"​ຊີດ"},
    {"lv", u8"Izklājlapas"},
    {"lt", u8"Skaičiuoklės"},
    {"ms", u8"Helaian"},
    {"ml", u8"Sheets"},
    {"mr", u8"Sheets"},
    {"mn", u8"Хүснэгт"},
    {"ne", u8"पाना"},
    {"no", u8"Regneark"},
    {"fa", u8"کاربرگ‌نگار"},
    {"pl", u8"Arkusze"},
    {"pt-BR", u8"Planilhas"},
    {"pt-PT", u8"Sheets"},
    {"pa", u8"Sheets"},
    {"ro", u8"Foi de calcul"},
    {"ru", u8"Таблица"},
    {"sr", u8"Табеле"},
    {"si", u8"Sheets"},
    {"sk", u8"Tabuľky"},
    {"sl", u8"Preglednice"},
    {"es", u8"Hojas de cálculo"},
    {"es-419", u8"Hojas de cálculo"},
    {"sw", u8"Majedwali"},
    {"sv", u8"Kalkylark"},
    {"ta", u8"Sheets"},
    {"te", u8"షీట్‌లు"},
    {"th", u8"ชีต"},
    {"tr", u8"E-Tablolar"},
    {"uk", u8"Таблиці"},
    {"ur", u8"شیٹس"},
    {"vi", u8"Trang tính"},
    {"cy", u8"Dalenni"},
    {"zu", u8"AmaSpredishithi"},
};
// clang-format on

}  // namespace

ExternalInstallOptions GetConfigForGoogleSheets() {
  ExternalInstallOptions options(
      /*install_url=*/GURL("https://docs.google.com/spreadsheets/"
                           "installwebapp?usp=chrome_default"),
      /*user_display_mode=*/DisplayMode::kBrowser,
      /*install_source=*/ExternalInstallSource::kExternalDefault);

  options.user_type_allowlist = {"unmanaged", "managed", "child"};
  options.gate_on_feature = kMigrateDefaultChromeAppToWebAppsGSuite.name;
  options.uninstall_and_replace.push_back("felcaaldnbdncclmgdcncolpebgiejap");
  options.load_and_await_service_worker_registration = false;
  options.only_use_app_info_factory = true;
  options.app_info_factory = base::BindRepeating([]() {
    auto info = std::make_unique<WebApplicationInfo>();
    info->title =
        base::UTF8ToUTF16(GetTranslatedName("Sheets", kNameTranslations));

    info->start_url =
        GURL("https://docs.google.com/spreadsheets/?usp=installed_webapp");
    info->scope = GURL("https://docs.google.com/spreadsheets/");
    info->display_mode = DisplayMode::kBrowser;
    info->icon_bitmaps.any = LoadBundledIcons(
        {IDR_PREINSTALLED_WEB_APPS_GOOGLE_SHEETS_ICON_192_PNG});
    return info;
  });

  return options;
}

}  // namespace web_app
