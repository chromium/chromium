// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

// NOTE: You should probably not change the data in this file without changing
// |kCurrentDataVersion| in prepopulated_engines.json. See comments in
// GetDataVersion() below!

// Also see if the config at
// tools/search_engine_choice/generate_search_engine_icons_config.json needs to
// be updated, and then run
// tools/search_engine_choice/generate_search_engine_icons.py to refresh icons.

// Search engine tier per country.
// SearchEngineTier will be equal to kTopEngines for the top 5 engines,
// kTyingEngines for tying 5+th engines and kRemainingEngines for the
// remaining engines.
enum class SearchEngineTier {
  kTopEngines = 1,
  kTyingEngines,
  kRemainingEngines,
};

// `PrepopulateEngine` and tier per country.
struct EngineAndTier {
  SearchEngineTier tier;
  const raw_ptr<PrepopulatedEngine const> search_engine;
};

// Put the engines within each country sorted based on some internal logic.
// The default will be the first engine.
// TODO(b/326396215): Link to documentation on this once it is published.

// Default (for countries with no better engine set)
constexpr EngineAndTier engines_default[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo},
};

// Note, the below entries are sorted by country code, not the name in comment.
// Engine selection by country ------------------------------------------------
// clang-format off
// United Arab Emirates
constexpr EngineAndTier engines_AE[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo},
    {SearchEngineTier::kTopEngines, &yandex_ru},
    {SearchEngineTier::kTopEngines, &duckduckgo},
};

// Albania
constexpr EngineAndTier engines_AL[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo},
    {SearchEngineTier::kTopEngines, &yandex_tr},
    {SearchEngineTier::kTopEngines, &duckduckgo},
};

// Argentina
constexpr EngineAndTier engines_AR[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo_ar},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &yandex_com},
};

// Austria
constexpr EngineAndTier engines_AT[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &brave},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &ecosia},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kRemainingEngines, &yahoo_at},
    {SearchEngineTier::kRemainingEngines, &qwant},
    {SearchEngineTier::kRemainingEngines, &privacywall},
};

// Australia
constexpr EngineAndTier engines_AU[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo_au},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &ecosia},
};

// Bosnia and Herzegovina
constexpr EngineAndTier engines_BA[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo},
    {SearchEngineTier::kTopEngines, &yandex_com},
    {SearchEngineTier::kTopEngines, &duckduckgo},
};

// Belgium
constexpr EngineAndTier engines_BE[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &brave},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &ecosia},
    {SearchEngineTier::kRemainingEngines, &yahoo_emea},
    {SearchEngineTier::kRemainingEngines, &qwant},
    {SearchEngineTier::kRemainingEngines, &privacywall},
};
// Bulgaria
constexpr EngineAndTier engines_BG[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &brave},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo_emea},
    {SearchEngineTier::kRemainingEngines, &yep},
    {SearchEngineTier::kRemainingEngines, &ecosia},
    {SearchEngineTier::kRemainingEngines, &qwant},
};

// Bahrain
constexpr EngineAndTier engines_BH[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo},
    {SearchEngineTier::kTopEngines, &yandex_com},
    {SearchEngineTier::kTopEngines, &duckduckgo},
};

// Burundi
constexpr EngineAndTier engines_BI[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo},
    {SearchEngineTier::kTopEngines, &yandex_ru},
    {SearchEngineTier::kTopEngines, &duckduckgo},
};

// Brunei
constexpr EngineAndTier engines_BN[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &yandex_com},
};

// Bolivia
constexpr EngineAndTier engines_BO[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo},
    {SearchEngineTier::kTopEngines, &yandex_com},
    {SearchEngineTier::kTopEngines, &duckduckgo},
};

// Brazil
constexpr EngineAndTier engines_BR[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo_br},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &yandex_com},
};

// Belarus
constexpr EngineAndTier engines_BY[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &yandex_by},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &mail_ru},
};

// Belize
constexpr EngineAndTier engines_BZ[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &yahoo},
    {SearchEngineTier::kTopEngines, &yandex_com},
};

// Canada
constexpr EngineAndTier engines_CA[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo_ca},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &yandex_com},
};

// Switzerland
constexpr EngineAndTier engines_CH[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &yahoo_ch},
    {SearchEngineTier::kTopEngines, &ecosia},
};

// Chile
constexpr EngineAndTier engines_CL[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo_cl},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &yandex_com},
};

// China
constexpr EngineAndTier engines_CN[] = {
    {SearchEngineTier::kTopEngines, &baidu},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &sogou},
    {SearchEngineTier::kTopEngines, &so_360},
    {SearchEngineTier::kTopEngines, &google},
};

// Colombia
constexpr EngineAndTier engines_CO[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo_co},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &ecosia},
};

// Costa Rica
constexpr EngineAndTier engines_CR[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &yandex_com},
};

// Republic of Cyprus
constexpr EngineAndTier engines_CY[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &brave},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &yahoo_emea},
    {SearchEngineTier::kRemainingEngines, &ecosia},
    {SearchEngineTier::kRemainingEngines, &qwant},
    {SearchEngineTier::kRemainingEngines, &privacywall},
};

// Czech Republic
constexpr EngineAndTier engines_CZ[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &seznam},
    {SearchEngineTier::kTopEngines, &brave},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kRemainingEngines, &ecosia},
    {SearchEngineTier::kRemainingEngines, &yahoo_emea},
    {SearchEngineTier::kRemainingEngines, &privacywall},
};

