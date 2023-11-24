// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/search_engine_choice/icon_utils.h"

#include "base/check_op.h"
#include "build/branding_buildflags.h"
#include "components/grit/components_scaled_resources.h"
#include "content/public/browser/web_ui_data_source.h"

// This code is generated using `generate_search_engine_icons.py`. Don't modify
// it manually.
void AddGeneratedIconResources(content::WebUIDataSource* source,
                               const std::string& directory) {
  CHECK(source);
  CHECK_EQ(directory.back(), '/');
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  source->AddResourcePath(directory + "google_com.png", IDR_GOOGLE_COM_PNG);
#endif
  source->AddResourcePath(directory + "bing_com.png", IDR_BING_COM_PNG);
  source->AddResourcePath(directory + "search_brave_com.png",
                          IDR_SEARCH_BRAVE_COM_PNG);
  source->AddResourcePath(directory + "duckduckgo_com.png",
                          IDR_DUCKDUCKGO_COM_PNG);
  source->AddResourcePath(directory + "ecosia_org.png", IDR_ECOSIA_ORG_PNG);
  source->AddResourcePath(directory + "karmasearch_org.png",
                          IDR_KARMASEARCH_ORG_PNG);
  source->AddResourcePath(directory + "lilo_org.png", IDR_LILO_ORG_PNG);
  source->AddResourcePath(directory + "mail_ru.png", IDR_MAIL_RU_PNG);
  source->AddResourcePath(directory + "mojeek_com.png", IDR_MOJEEK_COM_PNG);
  source->AddResourcePath(directory + "nona_de.png", IDR_NONA_DE_PNG);
  source->AddResourcePath(directory + "panda_search_org.png",
                          IDR_PANDA_SEARCH_ORG_PNG);
  source->AddResourcePath(directory + "quendu_com.png", IDR_QUENDU_COM_PNG);
  source->AddResourcePath(directory + "qwant_com.png", IDR_QWANT_COM_PNG);
  source->AddResourcePath(directory + "seznam_cz.png", IDR_SEZNAM_CZ_PNG);
  source->AddResourcePath(directory + "seznam_sk.png", IDR_SEZNAM_SK_PNG);
  source->AddResourcePath(directory + "yahoo_com.png", IDR_YAHOO_COM_PNG);
  source->AddResourcePath(directory + "ar_yahoo_com.png", IDR_AR_YAHOO_COM_PNG);
  source->AddResourcePath(directory + "at_yahoo_com.png", IDR_AT_YAHOO_COM_PNG);
  source->AddResourcePath(directory + "au_yahoo_com.png", IDR_AU_YAHOO_COM_PNG);
  source->AddResourcePath(directory + "br_yahoo_com.png", IDR_BR_YAHOO_COM_PNG);
  source->AddResourcePath(directory + "ca_yahoo_com.png", IDR_CA_YAHOO_COM_PNG);
  source->AddResourcePath(directory + "ch_yahoo_com.png", IDR_CH_YAHOO_COM_PNG);
  source->AddResourcePath(directory + "cl_yahoo_com.png", IDR_CL_YAHOO_COM_PNG);
  source->AddResourcePath(directory + "co_yahoo_com.png", IDR_CO_YAHOO_COM_PNG);
  source->AddResourcePath(directory + "de_yahoo_com.png", IDR_DE_YAHOO_COM_PNG);
  source->AddResourcePath(directory + "dk_yahoo_com.png", IDR_DK_YAHOO_COM_PNG);
  source->AddResourcePath(directory + "es_yahoo_com.png", IDR_ES_YAHOO_COM_PNG);
  source->AddResourcePath(directory + "fi_yahoo_com.png", IDR_FI_YAHOO_COM_PNG);
  source->AddResourcePath(directory + "fr_yahoo_com.png", IDR_FR_YAHOO_COM_PNG);
  source->AddResourcePath(directory + "hk_yahoo_com.png", IDR_HK_YAHOO_COM_PNG);
  source->AddResourcePath(directory + "id_yahoo_com.png", IDR_ID_YAHOO_COM_PNG);
  source->AddResourcePath(directory + "in_yahoo_com.png", IDR_IN_YAHOO_COM_PNG);
  source->AddResourcePath(directory + "yahoo_co_jp.png", IDR_YAHOO_CO_JP_PNG);
  source->AddResourcePath(directory + "mx_yahoo_com.png", IDR_MX_YAHOO_COM_PNG);
  source->AddResourcePath(directory + "malaysia_yahoo_com.png",
                          IDR_MALAYSIA_YAHOO_COM_PNG);
  source->AddResourcePath(directory + "nl_yahoo_com.png", IDR_NL_YAHOO_COM_PNG);
  source->AddResourcePath(directory + "nz_yahoo_com.png", IDR_NZ_YAHOO_COM_PNG);
  source->AddResourcePath(directory + "pe_yahoo_com.png", IDR_PE_YAHOO_COM_PNG);
  source->AddResourcePath(directory + "ph_yahoo_com.png", IDR_PH_YAHOO_COM_PNG);
  source->AddResourcePath(directory + "se_yahoo_com.png", IDR_SE_YAHOO_COM_PNG);
  source->AddResourcePath(directory + "sg_yahoo_com.png", IDR_SG_YAHOO_COM_PNG);
  source->AddResourcePath(directory + "th_yahoo_com.png", IDR_TH_YAHOO_COM_PNG);
  source->AddResourcePath(directory + "tr_yahoo_com.png", IDR_TR_YAHOO_COM_PNG);
  source->AddResourcePath(directory + "tw_yahoo_com.png", IDR_TW_YAHOO_COM_PNG);
  source->AddResourcePath(directory + "uk_yahoo_com.png", IDR_UK_YAHOO_COM_PNG);
  source->AddResourcePath(directory + "yandex_by.png", IDR_YANDEX_BY_PNG);
  source->AddResourcePath(directory + "yandex_com.png", IDR_YANDEX_COM_PNG);
  source->AddResourcePath(directory + "yandex_kz.png", IDR_YANDEX_KZ_PNG);
  source->AddResourcePath(directory + "yandex_ru.png", IDR_YANDEX_RU_PNG);
  source->AddResourcePath(directory + "yandex_com_tr.png",
                          IDR_YANDEX_COM_TR_PNG);
  source->AddResourcePath(directory + "yep_com.png", IDR_YEP_COM_PNG);
  source->AddResourcePath(directory + "info_com.png", IDR_INFO_COM_PNG);
  source->AddResourcePath(directory + "metager_de.png", IDR_METAGER_DE_PNG);
  source->AddResourcePath(directory + "oceanhero_today.png",
                          IDR_OCEANHERO_TODAY_PNG);
  source->AddResourcePath(directory + "privacywall_org.png",
                          IDR_PRIVACYWALL_ORG_PNG);
}
