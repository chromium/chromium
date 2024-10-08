// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/prepopulated_engines.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_utils.h"
#include "components/strings/grit/search_engine_descriptions_strings.h"

// This file is generated using
// http://go/chrome-search-engines-marketing-snippets-script.
// Do not modify it manually.
namespace search_engines {
// Implements a function declared in search_engine_choice_utils.h.
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
  if (engine_keyword == TemplateURLPrepopulateData::lilo.keyword) {
    return IDS_LILO_SEARCH_DESCRIPTION;
  }
  if (engine_keyword == TemplateURLPrepopulateData::privacywall.keyword) {
    return IDS_PRIVACYWALL_SEARCH_DESCRIPTION;
  }
  if (engine_keyword == TemplateURLPrepopulateData::qwant.keyword) {
    return IDS_QWANT_SEARCH_DESCRIPTION;
  }
  if (engine_keyword == TemplateURLPrepopulateData::seznam.keyword) {
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
  if (engine_keyword == TemplateURLPrepopulateData::yahoo_emea.keyword) {
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
  if (engine_keyword == TemplateURLPrepopulateData::yep.keyword) {
    return IDS_YEP_SEARCH_DESCRIPTION;
  }
  return -1;
}
}  // namespace search_engines