// Germany
constexpr EngineAndTier engines_DE[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &ecosia},
    {SearchEngineTier::kTopEngines, &brave},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kRemainingEngines, &yahoo_de},
    {SearchEngineTier::kRemainingEngines, &qwant},
    {SearchEngineTier::kRemainingEngines, &privacywall},
};

// Denmark
constexpr EngineAndTier engines_DK[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &brave},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &ecosia},
    {SearchEngineTier::kRemainingEngines, &yahoo_dk},
    {SearchEngineTier::kRemainingEngines, &qwant},
    {SearchEngineTier::kRemainingEngines, &privacywall},
};


// Dominican Republic
constexpr EngineAndTier engines_DO[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &yandex_com},
};

// Algeria
constexpr EngineAndTier engines_DZ[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo_fr},
    {SearchEngineTier::kTopEngines, &yandex_com},
    {SearchEngineTier::kTopEngines, &duckduckgo},
};

// Ecuador
constexpr EngineAndTier engines_EC[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo_es},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &ecosia},
};

// Estonia
constexpr EngineAndTier engines_EE[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &brave},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &ecosia},
    {SearchEngineTier::kRemainingEngines, &yahoo_emea},
    {SearchEngineTier::kRemainingEngines, &qwant},
    {SearchEngineTier::kRemainingEngines, &privacywall},
};

// Egypt
constexpr EngineAndTier engines_EG[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo},
    {SearchEngineTier::kTopEngines, &yandex_com},
    {SearchEngineTier::kTopEngines, &duckduckgo},
};

// Spain
constexpr EngineAndTier engines_ES[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &brave},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &ecosia},
    {SearchEngineTier::kRemainingEngines, &yahoo_es},
    {SearchEngineTier::kRemainingEngines, &qwant},
    {SearchEngineTier::kRemainingEngines, &privacywall},
};

// Finland
constexpr EngineAndTier engines_FI[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &brave},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &ecosia},
    {SearchEngineTier::kRemainingEngines, &yahoo_fi},
    {SearchEngineTier::kRemainingEngines, &privacywall},
    {SearchEngineTier::kRemainingEngines, &qwant},
};

// Faroe Islands
constexpr EngineAndTier engines_FO[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo_uk},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &ecosia},
};

// France
constexpr EngineAndTier engines_FR[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &brave},
    {SearchEngineTier::kTopEngines, &ecosia},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &qwant},
    {SearchEngineTier::kRemainingEngines, &duckduckgo},
    {SearchEngineTier::kRemainingEngines, &yahoo_fr},
    {SearchEngineTier::kRemainingEngines, &lilo},
};

// United Kingdom
constexpr EngineAndTier engines_GB[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo_uk},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &ecosia},
};

// Greece
constexpr EngineAndTier engines_GR[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &brave},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo_emea},
    {SearchEngineTier::kRemainingEngines, &ecosia},
    {SearchEngineTier::kRemainingEngines, &yep},
    {SearchEngineTier::kRemainingEngines, &qwant},
};

// Guatemala
constexpr EngineAndTier engines_GT[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &yandex_com},
};

// Hong Kong
constexpr EngineAndTier engines_HK[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo_hk},
    {SearchEngineTier::kTopEngines, &baidu},
    {SearchEngineTier::kTopEngines, &so_360},
};

// Honduras
constexpr EngineAndTier engines_HN[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &yandex_com},
};

// Croatia
constexpr EngineAndTier engines_HR[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &brave},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &ecosia},
    {SearchEngineTier::kRemainingEngines, &yahoo_emea},
    {SearchEngineTier::kRemainingEngines, &yep},
    {SearchEngineTier::kRemainingEngines, &privacywall},
};

// Hungary
constexpr EngineAndTier engines_HU[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &brave},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &ecosia},
    {SearchEngineTier::kRemainingEngines, &yahoo_emea},
    {SearchEngineTier::kRemainingEngines, &yep},
    {SearchEngineTier::kRemainingEngines, &qwant},
};

// Indonesia
constexpr EngineAndTier engines_ID[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &yandex_com},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo_id},
    {SearchEngineTier::kTopEngines, &duckduckgo},
};

// Ireland
constexpr EngineAndTier engines_IE[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &brave},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo_uk},
    {SearchEngineTier::kRemainingEngines, &ecosia},
    {SearchEngineTier::kRemainingEngines, &privacywall},
    {SearchEngineTier::kRemainingEngines, &qwant},
};

// Israel
constexpr EngineAndTier engines_IL[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yandex_ru},
    {SearchEngineTier::kTopEngines, &yahoo},
    {SearchEngineTier::kTopEngines, &duckduckgo},
};

// India
constexpr EngineAndTier engines_IN[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo_in},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &yandex_com},
};

// Iraq
constexpr EngineAndTier engines_IQ[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yandex_tr},
    {SearchEngineTier::kTopEngines, &yahoo},
    {SearchEngineTier::kTopEngines, &duckduckgo},
};

// Iran
constexpr EngineAndTier engines_IR[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &yandex_com},
};

