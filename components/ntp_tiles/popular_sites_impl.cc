// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_tiles/popular_sites_impl.h"

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "components/google/core/common/google_util.h"
#include "components/ntp_tiles/features.h"
#include "components/ntp_tiles/pref_names.h"
#include "components/ntp_tiles/switches.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/search_engine_type.h"
#include "components/search_engines/template_url_service.h"
#include "components/variations/service/variations_service.h"
#include "components/variations/variations_associated_data.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
#include "base/json/json_reader.h"
#include "components/grit/components_resources.h"
#include "ui/base/resource/resource_bundle.h"
#endif

#if BUILDFLAG(IS_IOS)
#include "components/ntp_tiles/country_code_ios.h"
#endif

using variations::VariationsService;

namespace ntp_tiles {

namespace {

const char kPopularSitesURLFormat[] =
    "https://www.gstatic.com/%ssuggested_sites_%s_%s.json";
const char kPopularSitesDefaultDirectory[] = "chrome/ntp/";
const char kPopularSitesDefaultCountryCode[] = "DEFAULT";
const char kPopularSitesDefaultVersion[] = "5";
const int kSitesExplorationStartVersion = 6;
const int kPopularSitesRedownloadIntervalHours = 24;
#if BUILDFLAG(IS_IOS)
const char kIOSDefaultPopularSitesLocaleUS[] =
    "https://www.gstatic.com/chrome/ntp/ios/"
    "suggested_sites_US_2023q1_mvt_experiment_with_popular_sites.json";
#endif

GURL GetPopularSitesURL(const std::string& directory,
                        const std::string& country,
                        const std::string& version) {
  // US-based Chrome iOS users have a curated set of default Most Visited Tiles,
  // based on Chrome iOS history in the US.
#if BUILDFLAG(IS_IOS)
  if (country == "US") {
    return GURL(kIOSDefaultPopularSitesLocaleUS);
  }
#endif

  return GURL(base::StringPrintf(kPopularSitesURLFormat, directory.c_str(),
                                 country.c_str(), version.c_str()));
}

// Extract the country from the default search engine if the default search
// engine is Google.
std::string GetDefaultSearchEngineCountryCode(
    const TemplateURLService* template_url_service) {
  DCHECK(template_url_service);

  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (!cmd_line->HasSwitch(switches::kEnableNTPSearchEngineCountryDetection)) {
    return std::string();
  }

  const TemplateURL* default_provider =
      template_url_service->GetDefaultSearchProvider();
  // It's possible to not have a default provider in the case that the default
  // search engine is defined by policy.
  if (default_provider) {
    bool is_google_search_engine =
        default_provider->GetEngineType(
            template_url_service->search_terms_data()) ==
        SearchEngineType::SEARCH_ENGINE_GOOGLE;

    if (is_google_search_engine) {
      GURL search_url = default_provider->GenerateSearchURL(
          template_url_service->search_terms_data());
      return google_util::GetGoogleCountryCode(search_url);
    }
  }

  return std::string();
}

std::string GetVariationCountry() {
  return base::GetFieldTrialParamValue(kPopularSitesFieldTrialName, "country");
}

std::string GetVariationVersion() {
  return base::GetFieldTrialParamValue(kPopularSitesFieldTrialName, "version");
}

std::string GetVariationDirectory() {
  return base::GetFieldTrialParamValue(kPopularSitesFieldTrialName,
                                       "directory");
}

PopularSites::SitesVector ParseSiteList(const base::Value::List& list) {
  PopularSites::SitesVector sites;
  for (const base::Value& item_value : list) {
    if (!item_value.is_dict()) {
      continue;
    }
    const base::Value::Dict& item = item_value.GetDict();
    std::u16string title;
    if (const std::string* ptr = item.FindString("title")) {
      title = base::UTF8ToUTF16(*ptr);
    } else {
      continue;
    }
    std::string url;
    if (const std::string* ptr = item.FindString("url")) {
      url = *ptr;
    } else {
      continue;
    }
    std::string favicon_url;
    if (const std::string* ptr = item.FindString("favicon_url")) {
      favicon_url = *ptr;
    }
    std::string large_icon_url;
    if (const std::string* ptr = item.FindString("large_icon_url")) {
      large_icon_url = *ptr;
    }

    TileTitleSource title_source = TileTitleSource::UNKNOWN;
    std::optional<int> title_source_int = item.FindInt("title_source");
    if (!title_source_int) {
      // Only v6 and later have "title_source". Earlier versions use title tags.
      title_source = TileTitleSource::TITLE_TAG;
    } else if (*title_source_int <= static_cast<int>(TileTitleSource::LAST) &&
               *title_source_int >= 0) {
      title_source = static_cast<TileTitleSource>(*title_source_int);
    }

    sites.emplace_back(title, GURL(url), GURL(favicon_url),
                       GURL(large_icon_url), title_source);
    std::optional<int> default_icon_resource =
        item.FindInt("default_icon_resource");
    if (default_icon_resource) {
      sites.back().default_icon_resource = *default_icon_resource;
    }
    std::optional<bool> baked_in = item.FindBool("baked_in");
    if (baked_in.has_value()) {
      sites.back().baked_in = baked_in.value();
    }
  }
  return sites;
}

std::map<SectionType, PopularSites::SitesVector> ParseVersion5(
    const base::Value::List& list) {
  return {{SectionType::PERSONALIZED, ParseSiteList(list)}};
}

std::map<SectionType, PopularSites::SitesVector> ParseVersion6OrAbove(
    const base::Value::List& list) {
  // Valid lists would have contained at least the PERSONALIZED section.
  std::map<SectionType, PopularSites::SitesVector> sections = {
      std::make_pair(SectionType::PERSONALIZED, PopularSites::SitesVector{})};
  for (size_t i = 0; i < list.size(); i++) {
    const base::Value::Dict* item_dict = list[i].GetIfDict();
    if (!item_dict) {
      LOG(WARNING) << "Parsed SitesExploration list contained an invalid "
                   << "section at position " << i << ".";
      continue;
    }
    int section = item_dict->FindInt("section").value_or(-1);
    if (section < 0 || section > static_cast<int>(SectionType::LAST)) {
      LOG(WARNING) << "Parsed SitesExploration list contained a section with "
                   << "invalid ID (" << section << ")";
      continue;
    }
    // Non-personalized site exploration tiles are no longer supported, so
    // ignore all other section types.
    SectionType section_type = static_cast<SectionType>(section);
    if (section_type != SectionType::PERSONALIZED) {
      continue;
    }
    const base::Value::List* sites_list = item_dict->FindList("sites");
    if (!sites_list) {
      continue;
    }
    sections[section_type] = ParseSiteList(*sites_list);
  }
  return sections;
}

std::map<SectionType, PopularSites::SitesVector> ParseSites(
    const base::Value::List& list,
    int version) {
  if (version >= kSitesExplorationStartVersion) {
    return ParseVersion6OrAbove(list);
  }
  return ParseVersion5(list);
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && \
    (BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS))
void SetDefaultResourceForSite(size_t index,
                               int resource_id,
                               base::Value::List& sites) {
  if (index >= sites.size() || !sites[index].is_dict()) {
    return;
  }

  sites[index].GetDict().Set("default_icon_resource", resource_id);
}
#endif

// Creates the list of popular sites based on a snapshot available for mobile.
base::Value::List DefaultPopularSites(std::optional<std::string> country) {
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  return base::Value::List();
#else
  if (!base::FeatureList::IsEnabled(kPopularSitesBakedInContentFeature)) {
    return base::Value::List();
  }

  int popular_sites_json = IDR_DEFAULT_POPULAR_SITES_JSON;

#if BUILDFLAG(IS_IOS)
  if (country.has_value() && *country == "US") {
    popular_sites_json = IDR_DEFAULT_POPULAR_SITES_WITH_POPULAR_APPS_JSON;
  }
#endif

  std::optional<base::Value> sites = base::JSONReader::Read(
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          popular_sites_json));
  base::Value::List& sites_list = sites->GetList();
  for (base::Value& site : sites_list) {
    site.GetDict().Set("baked_in", true);
  }

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  static constexpr int default_popular_sites_icons[] = {
      IDR_DEFAULT_POPULAR_SITES_ICON0, IDR_DEFAULT_POPULAR_SITES_ICON1,
      IDR_DEFAULT_POPULAR_SITES_ICON2, IDR_DEFAULT_POPULAR_SITES_ICON3,
      IDR_DEFAULT_POPULAR_SITES_ICON4, IDR_DEFAULT_POPULAR_SITES_ICON5,
      IDR_DEFAULT_POPULAR_SITES_ICON6, IDR_DEFAULT_POPULAR_SITES_ICON7};
  base::span<const int> icon_list = default_popular_sites_icons;

#if BUILDFLAG(IS_IOS)
  // US-based Chrome iOS users have a curated set of default Most Visited Tiles,
  // based on Chrome iOS history in the US.
  if (country.has_value() && *country == "US") {
    static constexpr int popular_sites_icons_with_popular_ios_apps[] = {
        IDR_DEFAULT_POPULAR_SITES_WITH_POPULAR_APPS_ICON0,
        IDR_DEFAULT_POPULAR_SITES_WITH_POPULAR_APPS_ICON1,
        IDR_DEFAULT_POPULAR_SITES_WITH_POPULAR_APPS_ICON2,
        IDR_DEFAULT_POPULAR_SITES_WITH_POPULAR_APPS_ICON3,
        IDR_DEFAULT_POPULAR_SITES_WITH_POPULAR_APPS_ICON4,
        IDR_DEFAULT_POPULAR_SITES_WITH_POPULAR_APPS_ICON5,
        IDR_DEFAULT_POPULAR_SITES_WITH_POPULAR_APPS_ICON6,
        IDR_DEFAULT_POPULAR_SITES_WITH_POPULAR_APPS_ICON7};

    icon_list = popular_sites_icons_with_popular_ios_apps;
  }
#endif

