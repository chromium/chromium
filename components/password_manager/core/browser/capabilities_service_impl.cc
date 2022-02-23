// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/capabilities_service_impl.h"

#include "base/containers/flat_set.h"
#include "base/hash/legacy_hash.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "net/http/http_status_code.h"
#include "url/gurl.h"

namespace {

// Number of leading bits of the domain url hashes to send to the server.
constexpr uint32_t kHashPrefixSize = 15;

constexpr char kRequestIntent[] = "PASSWORD_CHANGE";
// Parameter that specifies the script's experiments.
const char kExperimentsParameterName[] = "EXPERIMENT_IDS";
// Server side experiment id that specifies when a script has only been released
// to a small subset of clients.
const char kScriptLiveExperiment[] = "3345172";

bool ScriptInLiveExperiment(
    const base::flat_map<std::string, std::string>& script_parameters) {
  auto params_iter = script_parameters.find(kExperimentsParameterName);
  if (params_iter == script_parameters.end()) {
    return false;
  }

  const std::vector<std::string> experiments = base::SplitString(
      params_iter->second, ",", base::WhitespaceHandling::TRIM_WHITESPACE,
      base::SplitResult::SPLIT_WANT_NONEMPTY);

  return base::ranges::count(experiments, kScriptLiveExperiment) > 0;
}

std::string CanonicalizeOrigin(const url::Origin& origin) {
  std::string url_str = origin.GetURL().spec();
  // Remove trailing '/' if exist.
  base::TrimString(url_str, "/", &url_str);
  return url_str;
}

uint64_t GetHashPrefix(const url::Origin& origin) {
  const std::string& canonicalized_url = CanonicalizeOrigin(origin);
  uint64_t hash = base::legacy::CityHash64(
      base::as_bytes(base::make_span(canonicalized_url)));
  return hash >> (64 - kHashPrefixSize);
}
}  // namespace

CapabilitiesServiceImpl::CapabilitiesServiceImpl(
    std::unique_ptr<autofill_assistant::AutofillAssistant> autofill_assistant)
    : autofill_assistant_(std::move(autofill_assistant)) {}

CapabilitiesServiceImpl::~CapabilitiesServiceImpl() = default;

void CapabilitiesServiceImpl::QueryPasswordChangeScriptAvailability(
    const std::vector<url::Origin>& origins,
    ResponseCallback callback) {
  if (origins.empty()) {
    std::move(callback).Run(std::set<url::Origin>());
    return;
  }

  std::vector<uint64_t> hash_prefixes;
  base::ranges::transform(origins, std::back_inserter(hash_prefixes),
                          GetHashPrefix);

  autofill_assistant_->GetCapabilitiesByHashPrefix(
      kHashPrefixSize, hash_prefixes, kRequestIntent,
      base::BindOnce(&CapabilitiesServiceImpl::OnGetCapabilitiesResult,
                     base::Unretained(this), origins, std::move(callback)));
}

void CapabilitiesServiceImpl::OnGetCapabilitiesResult(
    const std::vector<url::Origin>& origins,
    ResponseCallback callback,
    int http_status,
    const std::vector<CapabilitiesInfo>& infos) {
  base::UmaHistogramSparse(
      "PasswordManager.CapabilitiesService.HttpResponseCode", http_status);
  if (http_status != net::HTTP_OK) {
    std::move(callback).Run(std::set<url::Origin>());
    return;
  }

  std::set<url::Origin> infos_origin_set;
  for (const CapabilitiesInfo& info : infos) {
    // Checks if the script is visible to the client.
    if (password_manager::features::kPasswordChangeLiveExperimentParam.Get() ||
        !ScriptInLiveExperiment(info.script_parameters)) {
      infos_origin_set.insert(url::Origin::Create(GURL(info.url)));
    }
  }
  std::set<url::Origin> origins_set(origins.begin(), origins.end());
  std::set<url::Origin> response =
      base::STLSetIntersection<std::set<url::Origin>>(origins_set,
                                                      infos_origin_set);
  std::move(callback).Run(response);
}
