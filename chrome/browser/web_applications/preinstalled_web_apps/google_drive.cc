// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/preinstalled_web_apps/google_drive.h"

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
    {"af", u8"Google Drive"},
    {"sq", u8"Disku i Google"},
    {"am", u8"Google Drive"},
    {"ar", u8"Google Drive"},
    {"hy", u8"Google Drive"},
    {"az", u8"Google Disk"},
    {"eu", u8"Google Drive"},
    {"be", u8"Google Дыск"},
    {"bn", u8"Google Drive"},
    {"bs", u8"Google Disk"},
    {"bg", u8"Google Диск"},
    {"my", u8"Google Drive"},
    {"ca", u8"Google Drive"},
    {"zh-HK", u8"Google 雲端硬碟"},
    {"zh-CN", u8"Google 云端硬盘"},
    {"zh-TW", u8"Google 雲端硬碟"},
    {"hr", u8"Google disk"},
    {"cs", u8"Disk Google"},
    {"da", u8"Google Drev"},
    {"nl", u8"Google Drive"},
    {"en-GB", u8"Google Drive"},
    {"et", u8"Google Drive"},
    {"fil", u8"Google Drive"},
    {"fi", u8"Google Drive"},
    {"fr-CA", u8"Google Disque"},
    {"fr", u8"Google Drive"},
    {"gl", u8"Google Drive"},
    {"ka", u8"Google Drive"},
    {"de", u8"Google Drive"},
    {"el", u8"Google Drive"},
    {"gu", u8"Google Drive"},
    {"iw", u8"Google Drive"},
    {"hi", u8"Google Drive"},
    {"hu", u8"Google Drive"},
    {"is", u8"Google Drive"},
    {"id", u8"Google Drive"},
    {"it", u8"Google Drive"},
    {"ja", u8"Google ドライブ"},
    {"kn", u8"Google Drive"},
    {"kk", u8"Google Drive"},
    {"km", u8"Google ថាស"},
    {"ko", u8"Google 드라이브"},
    {"ky", u8"Google Drive"},
    {"lo", u8"Google Drive"},
    {"lv", u8"Google disks"},
    {"lt", u8"„Google“ diskas"},
    {"ms", u8"Google Drive"},
    {"ml", u8"Google Drive"},
    {"mr", u8"Google Drive"},
    {"mn", u8"Google Драйв"},
    {"ne", u8"Google Drive"},
    {"no", u8"Google Disk"},
    {"fa", u8"Google Drive"},
    {"pl", u8"Dysk Google"},
    {"pt-BR", u8"Google Drive"},
    {"pt-PT", u8"Google Drive"},
    {"pa", u8"Google Drive"},
    {"ro", u8"Google Drive"},
    {"ru", u8"Google Диск"},
    {"sr", u8"Google диск"},
    {"si", u8"Google Drive"},
    {"sk", u8"Disk Google"},
    {"sl", u8"Google Drive"},
    {"es-419", u8"Google Drive"},
    {"es", u8"Google Drive"},
    {"sw", u8"Hifadhi ya Google"},
    {"sv", u8"Google Drive"},
    {"ta", u8"Google Drive"},
    {"te", u8"Google Drive"},
    {"th", u8"Google ไดรฟ์"},
    {"tr", u8"Google Drive"},
    {"uk", u8"Google Диск"},
    {"ur", u8"Google Drive"},
    {"uz", u8"Google Drive"},
    {"vi", u8"Google Drive"},
    {"cy", u8"Google Drive"},
    {"zu", u8"Google Drayivu"},
    {"zu", u8"Drayivu"},
};
// clang-format on

}  // namespace

ExternalInstallOptions GetConfigForGoogleDrive() {
  ExternalInstallOptions options(
      /*install_url=*/GURL(
          "https://drive.google.com/drive/installwebapp?usp=chrome_default"),
#if BUILDFLAG(IS_CHROMEOS_ASH)
      /*user_display_mode=*/DisplayMode::kStandalone,
#else
      /*user_display_mode=*/DisplayMode::kBrowser,
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
      /*install_source=*/ExternalInstallSource::kExternalDefault);

  options.user_type_allowlist = {"unmanaged", "managed", "child"};
  options.gate_on_feature = kMigrateDefaultChromeAppToWebAppsGSuite.name;
  options.uninstall_and_replace.push_back("apdfllckaahabafndbhieahigkjlhalf");
  options.load_and_await_service_worker_registration = false;
  options.launch_query_params = "usp=installed_webapp";

  options.only_use_app_info_factory = true;
  options.app_info_factory = base::BindRepeating([]() {
    auto info = std::make_unique<WebApplicationInfo>();
    info->title =
        base::UTF8ToUTF16(GetTranslatedName("Google Drive", kNameTranslations));
    info->start_url = GURL("https://drive.google.com/?lfhs=2");
    info->scope = GURL("https://drive.google.com/");
    info->display_mode = DisplayMode::kStandalone;
    info->icon_bitmaps.any =
        LoadBundledIcons({IDR_PREINSTALLED_WEB_APPS_GOOGLE_DRIVE_ICON_192_PNG});
    return info;
  });

  return options;
}

}  // namespace web_app
