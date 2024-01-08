// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/fixed_flat_map.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/ui/webui/search_engine_choice/icon_utils.h"
// This code is generated
// using`tools/search_engine_choice/generate_search_engine_icons.py`. Don't
// modify it manually.

namespace {

constexpr auto kSearchEngineIconPathMap =
    base::MakeFixedFlatMap<std::wstring_view, std::string_view>(
        {{L"bing.com", "chrome://theme/IDR_BING_COM_PNG"},
         {L"search.brave.com", "chrome://theme/IDR_SEARCH_BRAVE_COM_PNG"},
         {L"duckduckgo.com", "chrome://theme/IDR_DUCKDUCKGO_COM_PNG"},
         {L"ecosia.org", "chrome://theme/IDR_ECOSIA_ORG_PNG"},
         {L"karmasearch.org", "chrome://theme/IDR_KARMASEARCH_ORG_PNG"},
         {L"lilo.org", "chrome://theme/IDR_LILO_ORG_PNG"},
         {L"mail.ru", "chrome://theme/IDR_MAIL_RU_PNG"},
         {L"mojeek.com", "chrome://theme/IDR_MOJEEK_COM_PNG"},
         {L"nona.de", "chrome://theme/IDR_NONA_DE_PNG"},
         {L"panda-search.org", "chrome://theme/IDR_PANDA_SEARCH_ORG_PNG"},
         {L"quendu.com", "chrome://theme/IDR_QUENDU_COM_PNG"},
         {L"qwant.com", "chrome://theme/IDR_QWANT_COM_PNG"},
         {L"seznam.cz", "chrome://theme/IDR_SEZNAM_CZ_PNG"},
         {L"seznam.sk", "chrome://theme/IDR_SEZNAM_SK_PNG"},
         {L"yahoo.com", "chrome://theme/IDR_YAHOO_COM_PNG"},
         {L"ar.yahoo.com", "chrome://theme/IDR_AR_YAHOO_COM_PNG"},
         {L"at.yahoo.com", "chrome://theme/IDR_AT_YAHOO_COM_PNG"},
         {L"au.yahoo.com", "chrome://theme/IDR_AU_YAHOO_COM_PNG"},
         {L"br.yahoo.com", "chrome://theme/IDR_BR_YAHOO_COM_PNG"},
         {L"ca.yahoo.com", "chrome://theme/IDR_CA_YAHOO_COM_PNG"},
         {L"ch.yahoo.com", "chrome://theme/IDR_CH_YAHOO_COM_PNG"},
         {L"cl.yahoo.com", "chrome://theme/IDR_CL_YAHOO_COM_PNG"},
         {L"co.yahoo.com", "chrome://theme/IDR_CO_YAHOO_COM_PNG"},
         {L"de.yahoo.com", "chrome://theme/IDR_DE_YAHOO_COM_PNG"},
         {L"dk.yahoo.com", "chrome://theme/IDR_DK_YAHOO_COM_PNG"},
         {L"es.yahoo.com", "chrome://theme/IDR_ES_YAHOO_COM_PNG"},
         {L"fi.yahoo.com", "chrome://theme/IDR_FI_YAHOO_COM_PNG"},
         {L"fr.yahoo.com", "chrome://theme/IDR_FR_YAHOO_COM_PNG"},
         {L"hk.yahoo.com", "chrome://theme/IDR_HK_YAHOO_COM_PNG"},
         {L"id.yahoo.com", "chrome://theme/IDR_ID_YAHOO_COM_PNG"},
         {L"in.yahoo.com", "chrome://theme/IDR_IN_YAHOO_COM_PNG"},
         {L"yahoo.co.jp", "chrome://theme/IDR_YAHOO_CO_JP_PNG"},
         {L"mx.yahoo.com", "chrome://theme/IDR_MX_YAHOO_COM_PNG"},
         {L"malaysia.yahoo.com", "chrome://theme/IDR_MALAYSIA_YAHOO_COM_PNG"},
         {L"nl.yahoo.com", "chrome://theme/IDR_NL_YAHOO_COM_PNG"},
         {L"nz.yahoo.com", "chrome://theme/IDR_NZ_YAHOO_COM_PNG"},
         {L"pe.yahoo.com", "chrome://theme/IDR_PE_YAHOO_COM_PNG"},
         {L"ph.yahoo.com", "chrome://theme/IDR_PH_YAHOO_COM_PNG"},
         {L"se.yahoo.com", "chrome://theme/IDR_SE_YAHOO_COM_PNG"},
         {L"sg.yahoo.com", "chrome://theme/IDR_SG_YAHOO_COM_PNG"},
         {L"th.yahoo.com", "chrome://theme/IDR_TH_YAHOO_COM_PNG"},
         {L"tr.yahoo.com", "chrome://theme/IDR_TR_YAHOO_COM_PNG"},
         {L"tw.yahoo.com", "chrome://theme/IDR_TW_YAHOO_COM_PNG"},
         {L"uk.yahoo.com", "chrome://theme/IDR_UK_YAHOO_COM_PNG"},
         {L"yandex.by", "chrome://theme/IDR_YANDEX_BY_PNG"},
         {L"yandex.com", "chrome://theme/IDR_YANDEX_COM_PNG"},
         {L"yandex.kz", "chrome://theme/IDR_YANDEX_KZ_PNG"},
         {L"yandex.ru", "chrome://theme/IDR_YANDEX_RU_PNG"},
         {L"yandex.com.tr", "chrome://theme/IDR_YANDEX_COM_TR_PNG"},
         {L"yep.com", "chrome://theme/IDR_YEP_COM_PNG"},
         {L"info.com", "chrome://theme/IDR_INFO_COM_PNG"},
         {L"metager.de", "chrome://theme/IDR_METAGER_DE_PNG"},
         {L"oceanhero.today", "chrome://theme/IDR_OCEANHERO_TODAY_PNG"},
         {L"privacywall.org", "chrome://theme/IDR_PRIVACYWALL_ORG_PNG"},
         {L"google.com",
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
          "chrome://theme/IDR_GOOGLE_COM_PNG"
#else
          "chrome://theme/IDR_DEFAULT_FAVICON"
#endif
         }});

}  // namespace

std::string_view GetSearchEngineGeneratedIconPath(
    const std::u16string& engine_keyword) {
  const base::fixed_flat_map<std::wstring_view, std::string_view,
                             kSearchEngineIconPathMap.size()>::const_iterator
      iterator =
          kSearchEngineIconPathMap.find(base::UTF16ToWide(engine_keyword));
  return iterator == kSearchEngineIconPathMap.cend() ? std::string_view()
                                                     : iterator->second;
}
