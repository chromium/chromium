// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/preinstalled_web_apps/google_docs.h"

#include <memory>
#include <string>
#include <vector>

#include "ash/constants/web_app_id_constants.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom-shared.h"
#include "chrome/browser/web_applications/preinstalled_web_apps/preinstalled_web_app_definition_utils.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/grit/preinstalled_web_apps_resources.h"
#include "components/webapps/common/web_app_id.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom-shared.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/startup/browser_params_proxy.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "chrome/browser/ash/drive/file_system_util.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace web_app {
namespace {

// clang-format off
constexpr Translation kNameTranslations[] = {
    {"af", "Dokumente"},
    {"am", "ሰነዶች"},
    {"ar", "مستندات"},
    {"hy", "Փաստաթղթեր"},
    {"az", "Sənəd"},
    {"eu", "Dokumentuak"},
    {"be", "Дакументы"},
    {"bn", "Docs"},
    {"bg", "Документи"},
    {"my", "Docs"},
    {"ca", "Documents"},
    {"zh-HK", "Google 文件"},
    {"zh-CN", "Google 文档"},
    {"zh-TW", "文件"},
    {"hr", "Dokumenti"},
    {"cs", "Dokumenty"},
    {"da", "Docs"},
    {"nl", "Documenten"},
    {"en-AU", "Docs"},
    {"en-GB", "Docs"},
    {"et", "Dokumendid"},
    {"fil", "Docs"},
    {"fi", "Docs"},
    {"fr", "Docs"},
    {"fr-CA", "Documents"},
    {"gl", "Documentos"},
    {"ka", "Docs"},
    {"de", "Dokumente"},
    {"el", "Έγγραφα"},
    {"gu", "Docs"},
    {"iw", "Docs"},
    {"hi", "Docs"},
    {"hu", "Dokumentumok"},
    {"is", "Skjöl"},
    {"id", "Dokumen"},
    {"it", "Documenti"},
    {"ja", "ドキュメント"},
    {"kn", "Docs"},
    {"kk", "Құжаттар"},
    {"km", "ឯកសារ"},
    {"ko", "문서"},
    {"lo", "ເອກະສານ"},
    {"lv", "Dokumenti"},
    {"lt", "Dokumentai"},
    {"ms", "Dokumen"},
    {"ml", "Docs"},
    {"mr", "Docs"},
    {"mn", "Docs"},
    {"ne", "कागजात"},
    {"no", "Dokumenter"},
    {"or", "Docs"},
    {"fa", "سندنگار"},
    {"pl", "Dokumenty"},
    {"pt-BR", "Textos"},
    {"pt-PT", "Docs"},
    {"pa", "Docs"},
    {"ro", "Documente"},
    {"ru", "Документы"},
    {"sr", "Документи"},
    {"si", "Docs"},
    {"sk", "Dokumenty"},
    {"sl", "Dokumenti"},
    {"es", "Documentos"},
    {"es-419", "Documentos"},
    {"sw", "Hati za Google"},
    {"sv", "Dokument"},
    {"ta", "Docs"},
    {"te", "Docs"},
    {"th", "เอกสาร"},
    {"tr", "Dokümanlar"},
    {"uk", "Документи"},
    {"ur", "Docs"},
    {"vi", "Tài liệu"},
    {"cy", "Docs"},
    {"zu", "Amadokhumenti"},
};
// clang-format on

#if BUILDFLAG(IS_CHROMEOS)
bool IsDriveFsBulkPinningAvailable() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  return chromeos::BrowserParamsProxy::Get()->IsDriveFsBulkPinningAvailable();
#elif BUILDFLAG(IS_CHROMEOS_ASH)
  return drive::util::IsDriveFsBulkPinningAvailable();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace

ExternalInstallOptions GetConfigForGoogleDocs(bool is_standalone_tabbed) {
  ExternalInstallOptions options(
      /*install_url=*/GURL(
          "https://docs.google.com/document/installwebapp?usp=chrome_default"),
      /*user_display_mode=*/
      is_standalone_tabbed ? mojom::UserDisplayMode::kStandalone
                           : mojom::UserDisplayMode::kBrowser,
      /*install_source=*/ExternalInstallSource::kExternalDefault);

  options.user_type_allowlist = {"unmanaged", "managed", "child"};
  options.uninstall_and_replace.push_back("aohghmighlieiainnegkcijnfilokake");
  options.expected_app_id = kGoogleDocsAppId;

#if BUILDFLAG(IS_CHROMEOS)
  if (IsDriveFsBulkPinningAvailable()) {
    VLOG(1) << "DriveFsBulkPinning enabled, registering service worker";
    options.load_and_await_service_worker_registration = true;
    options.only_use_app_info_factory = false;
    options.service_worker_registration_url = GURL("https://docs.google.com");
    options.service_worker_registration_timeout = base::Minutes(10);
    return options;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  options.load_and_await_service_worker_registration = false;
  options.only_use_app_info_factory = true;
  options.app_info_factory = base::BindRepeating(
      [](bool is_standalone_tabbed) {
        GURL start_url =
            GURL("https://docs.google.com/document/?usp=installed_webapp");
        // `manifest_id` must remain fixed even if start_url changes.
        webapps::ManifestId manifest_id = GenerateManifestIdFromStartUrlOnly(
            GURL("https://docs.google.com/document/?usp=installed_webapp"));
        auto info = std::make_unique<WebAppInstallInfo>(manifest_id, start_url);
        info->title =
            base::UTF8ToUTF16(GetTranslatedName("Docs", kNameTranslations));
        info->scope = GURL("https://docs.google.com/document/");
        info->display_mode =
            is_standalone_tabbed ? DisplayMode::kTabbed : DisplayMode::kBrowser;
        info->icon_bitmaps.any = LoadBundledIcons(
            {IDR_PREINSTALLED_WEB_APPS_GOOGLE_DOCS_ICON_192_PNG});
        return info;
      },
      is_standalone_tabbed);

  return options;
}

}  // namespace web_app
