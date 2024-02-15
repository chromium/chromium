// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/search_engine_choice_utils.h"

#include "components/search_engines/prepopulated_engines.h"
#include "components/strings/grit/search_engine_descriptions_strings.h"

namespace search_engines {
// This file is generated using
// google3/googleclient/chrome/tools/search_engine_choice/generate_marketing_snippets.py.
// Do not modify it manually.
int GetMarketingSnippetResourceId(const std::u16string& engine_keyword) {
  if (engine_keyword == TemplateURLPrepopulateData::bing.keyword) {
    return IDS_BING_SEARCH_DESCRIPTION;
  }
  if (engine_keyword == TemplateURLPrepopulateData::brave.keyword) {
    return IDS_BRAVE_SEARCH_DESCRIPTION;
  }
  if (engine_keyword == TemplateURLPrepopulateData::duckduckgo.keyword) {
    return IDS_DUCKDUCKGO_SEARCH_DESCRIPTION;
  }
  if (engine_keyword == TemplateURLPrepopulateData::ecosia.keyword) {
    return IDS_ECOSIA_SEARCH_DESCRIPTION;
  }
  if (engine_keyword == TemplateURLPrepopulateData::google.keyword) {
    return IDS_GOOGLE_SEARCH_DESCRIPTION;
  }
  if (engine_keyword == TemplateURLPrepopulateData::info_com.keyword) {
    return IDS_INFOCOM_SEARCH_DESCRIPTION;
  }
  if (engine_keyword == TemplateURLPrepopulateData::karma.keyword) {
    return IDS_KARMA_SEARCH_DESCRIPTION;
  }
  if (engine_keyword == TemplateURLPrepopulateData::lilo.keyword) {
    return IDS_LILO_SEARCH_DESCRIPTION;
  }
  if (engine_keyword == TemplateURLPrepopulateData::metager_de.keyword) {
    return IDS_METAGER_SEARCH_DESCRIPTION;
  }
  if (engine_keyword == TemplateURLPrepopulateData::mojeek.keyword) {
    return IDS_MOJEEK_SEARCH_DESCRIPTION;
  }
  if (engine_keyword == TemplateURLPrepopulateData::nona.keyword) {
    return IDS_NONA_SEARCH_DESCRIPTION;
  }
  if (engine_keyword == TemplateURLPrepopulateData::oceanhero.keyword) {
    return IDS_OCEANHERO_SEARCH_DESCRIPTION;
  }
  if (engine_keyword == TemplateURLPrepopulateData::panda.keyword) {
    return IDS_PANDA_SEARCH_DESCRIPTION;
  }
  if (engine_keyword == TemplateURLPrepopulateData::privacywall.keyword) {
    return IDS_PRIVACYWALL_SEARCH_DESCRIPTION;
  }
  if (engine_keyword == TemplateURLPrepopulateData::quendu.keyword) {
    return IDS_QUENDU_SEARCH_DESCRIPTION;
  }
  if (engine_keyword == TemplateURLPrepopulateData::qwant.keyword) {
    return IDS_QWANT_SEARCH_DESCRIPTION;
  }
  if (engine_keyword == TemplateURLPrepopulateData::seznam_cz.keyword) {
    return IDS_SEZNAM_SEARCH_DESCRIPTION;
  }
  if (engine_keyword == TemplateURLPrepopulateData::seznam_sk.keyword) {
    return IDS_SEZNAM_SEARCH_DESCRIPTION;
  }
  if (engine_keyword == TemplateURLPrepopulateData::yahoo.keyword) {
    return IDS_YAHOO_SEARCH_DESCRIPTION;
  }
  if (engine_keyword == TemplateURLPrepopulateData::yahoo_ar.keyword) {
    return IDS_YAHOO_SEARCH_DESCRIPTION;
  }
  if (engine_keyword == TemplateURLPrepopulateData::yahoo_at.keyword) {
    return IDS_YAHOO_SEARCH_DESCRIPTION;
  }
  if (engine_keyword == TemplateURLPrepopulateData::yahoo_au.keyword) {
    return IDS_YAHOO_SEARCH_DESCRIPTION;
  }
  if (engine_keyword == TemplateURLPrepopulateData::yahoo_br.keyword) {
    return IDS_YAHOO_SEARCH_DESCRIPTION;
  }
  if (engine_keyword == TemplateURLPrepopulateData::yahoo_ca.keyword) {
    return IDS_YAHOO_SEARCH_DESCRIPTION;
  }
  if (engine_keyword == TemplateURLPrepopulateData::yahoo_ch.keyword) {
    return IDS_YAHOO_SEARCH_DESCRIPTION;
  }
  if (engine_keyword == TemplateURLPrepopulateData::yahoo_cl.keyword) {
    return IDS_YAHOO_SEARCH_DESCRIPTION;
  }
  if (engine_keyword == TemplateURLPrepopulateData::yahoo_co.keyword) {
    return IDS_YAHOO_SEARCH_DESCRIPTION;
  }
  if (engine_keyword == TemplateURLPrepopulateData::yahoo_de.keyword) {
    return IDS_YAHOO_SEARCH_DESCRIPTION;
  }
  if (engine_keyword == TemplateURLPrepopulateData::yahoo_dk.keyword) {
    return IDS_YAHOO_SEARCH_DESCRIPTION;
  }
  if (engine_keyword == TemplateURLPrepopulateData::yahoo_es.keyword) {
    return IDS_YAHOO_SEARCH_DESCRIPTION;
  }
  if (engine_keyword == TemplateURLPrepopulateData::yahoo_fi.keyword) {
    return IDS_YAHOO_SEARCH_DESCRIPTION;
  }
  if (engine_keyword == TemplateURLPrepopulateData::yahoo_fr.keyword) {
    return IDS_YAHOO_SEARCH_DESCRIPTION;
  }
  if (engine_keyword == TemplateURLPrepopulateData::yahoo_hk.keyword) {
    return IDS_YAHOO_SEARCH_DESCRIPTION;
  }
  if (engine_keyword == TemplateURLPrepopulateData::yahoo_id.keyword) {
    return IDS_YAHOO_SEARCH_DESCRIPTION;
  }
  if (engine_keyword == TemplateURLPrepopulateData::yahoo_in.keyword) {
    return IDS_YAHOO_SEARCH_DESCRIPTION;
  }
  if (engine_keyword == TemplateURLPrepopulateData::yahoo_it.keyword) {
    return IDS_YAHOO_SEARCH_DESCRIPTION;
  }
  if (engine_keyword == TemplateURLPrepopulateData::yahoo_jp.keyword) {
    return IDS_YAHOO_SEARCH_DESCRIPTION;
  }
  if (engine_keyword == TemplateURLPrepopulateData::yahoo_mx.keyword) {
    return IDS_YAHOO_SEARCH_DESCRIPTION;
  }
  if (engine_keyword == TemplateURLPrepopulateData::yahoo_my.keyword) {
    return IDS_YAHOO_SEARCH_DESCRIPTION;
  }
  if (engine_keyword == TemplateURLPrepopulateData::yahoo_nl.keyword) {
    return IDS_YAHOO_SEARCH_DESCRIPTION;
  }
  if (engine_keyword == TemplateURLPrepopulateData::yahoo_nz.keyword) {
    return IDS_YAHOO_SEARCH_DESCRIPTION;
  }
  if (engine_keyword == TemplateURLPrepopulateData::yahoo_pe.keyword) {
    return IDS_YAHOO_SEARCH_DESCRIPTION;
  }
  if (engine_keyword == TemplateURLPrepopulateData::yahoo_ph.keyword) {
    return IDS_YAHOO_SEARCH_DESCRIPTION;
  }
  if (engine_keyword == TemplateURLPrepopulateData::yahoo_se.keyword) {
    return IDS_YAHOO_SEARCH_DESCRIPTION;
  }
  if (engine_keyword == TemplateURLPrepopulateData::yahoo_sg.keyword) {
    return IDS_YAHOO_SEARCH_DESCRIPTION;
  }
  if (engine_keyword == TemplateURLPrepopulateData::yahoo_th.keyword) {
    return IDS_YAHOO_SEARCH_DESCRIPTION;
  }
  if (engine_keyword == TemplateURLPrepopulateData::yahoo_tr.keyword) {
    return IDS_YAHOO_SEARCH_DESCRIPTION;
  }
  if (engine_keyword == TemplateURLPrepopulateData::yahoo_tw.keyword) {
    return IDS_YAHOO_SEARCH_DESCRIPTION;
  }
  if (engine_keyword == TemplateURLPrepopulateData::yahoo_uk.keyword) {
    return IDS_YAHOO_SEARCH_DESCRIPTION;
  }
  if (engine_keyword == TemplateURLPrepopulateData::yandex_by.keyword) {
    return IDS_YANDEX_SEARCH_DESCRIPTION;
  }
  if (engine_keyword == TemplateURLPrepopulateData::yandex_com.keyword) {
    return IDS_YANDEX_SEARCH_DESCRIPTION;
  }
  if (engine_keyword == TemplateURLPrepopulateData::yandex_kz.keyword) {
    return IDS_YANDEX_SEARCH_DESCRIPTION;
  }
  if (engine_keyword == TemplateURLPrepopulateData::yandex_ru.keyword) {
    return IDS_YANDEX_SEARCH_DESCRIPTION;
  }
  if (engine_keyword == TemplateURLPrepopulateData::yandex_tr.keyword) {
    return IDS_YANDEX_SEARCH_DESCRIPTION;
  }
  if (engine_keyword == TemplateURLPrepopulateData::yep.keyword) {
    return IDS_YEP_SEARCH_DESCRIPTION;
  }
  if (engine_keyword == TemplateURLPrepopulateData::you.keyword) {
    return IDS_YOU_SEARCH_DESCRIPTION;
  }
  return -1;
}
}  // namespace search_engines
