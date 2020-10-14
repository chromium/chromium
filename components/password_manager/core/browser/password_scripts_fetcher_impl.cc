// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_scripts_fetcher_impl.h"

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/json/json_reader.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/version.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

constexpr int kCacheTimeoutInMinutes = 5;
constexpr int kFetchTimeoutInSeconds = 3;

constexpr int kMaxDownloadSizeInBytes = 10 * 1024;

using ParsingResult =
    password_manager::PasswordScriptsFetcherImpl::ParsingResult;

// Extracts the domains and min Chrome's version for which password changes are
// supported and adds them to |supported_domains|.
// |script_config| is the dictionary passed for a domain, representig the
// configuration data of one password change script. For example, the fetched
// JSON might look like this:
// {
//   'example.com': {
//     'domains': [ 'https://www.example.com', 'https://m.example.com' ],
//     'min_version': '86'
//   }
// }
// In this case |script_config| would represent the dictionary value
// of 'example.com'.
// Returns a set of warnings.
// The function tries to be lax about errors and prefers to skip them
// with warnings rather than bail the parsing. This is for forward
// compatibility.
base::flat_set<ParsingResult> ParseDomainSpecificParamaters(
    const base::Value& script_config,
    base::flat_map<url::Origin, base::Version>& supported_domains) {
  if (!script_config.is_dict())
    return {ParsingResult::kInvalidJson};

  const base::Value* supported_domains_list =
      script_config.FindListKey("domains");
  if (!supported_domains_list || !supported_domains_list->is_list())
    return {ParsingResult::kInvalidJson};

  base::flat_set<ParsingResult> warnings;

  const std::string* min_version = script_config.FindStringKey("min_version");
  base::Version version;
  if (!min_version) {
    warnings.insert(ParsingResult::kInvalidJson);
    // TODO(crbug.com/1132942): remove this when server side change is in place
    // and return error.
    version = base::Version("0");
  } else {
    version = base::Version(*min_version);
    if (!version.IsValid()) {
      return {ParsingResult::kInvalidJson};
    }
  }

  for (const base::Value& domain : supported_domains_list->GetList()) {
    if (!domain.is_string()) {
      warnings.insert(ParsingResult::kInvalidJson);
      continue;
    }

    GURL url(domain.GetString());
    if (!url.is_valid()) {
      warnings.insert(ParsingResult::kInvalidUrl);
      continue;
    }
    supported_domains.insert(std::make_pair(url::Origin::Create(url), version));
  }

  return warnings;
}

}  // namespace

namespace password_manager {

constexpr char kDefaultChangePasswordScriptsListUrl[] =
    "https://www.gstatic.com/chrome/duplex/change_password_scripts.json";

constexpr base::FeatureParam<std::string> kScriptsListUrlParam{
    &features::kPasswordScriptsFetching, "custom_list_url",
    kDefaultChangePasswordScriptsListUrl};

PasswordScriptsFetcherImpl::PasswordScriptsFetcherImpl(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : PasswordScriptsFetcherImpl(std::move(url_loader_factory),
                                 kScriptsListUrlParam.Get()) {}
PasswordScriptsFetcherImpl::PasswordScriptsFetcherImpl(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::string scripts_list_url)
    : scripts_list_url_(std::move(scripts_list_url)),
      url_loader_factory_(std::move(url_loader_factory)) {}

PasswordScriptsFetcherImpl::~PasswordScriptsFetcherImpl() = default;

void PasswordScriptsFetcherImpl::PrewarmCache() {
  if (IsCacheStale())
    StartFetch();
}

void PasswordScriptsFetcherImpl::RefreshScriptsIfNecessary(
    base::OnceClosure fetch_finished_callback) {
  CacheState state = IsCacheStale()
                         ? (url_loader_ ? CacheState::kWaiting
                                        : (last_fetch_timestamp_.is_null()
                                               ? CacheState::kNeverSet
                                               : CacheState::kStale))
                         : CacheState::kReady;
  base::UmaHistogramEnumeration(
      "PasswordManager.PasswordScriptsFetcher.CacheState", state);

  if (state == CacheState::kReady)
    std::move(fetch_finished_callback).Run();
  else
    fetch_finished_callbacks_.emplace_back(std::move(fetch_finished_callback));

  switch (state) {
    case CacheState::kReady:
    case CacheState::kWaiting:
      // No new fetching.
      break;
    case CacheState::kNeverSet:
    case CacheState::kStale:
      StartFetch();
      break;
  }
}

void PasswordScriptsFetcherImpl::FetchScriptAvailability(
    const url::Origin& origin,
    const base::Version& version,
    ResponseCallback callback) {
  if (IsCacheStale()) {
    pending_callbacks_.emplace_back(
        std::make_pair(std::make_pair(origin, version), std::move(callback)));
    StartFetch();
    return;
  }

  RunResponseCallback(origin, version, std::move(callback));
}

bool PasswordScriptsFetcherImpl::IsScriptAvailable(
    const url::Origin& origin,
    const base::Version& version) const {
  const auto& it = password_change_domains_.find(origin);
  if (it == password_change_domains_.end()) {
    return false;
  }
  return version >= it->second;
}

void PasswordScriptsFetcherImpl::StartFetch() {
  static const base::NoDestructor<base::TimeDelta> kFetchTimeout(
      base::TimeDelta::FromSeconds(kFetchTimeoutInSeconds));
  if (url_loader_)
    return;
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(scripts_list_url_);
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("gstatic_change_password_scripts",
                                          R"(
        semantics {
          sender: "Password Manager"
          description:
            "A JSON file hosted by gstatic containing a map of password change"
            "scripts to optional parameters for those scripts."
          trigger:
            "When the user visits chrome://settings/passwords/check or "
            "makes Safety Check in settings or sees a leak warning."
          data:
            "The request body is empty. No user data is included."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "The user can enable or disable automatic password leak checks in "
            "Chrome's security settings. The feature is enabled by default."
        })");
  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 traffic_annotation);
  url_loader_->SetTimeoutDuration(*kFetchTimeout);
  url_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&PasswordScriptsFetcherImpl::OnFetchComplete,
                     base::Unretained(this), base::TimeTicks::Now()),
      kMaxDownloadSizeInBytes);
}

