// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/preinstalled_web_apps/google_slides.h"

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
    {"af", u8"Skyfies"},
    {"am", u8"ስላይዶች"},
    {"ar", u8"العروض التقديمية"},
    {"hy", u8"Սլայդներ"},
    {"az", u8"Slaydlar"},
    {"eu", u8"Aurkezpenak"},
    {"be", u8"Прэзентацыі"},
    {"bn", u8"Slides"},
    {"bg", u8"Презентации"},
    {"my", u8"ဆလိုက်များ"},
    {"ca", u8"Presentacions"},
    {"zh-HK", u8"簡報"},
    {"zh-CN", u8"幻灯片"},
    {"zh-TW", u8"簡報"},
    {"hr", u8"Prezentacije"},
    {"cs", u8"Prezentace"},
    {"da", u8"Præsentationer"},
    {"nl", u8"Presentaties"},
    {"en-GB", u8"Slides"},
    {"et", u8"Esitlused"},
    {"fil", u8"Mga Slide"},
    {"fi", u8"Slides"},
    {"fr", u8"Présentations"},
    {"fr-CA", u8"Présentations"},
    {"gl", u8"Presentacións"},
    {"ka", u8"Slides"},
    {"de", u8"Präsentationen"},
    {"el", u8"Παρουσιάσεις"},
    {"gu", u8"Slides"},
    {"iw", u8"Slides"},
    {"hi", u8"Slides"},
    {"hu", u8"Diák"},
    {"is", u8"Skyggnur"},
    {"id", u8"Slide"},
    {"it", u8"Presentazioni"},
    {"ja", u8"スライド"},
    {"kn", u8"Slides"},
    {"kk", u8"Slides"},
    {"km", u8"បទបង្ហាញ"},
    {"ko", u8"프레젠테이션"},
    {"lo", u8"ສະໄລ້"},
    {"lv", u8"Prezentācijas"},
    {"lt", u8"Skaidrės"},
    {"ms", u8"Slaid"},
    {"ml", u8"Slides"},
    {"mr", u8"Slides"},
    {"mn", u8"Слайд"},
    {"ne", u8"स्लाइड"},
    {"no", u8"Presentasjoner"},
    {"fa", u8"اسلایدنگار"},
    {"pl", u8"Prezentacje"},
    {"pt-BR", u8"Apresentações"},
    {"pt-PT", u8"Slides"},
    {"pa", u8"Slides"},
    {"ro", u8"Prezentări"},
    {"ru", u8"Презентация"},
    {"sr", u8"Презентације"},
    {"si", u8"Slides"},
    {"sk", u8"Prezentácie"},
    {"sl", u8"Predstavitve"},
    {"es", u8"Presentaciones"},
    {"es-419", u8"Presentaciones"},
    {"sw", u8"Slaidi"},
    {"sv", u8"Presentationer"},
    {"ta", u8"Slides"},
    {"te", u8"Slides"},
    {"th", u8"สไลด์"},
    {"tr", u8"Slaytlar"},
    {"uk", u8"Презентації"},
    {"ur", u8"سلائیڈز"},
    {"vi", u8"Trang trình bày"},
    {"cy", u8"Sleidiau"},
    {"zu", u8"Amaslayidi"},
};
// clang-format on

}  // namespace

ExternalInstallOptions GetConfigForGoogleSlides() {
  ExternalInstallOptions options(
      /*install_url=*/GURL("https://docs.google.com/presentation/"
                           "installwebapp?usp=chrome_default"),
      /*user_display_mode=*/DisplayMode::kBrowser,
      /*install_source=*/ExternalInstallSource::kExternalDefault);

  options.user_type_allowlist = {"unmanaged", "managed", "child"};
  options.gate_on_feature = kMigrateDefaultChromeAppToWebAppsGSuite.name;
  options.uninstall_and_replace.push_back("aapocclcgogkmnckokdopfmhonfmgoek");
  options.load_and_await_service_worker_registration = false;
  options.only_use_app_info_factory = true;
  options.app_info_factory = base::BindRepeating([]() {
    auto info = std::make_unique<WebApplicationInfo>();
    info->title =
        base::UTF8ToUTF16(GetTranslatedName("Slides", kNameTranslations));
    info->start_url =
        GURL("https://docs.google.com/presentation/?usp=installed_webapp");
    info->scope = GURL("https://docs.google.com/presentation/");
    info->display_mode = DisplayMode::kBrowser;
    info->icon_bitmaps.any = LoadBundledIcons(
        {IDR_PREINSTALLED_WEB_APPS_GOOGLE_SLIDES_ICON_192_PNG});
    return info;
  });

  return options;
}

}  // namespace web_app
