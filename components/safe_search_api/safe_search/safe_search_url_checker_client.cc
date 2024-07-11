// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_search_api/safe_search/safe_search_url_checker_client.h"

#include <optional>
#include <utility>

#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/escape.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/google/core/common/google_util.h"
#include "net/base/load_flags.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/url_constants.h"

namespace safe_search_api {

namespace {

const char kSafeSearchApiUrl[] =
    "https://safesearch.googleapis.com/v1:classify";
const char kDataContentType[] = "application/x-www-form-urlencoded";
const char kDataFormat[] = "key=%s&urls=%s";

// Builds the POST data for SafeSearch API requests.
std::string BuildRequestData(const std::string& api_key, const GURL& url) {
  std::string query = base::EscapeQueryParamValue(url.spec(), true);
  return base::StringPrintf(kDataFormat, api_key.c_str(), query.c_str());
}

// Parses a SafeSearch API |response| and stores the result in |is_porn|,
// returns true on success. Otherwise, returns false and doesn't set |is_porn|.
bool ParseResponse(const std::string& response, bool* is_porn) {
  std::optional<base::Value> optional_value = base::JSONReader::Read(response);
  if (!optional_value || !optional_value.value().is_dict()) {
    DLOG(WARNING) << "ParseResponse failed to parse global dictionary";
    return false;
  }
  const base::Value::Dict& dict = optional_value.value().GetDict();
  const base::Value::List* classifications_list =
      dict.FindList("classifications");
  if (!classifications_list) {
    DLOG(WARNING) << "ParseResponse failed to parse classifications list";
    return false;
  }
  if (classifications_list->size() != 1u) {
    DLOG(WARNING) << "ParseResponse expected exactly one result";
    return false;
  }
  const base::Value& classification_value = (*classifications_list)[0];
  if (!classification_value.is_dict()) {
    DLOG(WARNING) << "ParseResponse failed to parse classification dict";
    return false;
  }
  const base::Value::Dict& classification_dict = classification_value.GetDict();
  std::optional<bool> is_porn_opt = classification_dict.FindBool("pornography");
  if (is_porn_opt.has_value())
    *is_porn = is_porn_opt.value();
  return true;
}

}  // namespace

struct SafeSearchURLCheckerClient::Check {
  Check(const GURL& url,
        std::unique_ptr<network::SimpleURLLoader> simple_url_loader,
        ClientCheckCallback callback);
  ~Check();

  GURL url;
  ClientCheckCallback callback;
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader;
  base::TimeTicks start_time;
};

SafeSearchURLCheckerClient::Check::Check(
    const GURL& url,
    std::unique_ptr<network::SimpleURLLoader> simple_url_loader,
    ClientCheckCallback callback)
    : url(url),
      callback(std::move(callback)),
      simple_url_loader(std::move(simple_url_loader)),
      start_time(base::TimeTicks::Now()) {}

SafeSearchURLCheckerClient::Check::~Check() = default;

SafeSearchURLCheckerClient::SafeSearchURLCheckerClient(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    const std::string& api_key)
    : url_loader_factory_(std::move(url_loader_factory)),
      traffic_annotation_(traffic_annotation),
      api_key_(api_key) {}

SafeSearchURLCheckerClient::~SafeSearchURLCheckerClient() = default;

void SafeSearchURLCheckerClient::CheckURL(const GURL& url,
                                          ClientCheckCallback callback) {
  DVLOG(1) << "Checking URL " << url;
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(kSafeSearchApiUrl);
  resource_request->method = "POST";
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader =
      network::SimpleURLLoader::Create(std::move(resource_request),
                                       traffic_annotation_);
  simple_url_loader->AttachStringForUpload(BuildRequestData(api_key_, url),
                                           kDataContentType);
  checks_in_progress_.push_front(std::make_unique<Check>(
      url, std::move(simple_url_loader), std::move(callback)));
  auto it = checks_in_progress_.begin();
  network::SimpleURLLoader* loader = it->get()->simple_url_loader.get();
  loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&SafeSearchURLCheckerClient::OnSimpleLoaderComplete,
                     base::Unretained(this), it));
}

void SafeSearchURLCheckerClient::OnSimpleLoaderComplete(
    CheckList::iterator it,
    std::unique_ptr<std::string> response_body) {
  std::unique_ptr<Check> check = std::move(*it);

  checks_in_progress_.erase(it);

  if (!response_body) {
    DLOG(WARNING) << "URL request failed! Letting through...";
    std::move(check->callback).Run(check->url, ClientClassification::kUnknown);
    return;
  }

  ClientClassification classification = ClientClassification::kUnknown;
  bool is_porn = false;
  if (ParseResponse(*response_body, &is_porn)) {
    classification = is_porn ? ClientClassification::kRestricted
                             : ClientClassification::kAllowed;
  }

  base::UmaHistogramTimes("Enterprise.SafeSites.Latency",
                          base::TimeTicks::Now() - check->start_time);

  std::move(check->callback).Run(check->url, classification);
}

}  // namespace safe_search_api
