// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/preinstalled_web_apps/google_drive.h"

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
    {"af", "Google Drive"},
    {"sq", "Disku i Google"},
    {"am", "Google Drive"},
    {"ar", "Google Drive"},
    {"hy", "Google Drive"},
    {"az", "Google Disk"},
    {"eu", "Google Drive"},
    {"be", "Google Дыск"},
    {"bn", "Google Drive"},
    {"bs", "Google Disk"},
    {"bg", "Google Диск"},
    {"my", "Google Drive"},
    {"ca", "Google Drive"},
    {"zh-HK", "Google 雲端硬碟"},
    {"zh-CN", "Google 云端硬盘"},
    {"zh-TW", "Google 雲端硬碟"},
    {"hr", "Google disk"},
    {"cs", "Disk Google"},
    {"da", "Google Drev"},
    {"nl", "Google Drive"},
    {"en-GB", "Google Drive"},
    {"et", "Google Drive"},
    {"fil", "Google Drive"},
    {"fi", "Google Drive"},
    {"fr-CA", "Google Disque"},
    {"fr", "Google Drive"},
    {"gl", "Google Drive"},
    {"ka", "Google Drive"},
    {"de", "Google Drive"},
    {"el", "Google Drive"},
    {"gu", "Google Drive"},
    {"iw", "Google Drive"},
    {"hi", "Google Drive"},
    {"hu", "Google Drive"},
    {"is", "Google Drive"},
    {"id", "Google Drive"},
    {"it", "Google Drive"},
    {"ja", "Google ドライブ"},
    {"kn", "Google Drive"},
    {"kk", "Google Drive"},
    {"km", "Google ថាស"},
    {"ko", "Google 드라이브"},
    {"ky", "Google Drive"},
    {"lo", "Google Drive"},
    {"lv", "Google disks"},
    {"lt", "„Google“ diskas"},
    {"ms", "Google Drive"},
    {"ml", "Google Drive"},
    {"mr", "Google Drive"},
    {"mn", "Google Драйв"},
    {"ne", "Google Drive"},
    {"no", "Google Disk"},
    {"fa", "Google Drive"},
    {"pl", "Dysk Google"},
    {"pt-BR", "Google Drive"},
    {"pt-PT", "Google Drive"},
    {"pa", "Google Drive"},
    {"ro", "Google Drive"},
    {"ru", "Google Диск"},
    {"sr", "Google диск"},
    {"si", "Google Drive"},
    {"sk", "Disk Google"},
    {"sl", "Google Drive"},
    {"es-419", "Google Drive"},
    {"es", "Google Drive"},
    {"sw", "Hifadhi ya Google"},
    {"sv", "Google Drive"},
    {"ta", "Google Drive"},
    {"te", "Google Drive"},
    {"th", "Google ไดรฟ์"},
    {"tr", "Google Drive"},
    {"uk", "Google Диск"},
    {"ur", "Google Drive"},
    {"uz", "Google Drive"},
    {"vi", "Google Drive"},
    {"cy", "Google Drive"},
    {"zu", "Google Drayivu"},
    {"zu", "Drayivu"},
};
// clang-format on

}  // namespace

ExternalInstallOptions GetConfigForGoogleDrive(bool is_standalone) {
  ExternalInstallOptions options(
      /*install_url=*/GURL(
          "https://drive.google.com/drive/installwebapp?usp=chrome_default"),
      /*user_display_mode=*/
      is_standalone ? mojom::UserDisplayMode::kStandalone
                    : mojom::UserDisplayMode::kBrowser,
      /*install_source=*/ExternalInstallSource::kExternalDefault);

  options.user_type_allowlist = {"unmanaged", "managed", "child"};
  options.uninstall_and_replace.push_back("apdfllckaahabafndbhieahigkjlhalf");
  options.load_and_await_service_worker_registration = false;
  options.launch_query_params = "usp=installed_webapp";

  options.only_use_app_info_factory = true;
  options.app_info_factory = base::BindRepeating([]() {
    GURL start_url = GURL("https://drive.google.com/?lfhs=2");
    // `manifest_id` must remain fixed even if start_url changes.
    webapps::ManifestId manifest_id = GenerateManifestIdFromStartUrlOnly(
        GURL("https://drive.google.com/?lfhs=2"));
    auto info = std::make_unique<WebAppInstallInfo>(manifest_id, start_url);
    info->title =
        base::UTF8ToUTF16(GetTranslatedName("Google Drive", kNameTranslations));
    info->scope = GURL("https://drive.google.com/");
    info->display_mode = DisplayMode::kStandalone;
    info->icon_bitmaps.any =
        LoadBundledIcons({IDR_PREINSTALLED_WEB_APPS_GOOGLE_DRIVE_ICON_192_PNG});
    return info;
  });
  options.expected_app_id = kGoogleDriveAppId;

  return options;
}

}  // namespace web_app