  size_t index = 0;
  for (int icon_resource : icon_list) {
    SetDefaultResourceForSite(index++, icon_resource, sites_list);
  }
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return std::move(sites_list);
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
}

}  // namespace

PopularSites::Site::Site(const std::u16string& title,
                         const GURL& url,
                         const GURL& favicon_url,
                         const GURL& large_icon_url,
                         TileTitleSource title_source)
    : title(title),
      url(url),
      favicon_url(favicon_url),
      large_icon_url(large_icon_url),
      title_source(title_source),
      baked_in(false),
      default_icon_resource(-1) {}

PopularSites::Site::Site(const Site& other) = default;

PopularSites::Site::~Site() = default;

PopularSitesImpl::PopularSitesImpl(
    PrefService* prefs,
    const TemplateURLService* template_url_service,
    VariationsService* variations_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : prefs_(prefs),
      template_url_service_(template_url_service),
      variations_(variations_service),
      url_loader_factory_(std::move(url_loader_factory)),
      is_fallback_(false),
      sections_(
          ParseSites(prefs->GetList(prefs::kPopularSitesJsonPref),
                     prefs_->GetInteger(prefs::kPopularSitesVersionPref))) {}

PopularSitesImpl::~PopularSitesImpl() = default;

bool PopularSitesImpl::MaybeStartFetch(bool force_download,
                                       FinishedCallback callback) {
  DCHECK(!callback_);
  callback_ = std::move(callback);

  const base::Time last_download_time = base::Time::FromInternalValue(
      prefs_->GetInt64(prefs::kPopularSitesLastDownloadPref));
  const base::TimeDelta time_since_last_download =
      base::Time::Now() - last_download_time;
  const base::TimeDelta redownload_interval =
      base::Hours(kPopularSitesRedownloadIntervalHours);
  const bool download_time_is_future = base::Time::Now() < last_download_time;

  pending_url_ = GetURLToFetch();
  const bool url_changed =
      pending_url_.spec() != prefs_->GetString(prefs::kPopularSitesURLPref);

  // Download forced, or we need to download a new file.
  if (force_download || download_time_is_future ||
      (time_since_last_download > redownload_interval) || url_changed) {
    FetchPopularSites();
    return true;
  }
  return false;
}

const std::map<SectionType, PopularSitesImpl::SitesVector>&
PopularSitesImpl::sections() const {
  return sections_;
}

GURL PopularSitesImpl::GetURLToFetch() {
  const std::string directory = GetDirectoryToFetch();
  const std::string country = GetCountryToFetch();
  const std::string version = GetVersionToFetch();

  if (!base::StringToInt(version, &version_in_pending_url_)) {
    // Parses the leading digits as version. Defaults to 0 if that failed.
    if (version_in_pending_url_ <= 0) {
      bool success = base::StringToInt(kPopularSitesDefaultVersion,
                                       &version_in_pending_url_);
      DLOG(WARNING) << "The set version \"" << version << "\" does not start "
                    << "with a valid version number. Default version was used "
                    << "instead (" << kPopularSitesDefaultVersion << ").";
      DCHECK(success);
    }
  }

  const GURL override_url =
      GURL(prefs_->GetString(prefs::kPopularSitesOverrideURL));
  return override_url.is_valid()
             ? override_url
             : GetPopularSitesURL(directory, country, version);
}

std::string PopularSitesImpl::GetDirectoryToFetch() {
  std::string directory =
      prefs_->GetString(prefs::kPopularSitesOverrideDirectory);

  if (directory.empty()) {
    directory = GetVariationDirectory();
  }

  if (directory.empty()) {
    directory = kPopularSitesDefaultDirectory;
  }

  return directory;
}

// Determine the country code to use. In order of precedence:
// - The explicit "override country" pref set by the user.
// - The country code from the field trial config (variation parameter).
// - The Google country code if Google is the default search engine (and the
//   "--enable-ntp-search-engine-country-detection" switch is present).
// - The country provided by the VariationsService.
// - A default fallback.
std::string PopularSitesImpl::GetCountryToFetch() {
  std::string country_code =
      prefs_->GetString(prefs::kPopularSitesOverrideCountry);

  if (country_code.empty()) {
    country_code = GetVariationCountry();
  }

  if (country_code.empty()) {
    country_code = GetDefaultSearchEngineCountryCode(template_url_service_);
  }

  if (country_code.empty() && variations_) {
    country_code = variations_->GetStoredPermanentCountry();
  }

#if BUILDFLAG(IS_IOS)
  if (country_code.empty()) {
    country_code = GetDeviceCountryCode();
  }
#endif

  if (country_code.empty()) {
    country_code = kPopularSitesDefaultCountryCode;
  }

  return base::ToUpperASCII(country_code);
}

// Determine the version to use. In order of precedence:
// - The explicit "override version" pref set by the user.
// - The version from the field trial config (variation parameter).
// - A default fallback.
std::string PopularSitesImpl::GetVersionToFetch() {
  std::string version = prefs_->GetString(prefs::kPopularSitesOverrideVersion);

  if (version.empty()) {
    version = GetVariationVersion();
  }

  if (version.empty()) {
    version = kPopularSitesDefaultVersion;
  }

  return version;
}

const base::Value::List& PopularSitesImpl::GetCachedJson() {
  return prefs_->GetList(prefs::kPopularSitesJsonPref);
}

// static
void PopularSitesImpl::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* user_prefs) {
  user_prefs->RegisterStringPref(prefs::kPopularSitesOverrideURL,
                                 std::string());
  user_prefs->RegisterStringPref(prefs::kPopularSitesOverrideDirectory,
                                 std::string());
  user_prefs->RegisterStringPref(prefs::kPopularSitesOverrideCountry,
                                 std::string());
  user_prefs->RegisterStringPref(prefs::kPopularSitesOverrideVersion,
                                 std::string());

  user_prefs->RegisterInt64Pref(prefs::kPopularSitesLastDownloadPref, 0);
  user_prefs->RegisterStringPref(prefs::kPopularSitesURLPref, std::string());

#if BUILDFLAG(IS_IOS)
  // Estimate the country code; `GetVariationCountry()` cannot be called because
  // `RegisterProfilePrefs()` is static.
  std::string country_code_estimate = GetVariationCountry();

  if (country_code_estimate.empty()) {
    country_code_estimate = GetDeviceCountryCode();
  }

  std::optional<std::string> country(country_code_estimate);

  user_prefs->RegisterListPref(prefs::kPopularSitesJsonPref,
                               DefaultPopularSites(country));
#else
  user_prefs->RegisterListPref(prefs::kPopularSitesJsonPref,
                               DefaultPopularSites(std::nullopt));
#endif
  int version;
  base::StringToInt(kPopularSitesDefaultVersion, &version);
  user_prefs->RegisterIntegerPref(prefs::kPopularSitesVersionPref, version);
}

