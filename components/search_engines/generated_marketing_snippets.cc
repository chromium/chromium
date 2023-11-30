// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/search_engine_choice_utils.h"

#include "base/strings/utf_string_conversions.h"
#include "components/search_engines/prepopulated_engines.h"
#include "components/strings/grit/search_engine_descriptions_strings.h"

namespace search_engines {
// This file is generated using
// tools/search_engine_choice/generate_search_engine_snippets.py. Do not modify
// it manually.
int GetMarketingSnippetResourceId(const std::u16string& engine_keyword) {
  if (engine_keyword ==
      base::WideToUTF16(TemplateURLPrepopulateData::bing.keyword)) {
    return IDS_BING_SEARCH_DESCRIPTION;
  }
  if (engine_keyword ==
      base::WideToUTF16(TemplateURLPrepopulateData::brave.keyword)) {
    return IDS_BRAVE_SEARCH_DESCRIPTION;
  }
  if (engine_keyword ==
      base::WideToUTF16(TemplateURLPrepopulateData::duckduckgo.keyword)) {
    return IDS_DUCKDUCKGO_SEARCH_DESCRIPTION;
  }
  if (engine_keyword ==
      base::WideToUTF16(TemplateURLPrepopulateData::ecosia.keyword)) {
    return IDS_ECOSIA_SEARCH_DESCRIPTION;
  }
  if (engine_keyword ==
      base::WideToUTF16(TemplateURLPrepopulateData::google.keyword)) {
    return IDS_GOOGLE_SEARCH_DESCRIPTION;
  }
  if (engine_keyword ==
      base::WideToUTF16(TemplateURLPrepopulateData::info_com.keyword)) {
    return IDS_INFOCOM_SEARCH_DESCRIPTION;
  }
  if (engine_keyword ==
      base::WideToUTF16(TemplateURLPrepopulateData::karma.keyword)) {
    return IDS_KARMA_SEARCH_DESCRIPTION;
  }
  if (engine_keyword ==
      base::WideToUTF16(TemplateURLPrepopulateData::lilo.keyword)) {
    return IDS_LILO_SEARCH_DESCRIPTION;
  }
  if (engine_keyword ==
      base::WideToUTF16(TemplateURLPrepopulateData::metager_de.keyword)) {
    return IDS_METAGER_SEARCH_DESCRIPTION;
  }
  if (engine_keyword ==
      base::WideToUTF16(TemplateURLPrepopulateData::mojeek.keyword)) {
    return IDS_MOJEEK_SEARCH_DESCRIPTION;
  }
  if (engine_keyword ==
      base::WideToUTF16(TemplateURLPrepopulateData::nona.keyword)) {
    return IDS_NONA_SEARCH_DESCRIPTION;
  }
  if (engine_keyword ==
      base::WideToUTF16(TemplateURLPrepopulateData::oceanhero.keyword)) {
    return IDS_OCEANHERO_SEARCH_DESCRIPTION;
  }
  if (engine_keyword ==
      base::WideToUTF16(TemplateURLPrepopulateData::panda.keyword)) {
    return IDS_PANDA_SEARCH_DESCRIPTION;
  }
  if (engine_keyword ==
      base::WideToUTF16(TemplateURLPrepopulateData::privacywall.keyword)) {
    return IDS_PRIVACYWALL_SEARCH_DESCRIPTION;
  }
  if (engine_keyword ==
      base::WideToUTF16(TemplateURLPrepopulateData::quendu.keyword)) {
    return IDS_QUENDU_SEARCH_DESCRIPTION;
  }
  if (engine_keyword ==
      base::WideToUTF16(TemplateURLPrepopulateData::qwant.keyword)) {
    return IDS_QWANT_SEARCH_DESCRIPTION;
  }
  if (engine_keyword ==
      base::WideToUTF16(TemplateURLPrepopulateData::seznam_cz.keyword)) {
    return IDS_SEZNAM_SEARCH_DESCRIPTION;
  }
  if (engine_keyword ==
      base::WideToUTF16(TemplateURLPrepopulateData::seznam_sk.keyword)) {
    return IDS_SEZNAM_SEARCH_DESCRIPTION;
  }
  if (engine_keyword ==
      base::WideToUTF16(TemplateURLPrepopulateData::yahoo.keyword)) {
    return IDS_YAHOO_SEARCH_DESCRIPTION;
  }
  if (engine_keyword ==
      base::WideToUTF16(TemplateURLPrepopulateData::yahoo_ar.keyword)) {
    return IDS_YAHOO_SEARCH_DESCRIPTION;
  }
  if (engine_keyword ==
      base::WideToUTF16(TemplateURLPrepopulateData::yahoo_at.keyword)) {
    return IDS_YAHOO_SEARCH_DESCRIPTION;
  }
  if (engine_keyword ==
      base::WideToUTF16(TemplateURLPrepopulateData::yahoo_au.keyword)) {
    return IDS_YAHOO_SEARCH_DESCRIPTION;
  }
  if (engine_keyword ==
      base::WideToUTF16(TemplateURLPrepopulateData::yahoo_br.keyword)) {
    return IDS_YAHOO_SEARCH_DESCRIPTION;
  }
  if (engine_keyword ==
      base::WideToUTF16(TemplateURLPrepopulateData::yahoo_ca.keyword)) {
    return IDS_YAHOO_SEARCH_DESCRIPTION;
  }
  if (engine_keyword ==
      base::WideToUTF16(TemplateURLPrepopulateData::yahoo_ch.keyword)) {
    return IDS_YAHOO_SEARCH_DESCRIPTION;
  }
  if (engine_keyword ==
      base::WideToUTF16(TemplateURLPrepopulateData::yahoo_cl.keyword)) {
    return IDS_YAHOO_SEARCH_DESCRIPTION;
  }
  if (engine_keyword ==
      base::WideToUTF16(TemplateURLPrepopulateData::yahoo_co.keyword)) {
    return IDS_YAHOO_SEARCH_DESCRIPTION;
  }
  if (engine_keyword ==
      base::WideToUTF16(TemplateURLPrepopulateData::yahoo_de.keyword)) {
    return IDS_YAHOO_SEARCH_DESCRIPTION;
  }
  if (engine_keyword ==
      base::WideToUTF16(TemplateURLPrepopulateData::yahoo_dk.keyword)) {
    return IDS_YAHOO_SEARCH_DESCRIPTION;
  }
  if (engine_keyword ==
      base::WideToUTF16(TemplateURLPrepopulateData::yahoo_es.keyword)) {
    return IDS_YAHOO_SEARCH_DESCRIPTION;
  }
  if (engine_keyword ==
      base::WideToUTF16(TemplateURLPrepopulateData::yahoo_fi.keyword)) {
    return IDS_YAHOO_SEARCH_DESCRIPTION;
  }
  if (engine_keyword ==
      base::WideToUTF16(TemplateURLPrepopulateData::yahoo_fr.keyword)) {
    return IDS_YAHOO_SEARCH_DESCRIPTION;
  }
  if (engine_keyword ==
      base::WideToUTF16(TemplateURLPrepopulateData::yahoo_hk.keyword)) {
    return IDS_YAHOO_SEARCH_DESCRIPTION;
  }
  if (engine_keyword ==
      base::WideToUTF16(TemplateURLPrepopulateData::yahoo_id.keyword)) {
    return IDS_YAHOO_SEARCH_DESCRIPTION;
  }
  if (engine_keyword ==
      base::WideToUTF16(TemplateURLPrepopulateData::yahoo_in.keyword)) {
    return IDS_YAHOO_SEARCH_DESCRIPTION;
  }
  if (engine_keyword ==
      base::WideToUTF16(TemplateURLPrepopulateData::yahoo_jp.keyword)) {
    return IDS_YAHOO_SEARCH_DESCRIPTION;
  }
  if (engine_keyword ==
      base::WideToUTF16(TemplateURLPrepopulateData::yahoo_mx.keyword)) {
    return IDS_YAHOO_SEARCH_DESCRIPTION;
  }
  if (engine_keyword ==
      base::WideToUTF16(TemplateURLPrepopulateData::yahoo_my.keyword)) {
    return IDS_YAHOO_SEARCH_DESCRIPTION;
  }
  if (engine_keyword ==
      base::WideToUTF16(TemplateURLPrepopulateData::yahoo_nl.keyword)) {
    return IDS_YAHOO_SEARCH_DESCRIPTION;
  }
  if (engine_keyword ==
      base::WideToUTF16(TemplateURLPrepopulateData::yahoo_nz.keyword)) {
    return IDS_YAHOO_SEARCH_DESCRIPTION;
  }
  if (engine_keyword ==
      base::WideToUTF16(TemplateURLPrepopulateData::yahoo_pe.keyword)) {
    return IDS_YAHOO_SEARCH_DESCRIPTION;
  }
  if (engine_keyword ==
      base::WideToUTF16(TemplateURLPrepopulateData::yahoo_ph.keyword)) {
    return IDS_YAHOO_SEARCH_DESCRIPTION;
  }
  if (engine_keyword ==
      base::WideToUTF16(TemplateURLPrepopulateData::yahoo_se.keyword)) {
    return IDS_YAHOO_SEARCH_DESCRIPTION;
  }
  if (engine_keyword ==
      base::WideToUTF16(TemplateURLPrepopulateData::yahoo_sg.keyword)) {
    return IDS_YAHOO_SEARCH_DESCRIPTION;
  }
  if (engine_keyword ==
      base::WideToUTF16(TemplateURLPrepopulateData::yahoo_th.keyword)) {
    return IDS_YAHOO_SEARCH_DESCRIPTION;
  }
  if (engine_keyword ==
      base::WideToUTF16(TemplateURLPrepopulateData::yahoo_tr.keyword)) {
    return IDS_YAHOO_SEARCH_DESCRIPTION;
  }
  if (engine_keyword ==
      base::WideToUTF16(TemplateURLPrepopulateData::yahoo_tw.keyword)) {
    return IDS_YAHOO_SEARCH_DESCRIPTION;
  }
  if (engine_keyword ==
      base::WideToUTF16(TemplateURLPrepopulateData::yahoo_uk.keyword)) {
    return IDS_YAHOO_SEARCH_DESCRIPTION;
  }
  if (engine_keyword ==
      base::WideToUTF16(TemplateURLPrepopulateData::yandex_by.keyword)) {
    return IDS_YANDEX_SEARCH_DESCRIPTION;
  }
  if (engine_keyword ==
      base::WideToUTF16(TemplateURLPrepopulateData::yandex_com.keyword)) {
    return IDS_YANDEX_SEARCH_DESCRIPTION;
  }
  if (engine_keyword ==
      base::WideToUTF16(TemplateURLPrepopulateData::yandex_kz.keyword)) {
    return IDS_YANDEX_SEARCH_DESCRIPTION;
  }
  if (engine_keyword ==
      base::WideToUTF16(TemplateURLPrepopulateData::yandex_ru.keyword)) {
    return IDS_YANDEX_SEARCH_DESCRIPTION;
  }
  if (engine_keyword ==
      base::WideToUTF16(TemplateURLPrepopulateData::yandex_tr.keyword)) {
    return IDS_YANDEX_SEARCH_DESCRIPTION;
  }
  if (engine_keyword ==
      base::WideToUTF16(TemplateURLPrepopulateData::yep.keyword)) {
    return IDS_YEP_SEARCH_DESCRIPTION;
  }
  return -1;
}
}  // namespace search_engines