// Iceland
constexpr EngineAndTier engines_IS[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &brave},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &ecosia},
    {SearchEngineTier::kRemainingEngines, &yahoo_emea},
    {SearchEngineTier::kRemainingEngines, &privacywall},
    {SearchEngineTier::kRemainingEngines, &qwant},
};

// Italy
constexpr EngineAndTier engines_IT[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &brave},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &ecosia},
    {SearchEngineTier::kRemainingEngines, &yahoo_it},
    {SearchEngineTier::kRemainingEngines, &qwant},
    {SearchEngineTier::kRemainingEngines, &privacywall},
};

// Jamaica
constexpr EngineAndTier engines_JM[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &yandex_com},
};

// Jordan
constexpr EngineAndTier engines_JO[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo},
    {SearchEngineTier::kTopEngines, &yandex_com},
    {SearchEngineTier::kTopEngines, &duckduckgo},
};

// Japan
constexpr EngineAndTier engines_JP[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &yahoo_jp},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &coccoc},
};

// Kenya
constexpr EngineAndTier engines_KE[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &yandex_com},
};

// South Korea
constexpr EngineAndTier engines_KR[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &naver},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &daum},
    {SearchEngineTier::kTopEngines, &coccoc},
};

// Kuwait
constexpr EngineAndTier engines_KW[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yandex_tr},
    {SearchEngineTier::kTopEngines, &yahoo},
    {SearchEngineTier::kTopEngines, &duckduckgo},
};

// Kazakhstan
constexpr EngineAndTier engines_KZ[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &yandex_kz},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &mail_ru},
    {SearchEngineTier::kTopEngines, &yahoo},
};

// Lebanon
constexpr EngineAndTier engines_LB[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &yandex_com},
};

// Liechtenstein
constexpr EngineAndTier engines_LI[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &brave},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &ecosia},
    {SearchEngineTier::kRemainingEngines, &qwant},
    {SearchEngineTier::kRemainingEngines, &yahoo_emea},
    {SearchEngineTier::kRemainingEngines, &privacywall},
};

// Lithuania
constexpr EngineAndTier engines_LT[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &brave},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo_emea},
    {SearchEngineTier::kRemainingEngines, &ecosia},
    {SearchEngineTier::kRemainingEngines, &qwant},
    {SearchEngineTier::kRemainingEngines, &yep},
};

// Luxembourg
constexpr EngineAndTier engines_LU[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &brave},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &ecosia},
    {SearchEngineTier::kRemainingEngines, &qwant},
    {SearchEngineTier::kRemainingEngines, &yahoo_emea},
    {SearchEngineTier::kRemainingEngines, &privacywall},
};

// Latvia
constexpr EngineAndTier engines_LV[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &brave},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo_emea},
    {SearchEngineTier::kRemainingEngines, &ecosia},
    {SearchEngineTier::kRemainingEngines, &qwant},
    {SearchEngineTier::kRemainingEngines, &privacywall},
};
// Libya
constexpr EngineAndTier engines_LY[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yandex_tr},
    {SearchEngineTier::kTopEngines, &yahoo},
    {SearchEngineTier::kTopEngines, &duckduckgo},
};

// Morocco
constexpr EngineAndTier engines_MA[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo_fr},
    {SearchEngineTier::kTopEngines, &yandex_com},
    {SearchEngineTier::kTopEngines, &duckduckgo},
};

// Monaco
constexpr EngineAndTier engines_MC[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo_fr},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &yandex_com},
};

// Moldova
constexpr EngineAndTier engines_MD[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &yandex_ru},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &mail_ru},
};

// Montenegro
constexpr EngineAndTier engines_ME[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yandex_ru},
    {SearchEngineTier::kTopEngines, &yahoo},
    {SearchEngineTier::kTopEngines, &duckduckgo},
};

// Macedonia
constexpr EngineAndTier engines_MK[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo},
    {SearchEngineTier::kTopEngines, &yandex_com},
    {SearchEngineTier::kTopEngines, &duckduckgo},
};

// Malta
constexpr EngineAndTier engines_MT[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &brave},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &ecosia},
    {SearchEngineTier::kRemainingEngines, &yahoo_emea},
    {SearchEngineTier::kRemainingEngines, &qwant},
    {SearchEngineTier::kRemainingEngines, &privacywall},
};

// Mexico
constexpr EngineAndTier engines_MX[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo_mx},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &ecosia},
};

// Malaysia
constexpr EngineAndTier engines_MY[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo_my},
    {SearchEngineTier::kTopEngines, &yandex_com},
    {SearchEngineTier::kTopEngines, &duckduckgo},
};

// Nicaragua
constexpr EngineAndTier engines_NI[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &yandex_com},
};

// Netherlands
constexpr EngineAndTier engines_NL[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &brave},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &ecosia},
    {SearchEngineTier::kRemainingEngines, &yahoo_nl},
    {SearchEngineTier::kRemainingEngines, &privacywall},
    {SearchEngineTier::kRemainingEngines, &qwant},
};

// Norway
constexpr EngineAndTier engines_NO[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &brave},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo_emea},
    {SearchEngineTier::kRemainingEngines, &ecosia},
    {SearchEngineTier::kRemainingEngines, &privacywall},
    {SearchEngineTier::kRemainingEngines, &qwant},
};

// New Zealand
constexpr EngineAndTier engines_NZ[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo_nz},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &ecosia},
};