void PopularSitesImpl::FetchPopularSites() {
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("popular_sites_fetch", R"(
        semantics {
          sender: "Popular Sites New Tab Fetch"
          description:
            "Google Chrome may display a list of regionally-popular web sites "
            "on the New Tab Page. This service fetches the list of these sites."
          trigger:
            "Once per day, unless no popular web sites are required because "
            "the New Tab Page is filled with suggestions based on the user's "
            "browsing history."
          data: "A two letter country code based on the user's location."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting: "This feature cannot be disabled in settings."
          policy_exception_justification:
            "Not implemented, considered not useful."
        })");
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = pending_url_;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  simple_url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);
  simple_url_loader_->SetRetryOptions(
      1, network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);
  simple_url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&PopularSitesImpl::OnSimpleLoaderComplete,
                     base::Unretained(this)));
}

void PopularSitesImpl::OnSimpleLoaderComplete(
    std::unique_ptr<std::string> response_body) {
  simple_url_loader_.reset();

  if (!response_body) {
    OnDownloadFailed();
    return;
  }

  data_decoder::DataDecoder::ParseJsonIsolated(
      *response_body, base::BindOnce(&PopularSitesImpl::OnJsonParsed,
                                     weak_ptr_factory_.GetWeakPtr()));
}

void PopularSitesImpl::OnJsonParsed(
    data_decoder::DataDecoder::ValueOrError result) {
  ASSIGN_OR_RETURN(base::Value list, std::move(result), [&](std::string error) {
    DLOG(WARNING) << "JSON parsing failed: " << std::move(error);
    OnDownloadFailed();
  });

  if (!list.is_list()) {
    DLOG(WARNING) << "JSON is not a list";
    OnDownloadFailed();
    return;
  }
  sections_ = ParseSites(list.GetList(), version_in_pending_url_);
  prefs_->SetList(prefs::kPopularSitesJsonPref, std::move(list).TakeList());
  prefs_->SetInt64(prefs::kPopularSitesLastDownloadPref,
                   base::Time::Now().ToInternalValue());
  prefs_->SetInteger(prefs::kPopularSitesVersionPref, version_in_pending_url_);
  prefs_->SetString(prefs::kPopularSitesURLPref, pending_url_.spec());

  std::move(callback_).Run(true);
}

void PopularSitesImpl::OnDownloadFailed() {
  if (!is_fallback_) {
    DLOG(WARNING) << "Download country site list failed";
    is_fallback_ = true;
    pending_url_ = GetPopularSitesURL(kPopularSitesDefaultDirectory,
                                      kPopularSitesDefaultCountryCode,
                                      kPopularSitesDefaultVersion);
    FetchPopularSites();
  } else {
    DLOG(WARNING) << "Download fallback site list failed";
    std::move(callback_).Run(false);
  }
}

}  // namespace ntp_tiles
