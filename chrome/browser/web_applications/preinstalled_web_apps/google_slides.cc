// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/preinstalled_web_apps/google_slides.h"

#include <memory>
#include <string>
#include <vector>

#include "ash/constants/web_app_id_constants.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom-shared.h"
#include "chrome/browser/web_applications/preinstalled_web_apps/preinstalled_web_app_definition_utils.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/grit/preinstalled_web_apps_resources.h"
#include "components/webapps/common/web_app_id.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom-shared.h"
#include "url/gurl.h"

namespace web_app {

namespace {

// clang-format off
constexpr Translation kNameTranslations[] = {
    {"af", "Skyfies"},
    {"am", "ስላይዶች"},
    {"ar", "العروض التقديمية"},
    {"hy", "Սլայդներ"},
    {"az", "Slaydlar"},
    {"eu", "Aurkezpenak"},
    {"be", "Прэзентацыі"},
    {"bn", "Slides"},
    {"bg", "Презентации"},
    {"my", "ဆလိုက်များ"},
    {"ca", "Presentacions"},
    {"zh-HK", "簡報"},
    {"zh-CN", "幻灯片"},
    {"zh-TW", "簡報"},
    {"hr", "Prezentacije"},
    {"cs", "Prezentace"},
    {"da", "Præsentationer"},
    {"nl", "Presentaties"},
    {"en-GB", "Slides"},
    {"et", "Esitlused"},
    {"fil", "Mga Slide"},
    {"fi", "Slides"},
    {"fr", "Présentations"},
    {"fr-CA", "Présentations"},
    {"gl", "Presentacións"},
    {"ka", "Slides"},
    {"de", "Präsentationen"},
    {"el", "Παρουσιάσεις"},
    {"gu", "Slides"},
    {"iw", "Slides"},
    {"hi", "Slides"},
    {"hu", "Diák"},
    {"is", "Skyggnur"},
    {"id", "Slide"},
    {"it", "Presentazioni"},
    {"ja", "スライド"},
    {"kn", "Slides"},
    {"kk", "Slides"},
    {"km", "បទបង្ហាញ"},
    {"ko", "프레젠테이션"},
    {"lo", "ສະໄລ້"},
    {"lv", "Prezentācijas"},
    {"lt", "Skaidrės"},
    {"ms", "Slaid"},
    {"ml", "Slides"},
    {"mr", "Slides"},
    {"mn", "Слайд"},
    {"ne", "स्लाइड"},
    {"no", "Presentasjoner"},
    {"fa", "اسلایدنگار"},
    {"pl", "Prezentacje"},
    {"pt-BR", "Apresentações"},
    {"pt-PT", "Slides"},
    {"pa", "Slides"},
    {"ro", "Prezentări"},
    {"ru", "Презентация"},
    {"sr", "Презентације"},
    {"si", "Slides"},
    {"sk", "Prezentácie"},
    {"sl", "Predstavitve"},
    {"es", "Presentaciones"},
    {"es-419", "Presentaciones"},
    {"sw", "Slaidi"},
    {"sv", "Presentationer"},
    {"ta", "Slides"},
    {"te", "Slides"},
    {"th", "สไลด์"},
    {"tr", "Slaytlar"},
    {"uk", "Презентації"},
    {"ur", "سلائیڈز"},
    {"vi", "Trang trình bày"},
    {"cy", "Sleidiau"},
    {"zu", "Amaslayidi"},
};
// clang-format on

}  // namespace

ExternalInstallOptions GetConfigForGoogleSlides(bool is_standalone_tabbed) {
  ExternalInstallOptions options(
      /*install_url=*/GURL("https://docs.google.com/presentation/"
                           "installwebapp?usp=chrome_default"),
      /*user_display_mode=*/
      is_standalone_tabbed ? mojom::UserDisplayMode::kStandalone
                           : mojom::UserDisplayMode::kBrowser,
      /*install_source=*/ExternalInstallSource::kExternalDefault);

  options.user_type_allowlist = {"unmanaged", "managed", "child"};
  options.uninstall_and_replace.push_back("aapocclcgogkmnckokdopfmhonfmgoek");
  options.load_and_await_service_worker_registration = false;
  options.only_use_app_info_factory = true;
  options.app_info_factory = base::BindRepeating(
      [](bool is_standalone_tabbed) {
        GURL start_url =
            GURL("https://docs.google.com/presentation/?usp=installed_webapp");
        // `manifest_id` must remain fixed even if start_url changes.
        webapps::ManifestId manifest_id = GenerateManifestIdFromStartUrlOnly(
            GURL("https://docs.google.com/presentation/?usp=installed_webapp"));
        auto info = std::make_unique<WebAppInstallInfo>(manifest_id, start_url);
        info->title =
            base::UTF8ToUTF16(GetTranslatedName("Slides", kNameTranslations));
        info->scope = GURL("https://docs.google.com/presentation/");
        info->display_mode =
            is_standalone_tabbed ? DisplayMode::kTabbed : DisplayMode::kBrowser;
        info->icon_bitmaps.any = LoadBundledIcons(
            {IDR_PREINSTALLED_WEB_APPS_GOOGLE_SLIDES_ICON_192_PNG});
        return info;
      },
      is_standalone_tabbed);
  options.expected_app_id = kGoogleSlidesAppId;

  return options;
}

}  // namespace web_app
