// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/search_engine_choice_utils.h"

#include "base/containers/fixed_flat_map.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "components/grit/components_scaled_resources.h"
#include "ui/resources/grit/ui_resources.h"

// This code is generated
// using`tools/search_engine_choice/generate_search_engine_icons.py`. Don't
// modify it manually.

namespace {

constexpr auto kSearchEngineResourceIdMap =
    base::MakeFixedFlatMap<std::wstring_view, int>(
        {{L"bing.com", IDR_BING_COM_PNG},
         {L"search.brave.com", IDR_SEARCH_BRAVE_COM_PNG},
         {L"duckduckgo.com", IDR_DUCKDUCKGO_COM_PNG},
         {L"ecosia.org", IDR_ECOSIA_ORG_PNG},
         {L"karmasearch.org", IDR_KARMASEARCH_ORG_PNG},
         {L"lilo.org", IDR_LILO_ORG_PNG},
         {L"mail.ru", IDR_MAIL_RU_PNG},
         {L"mojeek.com", IDR_MOJEEK_COM_PNG},
         {L"nona.de", IDR_NONA_DE_PNG},
         {L"panda-search.org", IDR_PANDA_SEARCH_ORG_PNG},
         {L"quendu.com", IDR_QUENDU_COM_PNG},
         {L"qwant.com", IDR_QWANT_COM_PNG},
         {L"seznam.cz", IDR_SEZNAM_CZ_PNG},
         {L"seznam.sk", IDR_SEZNAM_SK_PNG},
         {L"yahoo.com", IDR_YAHOO_COM_PNG},
         {L"ar.yahoo.com", IDR_AR_YAHOO_COM_PNG},
         {L"at.yahoo.com", IDR_AT_YAHOO_COM_PNG},
         {L"au.yahoo.com", IDR_AU_YAHOO_COM_PNG},
         {L"br.yahoo.com", IDR_BR_YAHOO_COM_PNG},
         {L"ca.yahoo.com", IDR_CA_YAHOO_COM_PNG},
         {L"ch.yahoo.com", IDR_CH_YAHOO_COM_PNG},
         {L"cl.yahoo.com", IDR_CL_YAHOO_COM_PNG},
         {L"co.yahoo.com", IDR_CO_YAHOO_COM_PNG},
         {L"de.yahoo.com", IDR_DE_YAHOO_COM_PNG},
         {L"dk.yahoo.com", IDR_DK_YAHOO_COM_PNG},
         {L"es.yahoo.com", IDR_ES_YAHOO_COM_PNG},
         {L"fi.yahoo.com", IDR_FI_YAHOO_COM_PNG},
         {L"fr.yahoo.com", IDR_FR_YAHOO_COM_PNG},
         {L"hk.yahoo.com", IDR_HK_YAHOO_COM_PNG},
         {L"id.yahoo.com", IDR_ID_YAHOO_COM_PNG},
         {L"in.yahoo.com", IDR_IN_YAHOO_COM_PNG},
         {L"yahoo.co.jp", IDR_YAHOO_CO_JP_PNG},
         {L"mx.yahoo.com", IDR_MX_YAHOO_COM_PNG},
         {L"malaysia.yahoo.com", IDR_MALAYSIA_YAHOO_COM_PNG},
         {L"nl.yahoo.com", IDR_NL_YAHOO_COM_PNG},
         {L"nz.yahoo.com", IDR_NZ_YAHOO_COM_PNG},
         {L"pe.yahoo.com", IDR_PE_YAHOO_COM_PNG},
         {L"ph.yahoo.com", IDR_PH_YAHOO_COM_PNG},
         {L"se.yahoo.com", IDR_SE_YAHOO_COM_PNG},
         {L"sg.yahoo.com", IDR_SG_YAHOO_COM_PNG},
         {L"th.yahoo.com", IDR_TH_YAHOO_COM_PNG},
         {L"tr.yahoo.com", IDR_TR_YAHOO_COM_PNG},
         {L"tw.yahoo.com", IDR_TW_YAHOO_COM_PNG},
         {L"uk.yahoo.com", IDR_UK_YAHOO_COM_PNG},
         {L"yandex.by", IDR_YANDEX_BY_PNG},
         {L"yandex.com", IDR_YANDEX_COM_PNG},
         {L"yandex.kz", IDR_YANDEX_KZ_PNG},
         {L"yandex.ru", IDR_YANDEX_RU_PNG},
         {L"yandex.com.tr", IDR_YANDEX_COM_TR_PNG},
         {L"yep.com", IDR_YEP_COM_PNG},
         {L"info.com", IDR_INFO_COM_PNG},
         {L"metager.de", IDR_METAGER_DE_PNG},
         {L"oceanhero.today", IDR_OCEANHERO_TODAY_PNG},
         {L"privacywall.org", IDR_PRIVACYWALL_ORG_PNG},
         {L"google.com",
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
          IDR_GOOGLE_COM_PNG
#else
          IDR_DEFAULT_FAVICON
#endif
         }});

}  // namespace

namespace search_engines {

int GetIconResourceId(const std::u16string& engine_keyword) {
  const base::fixed_flat_map<std::wstring_view, int,
                             kSearchEngineResourceIdMap.size()>::const_iterator
      iterator =
          kSearchEngineResourceIdMap.find(base::UTF16ToWide(engine_keyword));
  return iterator == kSearchEngineResourceIdMap.cend() ? -1 : iterator->second;
}

}  // namespace search_engines
