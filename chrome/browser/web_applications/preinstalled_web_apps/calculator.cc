// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/preinstalled_web_apps/calculator.h"

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/web_applications/preinstalled_app_install_features.h"
#include "chrome/browser/web_applications/preinstalled_web_apps/preinstalled_web_app_definition_utils.h"
#include "chrome/browser/web_applications/web_application_info.h"
#include "chrome/grit/preinstalled_web_apps_resources.h"

namespace web_app {

namespace {

// clang-format off
constexpr Translation kNameTranslations[] = {
  {"am", u8"ሒሳብ ማስያ ማሽን"},
  {"ar", u8"الآلة الحاسبة"},
  {"bg", u8"Калкулатор"},
  {"bn", u8"ক্যালকুলেটর"},
  {"ca", u8"Calculadora"},
  {"cs", u8"Kalkulačka"},
  {"da", u8"Lommeregner"},
  {"de", u8"Rechner"},
  {"el", u8"Αριθμομηχανή"},
  {"en-GB", u8"Calculator"},
  {"en", u8"Calculator"},
  {"es-419", u8"Calculadora"},
  {"es", u8"Calculadora"},
  {"et", u8"Kalkulaator"},
  {"fa", u8"ماشین حساب"},
  {"fil", u8"Calculator"},
  {"fi", u8"Laskin"},
  {"fr", u8"Calculatrice"},
  {"gu", u8"કેલ્ક્યુલેટર"},
  {"hi", u8"कैल्‍क्‍यूलेटर"},
  {"hr", u8"Kalkulator"},
  {"hu", u8"Számológép"},
  {"id", u8"Kalkulator"},
  {"it", u8"Calcolatrice"},
  {"iw", u8"מחשבון"},
  {"ja", u8"電卓"},
  {"kn", u8"ಕ್ಯಾಲ್ಕುಲೇಟರ್"},
  {"ko", u8"계산기"},
  {"lt", u8"Skaičiuotuvas"},
  {"lv", u8"Kalkulators"},
  {"ml", u8"കാൽക്കുലേറ്റർ"},
  {"mr", u8"कॅलक्युलेटर"},
  {"ms", u8"Kalkulator"},
  {"nl", u8"Rekenmachine"},
  {"no", u8"Kalkulator"},
  {"pl", u8"Kalkulator"},
  {"pt-BR", u8"Calculadora"},
  {"pt-PT", u8"Calculadora"},
  {"ro", u8"Calculator"},
  {"ru", u8"Калькулятор"},
  {"sk", u8"Kalkulačka"},
  {"sl", u8"Računalo"},
  {"sr", u8"Калкулатор"},
  {"sv", u8"Kalkylator"},
  {"sw", u8"Kikokotoo"},
  {"ta", u8"கால்குலேட்டர்"},
  {"te", u8"కాలిక్యులేటర్"},
  {"th", u8"เครื่องคิดเลข"},
  {"tr", u8"Hesap Makinesi"},
  {"uk", u8"Калькулятор"},
  {"vi", u8"Máy tính"},
  {"zh-CN", u8"计算器"},
  {"zh-TW", u8"計算機"},
};
// clang-format on

}  // namespace

ExternalInstallOptions GetConfigForCalculator() {
  ExternalInstallOptions options(
      /*install_url=*/GURL("https://calculator.apps.chrome/install"),
      /*user_display_mode=*/DisplayMode::kStandalone,
      /*install_source=*/ExternalInstallSource::kExternalDefault);

  options.user_type_allowlist = {"unmanaged", "managed", "child"};
  options.gate_on_feature = kDefaultCalculatorWebApp.name;
  options.uninstall_and_replace.push_back("joodangkbfjnajiiifokapkpmhfnpleo");

  options.only_use_app_info_factory = true;
  options.app_info_factory = base::BindRepeating([]() {
    auto info = std::make_unique<WebApplicationInfo>();
    info->title =
        base::UTF8ToUTF16(GetTranslatedName("Calculator", kNameTranslations));
    info->start_url = GURL("https://calculator.apps.chrome/");
    info->scope = GURL("https://calculator.apps.chrome/");
    info->display_mode = DisplayMode::kStandalone;
    info->icon_bitmaps.any =
        LoadBundledIcons({IDR_PREINSTALLED_WEB_APPS_CALCULATOR_ICON_256_PNG});
    info->background_color = 0xFFFFFFFF;
    return info;
  });

  return options;
}

}  // namespace web_app
