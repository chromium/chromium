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
  {SearchEngineTier::kTopEngines, &yep},
  {SearchEngineTier::kRemainingEngines, &yahoo_emea},
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
  {SearchEngineTier::kTopEngines, &duckduckgo},
  {SearchEngineTier::kTopEngines, &bing},
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
  {SearchEngineTier::kRemainingEngines, &startpage},
};

// Denmark
constexpr EngineAndTier engines_DK[] = {
  {SearchEngineTier::kTopEngines, &google},
  {SearchEngineTier::kTopEngines, &brave},
  {SearchEngineTier::kTopEngines, &duckduckgo},
  {SearchEngineTier::kTopEngines, &bing},
  {SearchEngineTier::kTopEngines, &ecosia},
  {SearchEngineTier::kRemainingEngines, &yahoo_dk},
  {SearchEngineTier::kRemainingEngines, &privacywall},
  {SearchEngineTier::kRemainingEngines, &qwant},
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
  {SearchEngineTier::kRemainingEngines, &lilo},
  {SearchEngineTier::kRemainingEngines, &yahoo_fr},
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
  {SearchEngineTier::kRemainingEngines, &privacywall},
  {SearchEngineTier::kRemainingEngines, &yahoo_emea},
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
  {SearchEngineTier::kRemainingEngines, &privacywall},
  {SearchEngineTier::kRemainingEngines, &qwant},
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
  {SearchEngineTier::kTopEngines, &brave},
  {SearchEngineTier::kTopEngines, &bing},
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
  {SearchEngineTier::kTopEngines, &ecosia},
  {SearchEngineTier::kRemainingEngines, &yahoo_emea},
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
  {SearchEngineTier::kRemainingEngines, &yep},
  {SearchEngineTier::kRemainingEngines, &yahoo_emea},
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
  {SearchEngineTier::kRemainingEngines, &yep},
  {SearchEngineTier::kRemainingEngines, &ecosia},
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
    country_codes::CountryId country_id) {
  const EngineAndTier* engines;
  size_t num_engines;
  // If you add a new country make sure to update the unit test for coverage.
  switch (country_id.Serialize()) {
#define UNHANDLED_COUNTRY(code) \
  case country_codes::CountryId(#code).Serialize():
#define END_UNHANDLED_COUNTRIES(code)      \
  engines = engines_##code;                \
  num_engines = std::size(engines_##code); \
  break;
#define DECLARE_COUNTRY(code) \
  UNHANDLED_COUNTRY(code)     \
  END_UNHANDLED_COUNTRIES(code)

    // Countries with their own, dedicated engine set.
    DECLARE_COUNTRY(AE)  // United Arab Emirates
    DECLARE_COUNTRY(AL)  // Albania
    DECLARE_COUNTRY(AR)  // Argentina
    DECLARE_COUNTRY(AT)  // Austria
    DECLARE_COUNTRY(AU)  // Australia
    DECLARE_COUNTRY(BA)  // Bosnia and Herzegovina
    DECLARE_COUNTRY(BE)  // Belgium
    DECLARE_COUNTRY(BG)  // Bulgaria
    DECLARE_COUNTRY(BH)  // Bahrain
    DECLARE_COUNTRY(BI)  // Burundi
    DECLARE_COUNTRY(BN)  // Brunei
    DECLARE_COUNTRY(BO)  // Bolivia
    DECLARE_COUNTRY(BR)  // Brazil
    DECLARE_COUNTRY(BY)  // Belarus
    DECLARE_COUNTRY(BZ)  // Belize
    DECLARE_COUNTRY(CA)  // Canada
    DECLARE_COUNTRY(CH)  // Switzerland
    DECLARE_COUNTRY(CL)  // Chile
    DECLARE_COUNTRY(CN)  // China
    DECLARE_COUNTRY(CO)  // Colombia
    DECLARE_COUNTRY(CR)  // Costa Rica
    DECLARE_COUNTRY(CY)  // Republic of Cyprus
    DECLARE_COUNTRY(CZ)  // Czech Republic
    DECLARE_COUNTRY(DE)  // Germany
    DECLARE_COUNTRY(DK)  // Denmark
    DECLARE_COUNTRY(DO)  // Dominican Republic
    DECLARE_COUNTRY(DZ)  // Algeria
    DECLARE_COUNTRY(EC)  // Ecuador
    DECLARE_COUNTRY(EE)  // Estonia
    DECLARE_COUNTRY(EG)  // Egypt
    DECLARE_COUNTRY(ES)  // Spain
    DECLARE_COUNTRY(FI)  // Finland
    DECLARE_COUNTRY(FO)  // Faroe Islands
    DECLARE_COUNTRY(FR)  // France
    DECLARE_COUNTRY(GB)  // United Kingdom
    DECLARE_COUNTRY(GR)  // Greece
    DECLARE_COUNTRY(GT)  // Guatemala
    DECLARE_COUNTRY(HK)  // Hong Kong
    DECLARE_COUNTRY(HN)  // Honduras
    DECLARE_COUNTRY(HR)  // Croatia
    DECLARE_COUNTRY(HU)  // Hungary
    DECLARE_COUNTRY(ID)  // Indonesia
    DECLARE_COUNTRY(IE)  // Ireland
    DECLARE_COUNTRY(IL)  // Israel
    DECLARE_COUNTRY(IN)  // India
    DECLARE_COUNTRY(IQ)  // Iraq
    DECLARE_COUNTRY(IR)  // Iran
    DECLARE_COUNTRY(IS)  // Iceland
    DECLARE_COUNTRY(IT)  // Italy
    DECLARE_COUNTRY(JM)  // Jamaica
    DECLARE_COUNTRY(JO)  // Jordan
    DECLARE_COUNTRY(JP)  // Japan
    DECLARE_COUNTRY(KE)  // Kenya
    DECLARE_COUNTRY(KR)  // South Korea
    DECLARE_COUNTRY(KW)  // Kuwait
    DECLARE_COUNTRY(KZ)  // Kazakhstan
    DECLARE_COUNTRY(LB)  // Lebanon
    DECLARE_COUNTRY(LI)  // Liechtenstein
    DECLARE_COUNTRY(LT)  // Lithuania
    DECLARE_COUNTRY(LU)  // Luxembourg
    DECLARE_COUNTRY(LV)  // Latvia
    DECLARE_COUNTRY(LY)  // Libya
    DECLARE_COUNTRY(MA)  // Morocco
    DECLARE_COUNTRY(MC)  // Monaco
    DECLARE_COUNTRY(MD)  // Moldova
    DECLARE_COUNTRY(ME)  // Montenegro
    DECLARE_COUNTRY(MK)  // Macedonia
    DECLARE_COUNTRY(MT)  // Malta
    DECLARE_COUNTRY(MX)  // Mexico
    DECLARE_COUNTRY(MY)  // Malaysia
    DECLARE_COUNTRY(NI)  // Nicaragua
    DECLARE_COUNTRY(NL)  // Netherlands
    DECLARE_COUNTRY(NO)  // Norway
    DECLARE_COUNTRY(NZ)  // New Zealand
    DECLARE_COUNTRY(OM)  // Oman
    DECLARE_COUNTRY(PA)  // Panama
    DECLARE_COUNTRY(PE)  // Peru
    DECLARE_COUNTRY(PH)  // Philippines
    DECLARE_COUNTRY(PK)  // Pakistan
    DECLARE_COUNTRY(PL)  // Poland
    DECLARE_COUNTRY(PR)  // Puerto Rico
    DECLARE_COUNTRY(PT)  // Portugal
    DECLARE_COUNTRY(PY)  // Paraguay
    DECLARE_COUNTRY(QA)  // Qatar
    DECLARE_COUNTRY(RO)  // Romania
    DECLARE_COUNTRY(RS)  // Serbia
    DECLARE_COUNTRY(RU)  // Russia
    DECLARE_COUNTRY(RW)  // Rwanda
    DECLARE_COUNTRY(SA)  // Saudi Arabia
    DECLARE_COUNTRY(SE)  // Sweden
    DECLARE_COUNTRY(SG)  // Singapore
    DECLARE_COUNTRY(SI)  // Slovenia
    DECLARE_COUNTRY(SK)  // Slovakia
    DECLARE_COUNTRY(SV)  // El Salvador
    DECLARE_COUNTRY(SY)  // Syria
    DECLARE_COUNTRY(TH)  // Thailand
    DECLARE_COUNTRY(TN)  // Tunisia
    DECLARE_COUNTRY(TR)  // Turkey
    DECLARE_COUNTRY(TT)  // Trinidad and Tobago
    DECLARE_COUNTRY(TW)  // Taiwan
    DECLARE_COUNTRY(TZ)  // Tanzania
    DECLARE_COUNTRY(UA)  // Ukraine
    DECLARE_COUNTRY(US)  // United States
    DECLARE_COUNTRY(UY)  // Uruguay
    DECLARE_COUNTRY(VE)  // Venezuela
    DECLARE_COUNTRY(VN)  // Vietnam
    DECLARE_COUNTRY(YE)  // Yemen
    DECLARE_COUNTRY(ZA)  // South Africa
    DECLARE_COUNTRY(ZW)  // Zimbabwe

    // Countries using the "Australia" engine set.
    UNHANDLED_COUNTRY(CC)  // Cocos Islands
    UNHANDLED_COUNTRY(CX)  // Christmas Island
    UNHANDLED_COUNTRY(HM)  // Heard Island and McDonald Islands
    UNHANDLED_COUNTRY(NF)  // Norfolk Island
    END_UNHANDLED_COUNTRIES(AU)

    // Countries using the "China" engine set.
    UNHANDLED_COUNTRY(MO)  // Macao
    END_UNHANDLED_COUNTRIES(CN)

    // Countries using the "Denmark" engine set.
    UNHANDLED_COUNTRY(GL)  // Greenland
    END_UNHANDLED_COUNTRIES(DK)

    // Countries using the "Spain" engine set.
    UNHANDLED_COUNTRY(AD)  // Andorra
    UNHANDLED_COUNTRY(EA)  // Ceuta & Melilla (not in ISO 3166-1 but included
                           // in some Chrome country code lists)
    UNHANDLED_COUNTRY(IC)  // Canary Islands (not in ISO 3166-1 but included
                           // in some Chrome country code lists)
    END_UNHANDLED_COUNTRIES(ES)

    // Countries using the "Finland" engine set.
    UNHANDLED_COUNTRY(AX)  // Aland Islands
    END_UNHANDLED_COUNTRIES(FI)

    // Countries using the "France" engine set.
    UNHANDLED_COUNTRY(BF)  // Burkina Faso
    UNHANDLED_COUNTRY(BJ)  // Benin
    UNHANDLED_COUNTRY(BL)  // St. Barthélemy
    UNHANDLED_COUNTRY(CD)  // Congo - Kinshasa
    UNHANDLED_COUNTRY(CF)  // Central African Republic
    UNHANDLED_COUNTRY(CG)  // Congo - Brazzaville
    UNHANDLED_COUNTRY(CI)  // Ivory Coast
    UNHANDLED_COUNTRY(CM)  // Cameroon
    UNHANDLED_COUNTRY(DJ)  // Djibouti
    UNHANDLED_COUNTRY(GA)  // Gabon
    UNHANDLED_COUNTRY(GF)  // French Guiana
    UNHANDLED_COUNTRY(GN)  // Guinea
    UNHANDLED_COUNTRY(GP)  // Guadeloupe
    UNHANDLED_COUNTRY(HT)  // Haiti
    UNHANDLED_COUNTRY(MF)  // Saint Martin
    UNHANDLED_COUNTRY(ML)  // Mali
    UNHANDLED_COUNTRY(MQ)  // Martinique
    UNHANDLED_COUNTRY(NC)  // New Caledonia
    UNHANDLED_COUNTRY(NE)  // Niger
    UNHANDLED_COUNTRY(PF)  // French Polynesia
    UNHANDLED_COUNTRY(PM)  // Saint Pierre and Miquelon
    UNHANDLED_COUNTRY(RE)  // Reunion
    UNHANDLED_COUNTRY(SN)  // Senegal
    UNHANDLED_COUNTRY(TD)  // Chad
    UNHANDLED_COUNTRY(TF)  // French Southern Territories
    UNHANDLED_COUNTRY(TG)  // Togo
    UNHANDLED_COUNTRY(WF)  // Wallis and Futuna
    UNHANDLED_COUNTRY(YT)  // Mayotte
    END_UNHANDLED_COUNTRIES(FR)

    // Countries using the "Italy" engine set.
    UNHANDLED_COUNTRY(SM)  // San Marino
    UNHANDLED_COUNTRY(VA)  // Vatican
    END_UNHANDLED_COUNTRIES(IT)

    // Countries using the "Morocco" engine set.
    UNHANDLED_COUNTRY(EH)  // Western Sahara
    END_UNHANDLED_COUNTRIES(MA)

    // Countries using the "Netherlands" engine set.
    UNHANDLED_COUNTRY(AN)  // Netherlands Antilles
    UNHANDLED_COUNTRY(AW)  // Aruba
    END_UNHANDLED_COUNTRIES(NL)

    // Countries using the "Norway" engine set.
    UNHANDLED_COUNTRY(BV)  // Bouvet Island
    UNHANDLED_COUNTRY(SJ)  // Svalbard and Jan Mayen
    END_UNHANDLED_COUNTRIES(NO)

    // Countries using the "New Zealand" engine set.
    UNHANDLED_COUNTRY(CK)  // Cook Islands
    UNHANDLED_COUNTRY(NU)  // Niue
    UNHANDLED_COUNTRY(TK)  // Tokelau
    END_UNHANDLED_COUNTRIES(NZ)

    // Countries using the "Portugal" engine set.
    UNHANDLED_COUNTRY(CV)  // Cape Verde
    UNHANDLED_COUNTRY(GW)  // Guinea-Bissau
    UNHANDLED_COUNTRY(MZ)  // Mozambique
    UNHANDLED_COUNTRY(ST)  // Sao Tome and Principe
    UNHANDLED_COUNTRY(TL)  // Timor-Leste
    END_UNHANDLED_COUNTRIES(PT)

    // Countries using the "Russia" engine set.
    UNHANDLED_COUNTRY(AM)  // Armenia
    UNHANDLED_COUNTRY(AZ)  // Azerbaijan
    UNHANDLED_COUNTRY(KG)  // Kyrgyzstan
    UNHANDLED_COUNTRY(TJ)  // Tajikistan
    UNHANDLED_COUNTRY(TM)  // Turkmenistan
    UNHANDLED_COUNTRY(UZ)  // Uzbekistan
    END_UNHANDLED_COUNTRIES(RU)

    // Countries using the "Saudi Arabia" engine set.
    UNHANDLED_COUNTRY(MR)  // Mauritania
    UNHANDLED_COUNTRY(PS)  // Palestinian Territory
    UNHANDLED_COUNTRY(SD)  // Sudan
    END_UNHANDLED_COUNTRIES(SA)

    // Countries using the "United Kingdom" engine set.
    UNHANDLED_COUNTRY(BM)  // Bermuda
    UNHANDLED_COUNTRY(CQ)  // Sark
    UNHANDLED_COUNTRY(FK)  // Falkland Islands
    UNHANDLED_COUNTRY(GG)  // Guernsey
    UNHANDLED_COUNTRY(GI)  // Gibraltar
    UNHANDLED_COUNTRY(GS)  // South Georgia and the South Sandwich
                           //   Islands
    UNHANDLED_COUNTRY(IM)  // Isle of Man
    UNHANDLED_COUNTRY(IO)  // British Indian Ocean Territory
    UNHANDLED_COUNTRY(JE)  // Jersey
    UNHANDLED_COUNTRY(KY)  // Cayman Islands
    UNHANDLED_COUNTRY(MS)  // Montserrat
    UNHANDLED_COUNTRY(PN)  // Pitcairn Islands
    UNHANDLED_COUNTRY(SH)  // Saint Helena, Ascension Island, and Tristan da
                           //   Cunha
    UNHANDLED_COUNTRY(TC)  // Turks and Caicos Islands
    UNHANDLED_COUNTRY(VG)  // British Virgin Islands
    END_UNHANDLED_COUNTRIES(GB)

    // Countries using the "United States" engine set.
    UNHANDLED_COUNTRY(AS)  // American Samoa
    UNHANDLED_COUNTRY(GU)  // Guam
    UNHANDLED_COUNTRY(MP)  // Northern Mariana Islands
    UNHANDLED_COUNTRY(UM)  // U.S. Minor Outlying Islands
    UNHANDLED_COUNTRY(VI)  // U.S. Virgin Islands
    END_UNHANDLED_COUNTRIES(US)

    // Countries using the "default" engine set.
    UNHANDLED_COUNTRY(AF)  // Afghanistan
    UNHANDLED_COUNTRY(AG)  // Antigua and Barbuda
    UNHANDLED_COUNTRY(AI)  // Anguilla
    UNHANDLED_COUNTRY(AO)  // Angola
    UNHANDLED_COUNTRY(AQ)  // Antarctica
    UNHANDLED_COUNTRY(BB)  // Barbados
    UNHANDLED_COUNTRY(BD)  // Bangladesh
    UNHANDLED_COUNTRY(BS)  // Bahamas
    UNHANDLED_COUNTRY(BT)  // Bhutan
    UNHANDLED_COUNTRY(BW)  // Botswana
    UNHANDLED_COUNTRY(CU)  // Cuba
    UNHANDLED_COUNTRY(DM)  // Dominica
    UNHANDLED_COUNTRY(ER)  // Eritrea
    UNHANDLED_COUNTRY(ET)  // Ethiopia
    UNHANDLED_COUNTRY(FJ)  // Fiji
    UNHANDLED_COUNTRY(FM)  // Micronesia
    UNHANDLED_COUNTRY(GD)  // Grenada
    UNHANDLED_COUNTRY(GE)  // Georgia
    UNHANDLED_COUNTRY(GH)  // Ghana
    UNHANDLED_COUNTRY(GM)  // Gambia
    UNHANDLED_COUNTRY(GQ)  // Equatorial Guinea
    UNHANDLED_COUNTRY(GY)  // Guyana
    UNHANDLED_COUNTRY(KH)  // Cambodia
    UNHANDLED_COUNTRY(KI)  // Kiribati
    UNHANDLED_COUNTRY(KM)  // Comoros
    UNHANDLED_COUNTRY(KN)  // Saint Kitts and Nevis
    UNHANDLED_COUNTRY(KP)  // North Korea
    UNHANDLED_COUNTRY(LA)  // Laos
    UNHANDLED_COUNTRY(LC)  // Saint Lucia
    UNHANDLED_COUNTRY(LK)  // Sri Lanka
    UNHANDLED_COUNTRY(LR)  // Liberia
    UNHANDLED_COUNTRY(LS)  // Lesotho
    UNHANDLED_COUNTRY(MG)  // Madagascar
    UNHANDLED_COUNTRY(MH)  // Marshall Islands
    UNHANDLED_COUNTRY(MM)  // Myanmar
    UNHANDLED_COUNTRY(MN)  // Mongolia
    UNHANDLED_COUNTRY(MU)  // Mauritius
    UNHANDLED_COUNTRY(MV)  // Maldives
    UNHANDLED_COUNTRY(MW)  // Malawi
    UNHANDLED_COUNTRY(NA)  // Namibia
    UNHANDLED_COUNTRY(NG)  // Nigeria
    UNHANDLED_COUNTRY(NP)  // Nepal
    UNHANDLED_COUNTRY(NR)  // Nauru
    UNHANDLED_COUNTRY(PG)  // Papua New Guinea
    UNHANDLED_COUNTRY(PW)  // Palau
    UNHANDLED_COUNTRY(SB)  // Solomon Islands
    UNHANDLED_COUNTRY(SC)  // Seychelles
    UNHANDLED_COUNTRY(SL)  // Sierra Leone
    UNHANDLED_COUNTRY(SO)  // Somalia
    UNHANDLED_COUNTRY(SR)  // Suriname
    UNHANDLED_COUNTRY(SZ)  // Swaziland
    UNHANDLED_COUNTRY(TO)  // Tonga
    UNHANDLED_COUNTRY(TV)  // Tuvalu
    UNHANDLED_COUNTRY(UG)  // Uganda
    UNHANDLED_COUNTRY(VC)  // Saint Vincent and the Grenadines
    UNHANDLED_COUNTRY(VU)  // Vanuatu
    UNHANDLED_COUNTRY(WS)  // Samoa
    UNHANDLED_COUNTRY(ZM)  // Zambia
    case country_codes::CountryId().Serialize():
    default:  // Unhandled location
      END_UNHANDLED_COUNTRIES(default)
  }

  std::vector<EngineAndTier> t_url;
  for (size_t i = 0; i < num_engines; i++) {
    t_url.push_back(engines[i]);
  }
  return t_url;
}