void PasswordScriptsFetcherImpl::OnFetchComplete(
    base::TimeTicks request_start_timestamp,
    std::unique_ptr<std::string> response_body) {
  base::UmaHistogramTimes("PasswordManager.PasswordScriptsFetcher.ResponseTime",
                          base::TimeTicks::Now() - request_start_timestamp);
  bool report_http_response_code =
      (url_loader_->NetError() == net::OK ||
       url_loader_->NetError() == net::ERR_HTTP_RESPONSE_CODE_FAILURE) &&
      url_loader_->ResponseInfo() && url_loader_->ResponseInfo()->headers;
  base::UmaHistogramSparse(
      "PasswordManager.PasswordScriptsFetcher.HttpResponseAndNetErrorCode",
      report_http_response_code
          ? url_loader_->ResponseInfo()->headers->response_code()
          : url_loader_->NetError());
  url_loader_.reset();
  last_fetch_timestamp_ = base::TimeTicks::Now();

  base::flat_set<ParsingResult> parsing_warnings =
      ParseResponse(std::move(response_body));
  if (parsing_warnings.empty())
    parsing_warnings.insert(ParsingResult::kOk);
  for (ParsingResult warning : parsing_warnings) {
    base::UmaHistogramEnumeration(
        "PasswordManager.PasswordScriptsFetcher.ParsingResult", warning);
  }

  for (auto& callback : std::exchange(fetch_finished_callbacks_, {}))
    std::move(callback).Run();
  for (auto& callback : std::exchange(pending_callbacks_, {}))
    RunResponseCallback(std::move(callback.first.first),
                        std::move(callback.first.second),
                        std::move(callback.second));
}

base::flat_set<ParsingResult> PasswordScriptsFetcherImpl::ParseResponse(
    std::unique_ptr<std::string> response_body) {
  password_change_domains_.clear();

  if (!response_body)
    return {ParsingResult::kNoResponse};

  base::JSONReader::ValueWithError data =
      base::JSONReader::ReadAndReturnValueWithError(*response_body);

  if (data.value == base::nullopt) {
    DVLOG(1) << "Parse error: " << data.error_message;
    return {ParsingResult::kInvalidJson};
  }
  if (!data.value->is_dict())
    return {ParsingResult::kInvalidJson};

  base::flat_set<ParsingResult> warnings;
  for (const auto& script_it : data.value->DictItems()) {
    // |script_it.first| is an identifier (normally, a domain name, e.g.
    // example.com) that we don't care about.
    // |script_it.second| provides domain-specific parameters.
    base::flat_set<ParsingResult> warnings_for_script =
        ParseDomainSpecificParamaters(script_it.second,
                                      password_change_domains_);
    warnings.insert(warnings_for_script.begin(), warnings_for_script.end());
  }
  return warnings;
}

bool PasswordScriptsFetcherImpl::IsCacheStale() const {
  static const base::NoDestructor<base::TimeDelta> kCacheTimeout(
      base::TimeDelta::FromMinutes(kCacheTimeoutInMinutes));
  return last_fetch_timestamp_.is_null() ||
         base::TimeTicks::Now() - last_fetch_timestamp_ >= *kCacheTimeout;
}

void PasswordScriptsFetcherImpl::RunResponseCallback(
    url::Origin origin,
    base::Version version,
    ResponseCallback callback) {
  DCHECK(!url_loader_);     // Fetching is not running.
  DCHECK(!IsCacheStale());  // Cache is ready.
  bool has_script = IsScriptAvailable(origin, version);
  std::move(callback).Run(has_script);
}

}  // namespace password_manager