// Oman
constexpr EngineAndTier engines_OM[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &yandex_com},
};

// Panama
constexpr EngineAndTier engines_PA[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo_es},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &yandex_com},
};

// Peru
constexpr EngineAndTier engines_PE[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo_pe},
    {SearchEngineTier::kTopEngines, &yandex_com},
    {SearchEngineTier::kTopEngines, &duckduckgo},
};

// Philippines
constexpr EngineAndTier engines_PH[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo_ph},
    {SearchEngineTier::kTopEngines, &ecosia},
    {SearchEngineTier::kTopEngines, &duckduckgo},
};

// Pakistan
constexpr EngineAndTier engines_PK[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &yandex_com},
};

// Poland
constexpr EngineAndTier engines_PL[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &brave},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo_emea},
    {SearchEngineTier::kRemainingEngines, &ecosia},
    {SearchEngineTier::kRemainingEngines, &privacywall},
    {SearchEngineTier::kRemainingEngines, &qwant},
};

// Puerto Rico
constexpr EngineAndTier engines_PR[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &yandex_com},
};

// Portugal
constexpr EngineAndTier engines_PT[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &brave},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &ecosia},
    {SearchEngineTier::kRemainingEngines, &yahoo_emea},
    {SearchEngineTier::kRemainingEngines, &yep},
    {SearchEngineTier::kRemainingEngines, &qwant},
};

// Paraguay
constexpr EngineAndTier engines_PY[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo_es},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &yandex_com},
};

// Qatar
constexpr EngineAndTier engines_QA[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &yandex_tr},
};

// Romania
constexpr EngineAndTier engines_RO[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &brave},
    {SearchEngineTier::kTopEngines, &yahoo_emea},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kRemainingEngines, &ecosia},
    {SearchEngineTier::kRemainingEngines, &yep},
    {SearchEngineTier::kRemainingEngines, &qwant},
};

// Serbia
constexpr EngineAndTier engines_RS[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo},
    {SearchEngineTier::kTopEngines, &yandex_ru},
    {SearchEngineTier::kTopEngines, &duckduckgo},
};

// Russia
constexpr EngineAndTier engines_RU[] = {
    {SearchEngineTier::kTopEngines, &yandex_ru},
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &mail_ru},
};

// Rwanda
constexpr EngineAndTier engines_RW[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &yandex_com},
};

// Saudi Arabia
constexpr EngineAndTier engines_SA[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yandex_com},
    {SearchEngineTier::kTopEngines, &yahoo},
    {SearchEngineTier::kTopEngines, &duckduckgo},
};

// Sweden
constexpr EngineAndTier engines_SE[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &brave},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &ecosia},
    {SearchEngineTier::kRemainingEngines, &yahoo_se},
    {SearchEngineTier::kRemainingEngines, &privacywall},
    {SearchEngineTier::kRemainingEngines, &qwant},
};

// Singapore
constexpr EngineAndTier engines_SG[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo_sg},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &yandex_com},
};

// Slovenia
constexpr EngineAndTier engines_SI[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &brave},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &ecosia},
    {SearchEngineTier::kRemainingEngines, &yahoo_emea},
    {SearchEngineTier::kRemainingEngines, &yep},
    {SearchEngineTier::kRemainingEngines, &qwant},
};

// Slovakia
constexpr EngineAndTier engines_SK[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &brave},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &seznam},
    {SearchEngineTier::kRemainingEngines, &yahoo_emea},
    {SearchEngineTier::kRemainingEngines, &ecosia},
    {SearchEngineTier::kRemainingEngines, &privacywall},
};

// El Salvador
constexpr EngineAndTier engines_SV[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo_es},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &yandex_com},
};

// Syria
constexpr EngineAndTier engines_SY[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yandex_ru},
    {SearchEngineTier::kTopEngines, &yahoo},
    {SearchEngineTier::kTopEngines, &duckduckgo},
};

// Thailand
constexpr EngineAndTier engines_TH[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo_th},
    {SearchEngineTier::kTopEngines, &yandex_com},
    {SearchEngineTier::kTopEngines, &duckduckgo},
};

// Tunisia
constexpr EngineAndTier engines_TN[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo_fr},
    {SearchEngineTier::kTopEngines, &yandex_com},
    {SearchEngineTier::kTopEngines, &duckduckgo},
};

// Turkey
constexpr EngineAndTier engines_TR[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &yandex_tr},
    {SearchEngineTier::kTopEngines, &yahoo_tr},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &duckduckgo},
};

// Trinidad and Tobago
constexpr EngineAndTier engines_TT[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &yandex_com},
};

// Taiwan
constexpr EngineAndTier engines_TW[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &yahoo_tw},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &coccoc},
    {SearchEngineTier::kTopEngines, &baidu},
};

// Tanzania
constexpr EngineAndTier engines_TZ[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo},
    {SearchEngineTier::kTopEngines, &yandex_com},
};

// Ukraine
constexpr EngineAndTier engines_UA[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &yandex_ru},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &yahoo},
};

// United States
constexpr EngineAndTier engines_US[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &yandex_com},
};

// Uruguay
constexpr EngineAndTier engines_UY[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo_es},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &yandex_com},
};

// Venezuela
constexpr EngineAndTier engines_VE[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo_es},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &yandex_com},
};

