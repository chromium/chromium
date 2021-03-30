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
    {"af", u8"Dokumente"},
    {"am", u8"ሰነዶች"},
    {"ar", u8"مستندات"},
    {"hy", u8"Փաստաթղթեր"},
    {"az", u8"Sənəd"},
    {"eu", u8"Dokumentuak"},
    {"be", u8"Дакументы"},
    {"bn", u8"Docs"},
    {"bg", u8"Документи"},
    {"my", u8"Docs"},
    {"ca", u8"Documents"},
    {"zh-HK", u8"Google 文件"},
    {"zh-CN", u8"Google 文档"},
    {"zh-TW", u8"文件"},
    {"hr", u8"Dokumenti"},
    {"cs", u8"Dokumenty"},
    {"da", u8"Docs"},
    {"nl", u8"Documenten"},
    {"en-AU", u8"Docs"},
    {"en-GB", u8"Docs"},
    {"et", u8"Dokumendid"},
    {"fil", u8"Docs"},
    {"fi", u8"Docs"},
    {"fr", u8"Docs"},
    {"fr-CA", u8"Documents"},
    {"gl", u8"Documentos"},
    {"ka", u8"Docs"},
    {"de", u8"Dokumente"},
    {"el", u8"Έγγραφα"},
    {"gu", u8"Docs"},
    {"iw", u8"Docs"},
    {"hi", u8"Docs"},
    {"hu", u8"Dokumentumok"},
    {"is", u8"Skjöl"},
    {"id", u8"Dokumen"},
    {"it", u8"Documenti"},
    {"ja", u8"ドキュメント"},
    {"kn", u8"Docs"},
    {"kk", u8"Құжаттар"},
    {"km", u8"ឯកសារ"},
    {"ko", u8"문서"},
    {"lo", u8"ເອກະສານ"},
    {"lv", u8"Dokumenti"},
    {"lt", u8"Dokumentai"},
    {"ms", u8"Dokumen"},
    {"ml", u8"Docs"},
    {"mr", u8"Docs"},
    {"mn", u8"Docs"},
    {"ne", u8"कागजात"},
    {"no", u8"Dokumenter"},
    {"or", u8"Docs"},
    {"fa", u8"سندنگار"},
    {"pl", u8"Dokumenty"},
    {"pt-BR", u8"Textos"},
    {"pt-PT", u8"Docs"},
    {"pa", u8"Docs"},
    {"ro", u8"Documente"},
    {"ru", u8"Документы"},
    {"sr", u8"Документи"},
    {"si", u8"Docs"},
    {"sk", u8"Dokumenty"},
    {"sl", u8"Dokumenti"},
    {"es", u8"Documentos"},
    {"es-419", u8"Documentos"},
    {"sw", u8"Hati za Google"},
    {"sv", u8"Dokument"},
    {"ta", u8"Docs"},
    {"te", u8"Docs"},
    {"th", u8"เอกสาร"},
    {"tr", u8"Dokümanlar"},
    {"uk", u8"Документи"},
    {"ur", u8"Docs"},
    {"vi", u8"Tài liệu"},
    {"cy", u8"Docs"},
    {"zu", u8"Amadokhumenti"},
};
// clang-format on

}  // namespace

ExternalInstallOptions GetConfigForGoogleDocs() {
  ExternalInstallOptions options(
      /*install_url=*/GURL(
          "https://docs.google.com/document/installwebapp?usp=chrome_default"),
      /*user_display_mode=*/DisplayMode::kBrowser,
      /*install_source=*/ExternalInstallSource::kExternalDefault);

  options.user_type_allowlist = {"unmanaged", "managed", "child"};
  options.gate_on_feature = kMigrateDefaultChromeAppToWebAppsGSuite.name;
  options.uninstall_and_replace.push_back("aohghmighlieiainnegkcijnfilokake");
  options.load_and_await_service_worker_registration = false;
  options.only_use_app_info_factory = true;
  options.app_info_factory = base::BindRepeating([]() {
    auto info = std::make_unique<WebApplicationInfo>();
    info->title =
        base::UTF8ToUTF16(GetTranslatedName("Docs", kNameTranslations));
    info->start_url =
        GURL("https://docs.google.com/document/?usp=installed_webapp");
    info->scope = GURL("https://docs.google.com/document/");
    info->display_mode = DisplayMode::kBrowser;
    info->icon_bitmaps.any =
        LoadBundledIcons({IDR_PREINSTALLED_WEB_APPS_GOOGLE_DOCS_ICON_192_PNG});
    return info;
  });

  return options;
}

}  // namespace web_app