// Vietnam
constexpr EngineAndTier engines_VN[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &coccoc},
    {SearchEngineTier::kTopEngines, &yahoo},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &duckduckgo},
};

// Yemen
constexpr EngineAndTier engines_YE[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &yandex_com},
};

// South Africa
constexpr EngineAndTier engines_ZA[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &yandex_com},
};

// Zimbabwe
constexpr EngineAndTier engines_ZW[] = {
    {SearchEngineTier::kTopEngines, &google},
    {SearchEngineTier::kTopEngines, &bing},
    {SearchEngineTier::kTopEngines, &yahoo},
    {SearchEngineTier::kTopEngines, &duckduckgo},
    {SearchEngineTier::kTopEngines, &sogou},
};

// clang-format on
// ----------------------------------------------------------------------------

const std::vector<EngineAndTier> GetPrepopulationSetFromCountryID(
    int country_id) {
  const EngineAndTier* engines;
  size_t num_engines;
  // If you add a new country make sure to update the unit test for coverage.
  switch (country_id) {
#define UNHANDLED_COUNTRY(code1, code2) \
  case country_codes::CountryCharsToCountryID((#code1)[0], (#code2)[0]):
#define END_UNHANDLED_COUNTRIES(code1, code2)      \
  engines = engines_##code1##code2;                \
  num_engines = std::size(engines_##code1##code2); \
  break;
#define DECLARE_COUNTRY(code1, code2) \
  UNHANDLED_COUNTRY(code1, code2)     \
  END_UNHANDLED_COUNTRIES(code1, code2)

    // Countries with their own, dedicated engine set.
    DECLARE_COUNTRY(A, E)  // United Arab Emirates
    DECLARE_COUNTRY(A, L)  // Albania
    DECLARE_COUNTRY(A, R)  // Argentina
    DECLARE_COUNTRY(A, T)  // Austria
    DECLARE_COUNTRY(A, U)  // Australia
    DECLARE_COUNTRY(B, A)  // Bosnia and Herzegovina
    DECLARE_COUNTRY(B, E)  // Belgium
    DECLARE_COUNTRY(B, G)  // Bulgaria
    DECLARE_COUNTRY(B, H)  // Bahrain
    DECLARE_COUNTRY(B, I)  // Burundi
    DECLARE_COUNTRY(B, N)  // Brunei
    DECLARE_COUNTRY(B, O)  // Bolivia
    DECLARE_COUNTRY(B, R)  // Brazil
    DECLARE_COUNTRY(B, Y)  // Belarus
    DECLARE_COUNTRY(B, Z)  // Belize
    DECLARE_COUNTRY(C, A)  // Canada
    DECLARE_COUNTRY(C, H)  // Switzerland
    DECLARE_COUNTRY(C, L)  // Chile
    DECLARE_COUNTRY(C, N)  // China
    DECLARE_COUNTRY(C, O)  // Colombia
    DECLARE_COUNTRY(C, R)  // Costa Rica
    DECLARE_COUNTRY(C, Y)  // Republic of Cyprus
    DECLARE_COUNTRY(C, Z)  // Czech Republic
    DECLARE_COUNTRY(D, E)  // Germany
    DECLARE_COUNTRY(D, K)  // Denmark
    DECLARE_COUNTRY(D, O)  // Dominican Republic
    DECLARE_COUNTRY(D, Z)  // Algeria
    DECLARE_COUNTRY(E, C)  // Ecuador
    DECLARE_COUNTRY(E, E)  // Estonia
    DECLARE_COUNTRY(E, G)  // Egypt
    DECLARE_COUNTRY(E, S)  // Spain
    DECLARE_COUNTRY(F, I)  // Finland
    DECLARE_COUNTRY(F, O)  // Faroe Islands
    DECLARE_COUNTRY(F, R)  // France
    DECLARE_COUNTRY(G, B)  // United Kingdom
    DECLARE_COUNTRY(G, R)  // Greece
    DECLARE_COUNTRY(G, T)  // Guatemala
    DECLARE_COUNTRY(H, K)  // Hong Kong
    DECLARE_COUNTRY(H, N)  // Honduras
    DECLARE_COUNTRY(H, R)  // Croatia
    DECLARE_COUNTRY(H, U)  // Hungary
    DECLARE_COUNTRY(I, D)  // Indonesia
    DECLARE_COUNTRY(I, E)  // Ireland
    DECLARE_COUNTRY(I, L)  // Israel
    DECLARE_COUNTRY(I, N)  // India
    DECLARE_COUNTRY(I, Q)  // Iraq
    DECLARE_COUNTRY(I, R)  // Iran
    DECLARE_COUNTRY(I, S)  // Iceland
    DECLARE_COUNTRY(I, T)  // Italy
    DECLARE_COUNTRY(J, M)  // Jamaica
    DECLARE_COUNTRY(J, O)  // Jordan
    DECLARE_COUNTRY(J, P)  // Japan
    DECLARE_COUNTRY(K, E)  // Kenya
    DECLARE_COUNTRY(K, R)  // South Korea
    DECLARE_COUNTRY(K, W)  // Kuwait
    DECLARE_COUNTRY(K, Z)  // Kazakhstan
    DECLARE_COUNTRY(L, B)  // Lebanon
    DECLARE_COUNTRY(L, I)  // Liechtenstein
    DECLARE_COUNTRY(L, T)  // Lithuania
    DECLARE_COUNTRY(L, U)  // Luxembourg
    DECLARE_COUNTRY(L, V)  // Latvia
    DECLARE_COUNTRY(L, Y)  // Libya
    DECLARE_COUNTRY(M, A)  // Morocco
    DECLARE_COUNTRY(M, C)  // Monaco
    DECLARE_COUNTRY(M, D)  // Moldova
    DECLARE_COUNTRY(M, E)  // Montenegro
    DECLARE_COUNTRY(M, K)  // Macedonia
    DECLARE_COUNTRY(M, T)  // Malta
    DECLARE_COUNTRY(M, X)  // Mexico
    DECLARE_COUNTRY(M, Y)  // Malaysia
    DECLARE_COUNTRY(N, I)  // Nicaragua
    DECLARE_COUNTRY(N, L)  // Netherlands
    DECLARE_COUNTRY(N, O)  // Norway
    DECLARE_COUNTRY(N, Z)  // New Zealand
    DECLARE_COUNTRY(O, M)  // Oman
    DECLARE_COUNTRY(P, A)  // Panama
    DECLARE_COUNTRY(P, E)  // Peru
    DECLARE_COUNTRY(P, H)  // Philippines
    DECLARE_COUNTRY(P, K)  // Pakistan
    DECLARE_COUNTRY(P, L)  // Poland
    DECLARE_COUNTRY(P, R)  // Puerto Rico
    DECLARE_COUNTRY(P, T)  // Portugal
    DECLARE_COUNTRY(P, Y)  // Paraguay
    DECLARE_COUNTRY(Q, A)  // Qatar
    DECLARE_COUNTRY(R, O)  // Romania
    DECLARE_COUNTRY(R, S)  // Serbia
    DECLARE_COUNTRY(R, U)  // Russia
    DECLARE_COUNTRY(R, W)  // Rwanda
    DECLARE_COUNTRY(S, A)  // Saudi Arabia
    DECLARE_COUNTRY(S, E)  // Sweden
    DECLARE_COUNTRY(S, G)  // Singapore
    DECLARE_COUNTRY(S, I)  // Slovenia
    DECLARE_COUNTRY(S, K)  // Slovakia
    DECLARE_COUNTRY(S, V)  // El Salvador
    DECLARE_COUNTRY(S, Y)  // Syria
    DECLARE_COUNTRY(T, H)  // Thailand
    DECLARE_COUNTRY(T, N)  // Tunisia
    DECLARE_COUNTRY(T, R)  // Turkey
    DECLARE_COUNTRY(T, T)  // Trinidad and Tobago
    DECLARE_COUNTRY(T, W)  // Taiwan
    DECLARE_COUNTRY(T, Z)  // Tanzania
    DECLARE_COUNTRY(U, A)  // Ukraine
    DECLARE_COUNTRY(U, S)  // United States
    DECLARE_COUNTRY(U, Y)  // Uruguay
    DECLARE_COUNTRY(V, E)  // Venezuela
    DECLARE_COUNTRY(V, N)  // Vietnam
    DECLARE_COUNTRY(Y, E)  // Yemen
    DECLARE_COUNTRY(Z, A)  // South Africa
    DECLARE_COUNTRY(Z, W)  // Zimbabwe

    // Countries using the "Australia" engine set.
    UNHANDLED_COUNTRY(C, C)  // Cocos Islands
    UNHANDLED_COUNTRY(C, X)  // Christmas Island
    UNHANDLED_COUNTRY(H, M)  // Heard Island and McDonald Islands
    UNHANDLED_COUNTRY(N, F)  // Norfolk Island
    END_UNHANDLED_COUNTRIES(A, U)

    // Countries using the "China" engine set.
    UNHANDLED_COUNTRY(M, O)  // Macao
    END_UNHANDLED_COUNTRIES(C, N)

    // Countries using the "Denmark" engine set.
    UNHANDLED_COUNTRY(G, L)  // Greenland
    END_UNHANDLED_COUNTRIES(D, K)

    // Countries using the "Spain" engine set.
    UNHANDLED_COUNTRY(A, D)  // Andorra
    UNHANDLED_COUNTRY(E, A)  // Ceuta & Melilla (not in ISO 3166-1 but included
                             // in some Chrome country code lists)
    UNHANDLED_COUNTRY(I, C)  // Canary Islands (not in ISO 3166-1 but included
                             // in some Chrome country code lists)
    END_UNHANDLED_COUNTRIES(E, S)

    // Countries using the "Finland" engine set.
    UNHANDLED_COUNTRY(A, X)  // Aland Islands
    END_UNHANDLED_COUNTRIES(F, I)

    // Countries using the "France" engine set.
    UNHANDLED_COUNTRY(B, F)  // Burkina Faso
    UNHANDLED_COUNTRY(B, J)  // Benin
    UNHANDLED_COUNTRY(B, L)  // St. Barthélemy
    UNHANDLED_COUNTRY(C, D)  // Congo - Kinshasa
    UNHANDLED_COUNTRY(C, F)  // Central African Republic
    UNHANDLED_COUNTRY(C, G)  // Congo - Brazzaville
    UNHANDLED_COUNTRY(C, I)  // Ivory Coast
    UNHANDLED_COUNTRY(C, M)  // Cameroon
    UNHANDLED_COUNTRY(D, J)  // Djibouti
    UNHANDLED_COUNTRY(G, A)  // Gabon
    UNHANDLED_COUNTRY(G, F)  // French Guiana
    UNHANDLED_COUNTRY(G, N)  // Guinea
    UNHANDLED_COUNTRY(G, P)  // Guadeloupe
    UNHANDLED_COUNTRY(H, T)  // Haiti
#if BUILDFLAG(IS_WIN)
    UNHANDLED_COUNTRY(I, P)  // Clipperton Island ('IP' is an WinXP-ism; ISO
                             //                    includes it with France)
#endif
    UNHANDLED_COUNTRY(M, F)  // Saint Martin
    UNHANDLED_COUNTRY(M, L)  // Mali
    UNHANDLED_COUNTRY(M, Q)  // Martinique
    UNHANDLED_COUNTRY(N, C)  // New Caledonia
    UNHANDLED_COUNTRY(N, E)  // Niger
    UNHANDLED_COUNTRY(P, F)  // French Polynesia
    UNHANDLED_COUNTRY(P, M)  // Saint Pierre and Miquelon
    UNHANDLED_COUNTRY(R, E)  // Reunion
    UNHANDLED_COUNTRY(S, N)  // Senegal
    UNHANDLED_COUNTRY(T, D)  // Chad
    UNHANDLED_COUNTRY(T, F)  // French Southern Territories
    UNHANDLED_COUNTRY(T, G)  // Togo
    UNHANDLED_COUNTRY(W, F)  // Wallis and Futuna
    UNHANDLED_COUNTRY(Y, T)  // Mayotte
    END_UNHANDLED_COUNTRIES(F, R)

    // Countries using the "Italy" engine set.
    UNHANDLED_COUNTRY(S, M)  // San Marino
    UNHANDLED_COUNTRY(V, A)  // Vatican
    END_UNHANDLED_COUNTRIES(I, T)

    // Countries using the "Morocco" engine set.
    UNHANDLED_COUNTRY(E, H)  // Western Sahara
    END_UNHANDLED_COUNTRIES(M, A)

    // Countries using the "Netherlands" engine set.
    UNHANDLED_COUNTRY(A, N)  // Netherlands Antilles
    UNHANDLED_COUNTRY(A, W)  // Aruba
    END_UNHANDLED_COUNTRIES(N, L)

    // Countries using the "Norway" engine set.
    UNHANDLED_COUNTRY(B, V)  // Bouvet Island
    UNHANDLED_COUNTRY(S, J)  // Svalbard and Jan Mayen
    END_UNHANDLED_COUNTRIES(N, O)

    // Countries using the "New Zealand" engine set.
    UNHANDLED_COUNTRY(C, K)  // Cook Islands
    UNHANDLED_COUNTRY(N, U)  // Niue
    UNHANDLED_COUNTRY(T, K)  // Tokelau
    END_UNHANDLED_COUNTRIES(N, Z)

    // Countries using the "Portugal" engine set.
    UNHANDLED_COUNTRY(C, V)  // Cape Verde
    UNHANDLED_COUNTRY(G, W)  // Guinea-Bissau
    UNHANDLED_COUNTRY(M, Z)  // Mozambique
    UNHANDLED_COUNTRY(S, T)  // Sao Tome and Principe
    UNHANDLED_COUNTRY(T, L)  // Timor-Leste
    END_UNHANDLED_COUNTRIES(P, T)

    // Countries using the "Russia" engine set.
    UNHANDLED_COUNTRY(A, M)  // Armenia
    UNHANDLED_COUNTRY(A, Z)  // Azerbaijan
    UNHANDLED_COUNTRY(K, G)  // Kyrgyzstan
    UNHANDLED_COUNTRY(T, J)  // Tajikistan
    UNHANDLED_COUNTRY(T, M)  // Turkmenistan
    UNHANDLED_COUNTRY(U, Z)  // Uzbekistan
    END_UNHANDLED_COUNTRIES(R, U)

    // Countries using the "Saudi Arabia" engine set.
    UNHANDLED_COUNTRY(M, R)  // Mauritania
    UNHANDLED_COUNTRY(P, S)  // Palestinian Territory
    UNHANDLED_COUNTRY(S, D)  // Sudan
    END_UNHANDLED_COUNTRIES(S, A)

    // Countries using the "United Kingdom" engine set.
    UNHANDLED_COUNTRY(B, M)  // Bermuda
    UNHANDLED_COUNTRY(C, Q)  // Sark
    UNHANDLED_COUNTRY(F, K)  // Falkland Islands
    UNHANDLED_COUNTRY(G, G)  // Guernsey
    UNHANDLED_COUNTRY(G, I)  // Gibraltar
    UNHANDLED_COUNTRY(G, S)  // South Georgia and the South Sandwich
                             //   Islands
    UNHANDLED_COUNTRY(I, M)  // Isle of Man
    UNHANDLED_COUNTRY(I, O)  // British Indian Ocean Territory
    UNHANDLED_COUNTRY(J, E)  // Jersey
    UNHANDLED_COUNTRY(K, Y)  // Cayman Islands
    UNHANDLED_COUNTRY(M, S)  // Montserrat
    UNHANDLED_COUNTRY(P, N)  // Pitcairn Islands
    UNHANDLED_COUNTRY(S, H)  // Saint Helena, Ascension Island, and Tristan da
                             //   Cunha
    UNHANDLED_COUNTRY(T, C)  // Turks and Caicos Islands
    UNHANDLED_COUNTRY(V, G)  // British Virgin Islands
    END_UNHANDLED_COUNTRIES(G, B)

    // Countries using the "United States" engine set.
    UNHANDLED_COUNTRY(A, S)  // American Samoa
    UNHANDLED_COUNTRY(G, U)  // Guam
    UNHANDLED_COUNTRY(M, P)  // Northern Mariana Islands
    UNHANDLED_COUNTRY(U, M)  // U.S. Minor Outlying Islands
    UNHANDLED_COUNTRY(V, I)  // U.S. Virgin Islands
    END_UNHANDLED_COUNTRIES(U, S)

    // Countries using the "default" engine set.
    UNHANDLED_COUNTRY(A, F)  // Afghanistan
    UNHANDLED_COUNTRY(A, G)  // Antigua and Barbuda
    UNHANDLED_COUNTRY(A, I)  // Anguilla
    UNHANDLED_COUNTRY(A, O)  // Angola
    UNHANDLED_COUNTRY(A, Q)  // Antarctica
    UNHANDLED_COUNTRY(B, B)  // Barbados
    UNHANDLED_COUNTRY(B, D)  // Bangladesh
    UNHANDLED_COUNTRY(B, S)  // Bahamas
    UNHANDLED_COUNTRY(B, T)  // Bhutan
    UNHANDLED_COUNTRY(B, W)  // Botswana
    UNHANDLED_COUNTRY(C, U)  // Cuba
    UNHANDLED_COUNTRY(D, M)  // Dominica
    UNHANDLED_COUNTRY(E, R)  // Eritrea
    UNHANDLED_COUNTRY(E, T)  // Ethiopia
    UNHANDLED_COUNTRY(F, J)  // Fiji
    UNHANDLED_COUNTRY(F, M)  // Micronesia
    UNHANDLED_COUNTRY(G, D)  // Grenada
    UNHANDLED_COUNTRY(G, E)  // Georgia
    UNHANDLED_COUNTRY(G, H)  // Ghana
    UNHANDLED_COUNTRY(G, M)  // Gambia
    UNHANDLED_COUNTRY(G, Q)  // Equatorial Guinea
    UNHANDLED_COUNTRY(G, Y)  // Guyana
    UNHANDLED_COUNTRY(K, H)  // Cambodia
    UNHANDLED_COUNTRY(K, I)  // Kiribati
    UNHANDLED_COUNTRY(K, M)  // Comoros
    UNHANDLED_COUNTRY(K, N)  // Saint Kitts and Nevis
    UNHANDLED_COUNTRY(K, P)  // North Korea
    UNHANDLED_COUNTRY(L, A)  // Laos
    UNHANDLED_COUNTRY(L, C)  // Saint Lucia
    UNHANDLED_COUNTRY(L, K)  // Sri Lanka
    UNHANDLED_COUNTRY(L, R)  // Liberia
    UNHANDLED_COUNTRY(L, S)  // Lesotho
    UNHANDLED_COUNTRY(M, G)  // Madagascar
    UNHANDLED_COUNTRY(M, H)  // Marshall Islands
    UNHANDLED_COUNTRY(M, M)  // Myanmar
    UNHANDLED_COUNTRY(M, N)  // Mongolia
    UNHANDLED_COUNTRY(M, U)  // Mauritius
    UNHANDLED_COUNTRY(M, V)  // Maldives
    UNHANDLED_COUNTRY(M, W)  // Malawi
    UNHANDLED_COUNTRY(N, A)  // Namibia
    UNHANDLED_COUNTRY(N, G)  // Nigeria
    UNHANDLED_COUNTRY(N, P)  // Nepal
    UNHANDLED_COUNTRY(N, R)  // Nauru
    UNHANDLED_COUNTRY(P, G)  // Papua New Guinea
    UNHANDLED_COUNTRY(P, W)  // Palau
    UNHANDLED_COUNTRY(S, B)  // Solomon Islands
    UNHANDLED_COUNTRY(S, C)  // Seychelles
    UNHANDLED_COUNTRY(S, L)  // Sierra Leone
    UNHANDLED_COUNTRY(S, O)  // Somalia
    UNHANDLED_COUNTRY(S, R)  // Suriname
    UNHANDLED_COUNTRY(S, Z)  // Swaziland
    UNHANDLED_COUNTRY(T, O)  // Tonga
    UNHANDLED_COUNTRY(T, V)  // Tuvalu
    UNHANDLED_COUNTRY(U, G)  // Uganda
    UNHANDLED_COUNTRY(V, C)  // Saint Vincent and the Grenadines
    UNHANDLED_COUNTRY(V, U)  // Vanuatu
    UNHANDLED_COUNTRY(W, S)  // Samoa
    UNHANDLED_COUNTRY(Z, M)  // Zambia
    case country_codes::kCountryIDUnknown:
    default:  // Unhandled location
      END_UNHANDLED_COUNTRIES(def, ault)
  }

  std::vector<EngineAndTier> t_url;
  for (size_t i = 0; i < num_engines; i++) {
    t_url.push_back(engines[i]);
  }
  return t_url;
}
