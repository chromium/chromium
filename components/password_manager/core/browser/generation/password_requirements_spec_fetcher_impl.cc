// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/generation/password_requirements_spec_fetcher_impl.h"

#include <string_view>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/hash/md5.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/proto/password_requirements.pb.h"
#include "components/autofill/core/browser/proto/password_requirements_shard.pb.h"
#include "components/password_manager/core/browser/generation/password_requirements_spec_printer.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/url_canon.h"

namespace autofill {

PasswordRequirementsSpecFetcherImpl::PasswordRequirementsSpecFetcherImpl(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    int version,
    size_t prefix_length,
    int timeout)
    : url_loader_factory_(std::move(url_loader_factory)),
      version_(version),
      prefix_length_(prefix_length),
      timeout_(timeout) {
  DCHECK_GE(version_, 0);
  DCHECK_LE(prefix_length_, 32u);
  DCHECK_GE(timeout_, 0);
}

PasswordRequirementsSpecFetcherImpl::~PasswordRequirementsSpecFetcherImpl() =
    default;

PasswordRequirementsSpecFetcherImpl::LookupInFlight::LookupInFlight() = default;
PasswordRequirementsSpecFetcherImpl::LookupInFlight::~LookupInFlight() =
    default;

namespace {

// Hashes the eTLD+1 of |origin| via MD5 and returns a filename with the first
// |prefix_length| bits populated. The returned value corresponds to the first
// 4 bytes of the truncated MD5 prefix in hex notation.
// For example:
//   "https://www.example.com" has a eTLD+1 of "example.com".
//   The MD5SUM of that is 5ababd603b22780302dd8d83498e5172.
//   Stripping this to the first 8 bits (prefix_length = 8) gives
//   500000000000000000000000000000000. The file name is always cut to the first
//   four bytes, i.e. 5000 in this example.
std::string GetHashPrefix(const GURL& origin, size_t prefix_length) {
  DCHECK_LE(prefix_length, 32u);
  std::string domain_and_registry =
      net::registry_controlled_domains::GetDomainAndRegistry(
          origin, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);

  base::MD5Digest digest;
  base::MD5Sum(base::as_byte_span(domain_and_registry), &digest);

  for (auto& byte : digest.a) {
    if (prefix_length >= 8) {
      prefix_length -= 8;
      continue;
    } else {
      // Determine the |prefix_length| most significant bits by calculating
      // the 8 - |prefix_length| least significant bits and inverting the
      // result.
      byte &= ~((1 << (8 - prefix_length)) - 1);
      prefix_length = 0;
    }
  }

  return base::MD5DigestToBase16(digest).substr(0, 4);
}

// Returns the URL on gstatic.com where the passwords spec file can be found
// that contains data for |hash_prefix|.
GURL GetUrlForRequirementsSpec(int version, const std::string& hash_prefix) {
  return GURL(base::StringPrintf(
      "https://www.gstatic.com/chrome/autofill/password_generation_specs/%d/%s",
      version, hash_prefix.c_str()));
}

}  // namespace

void PasswordRequirementsSpecFetcherImpl::Fetch(GURL origin,
                                                FetchCallback callback) {
  DCHECK(origin.is_valid());
  VLOG(1) << "Fetching password requirements spec for " << origin;

  if (!url_loader_factory_) {
    VLOG(1) << "No url_logger_factory_ available";
    TriggerCallback(std::move(callback), ResultCode::kErrorNoUrlLoader,
                    PasswordRequirementsSpec());
    return;
  }

  if (!origin.is_valid() || origin.HostIsIPAddress() ||
      !origin.SchemeIsHTTPOrHTTPS()) {
    VLOG(1) << "No valid origin";
    TriggerCallback(std::move(callback), ResultCode::kErrorInvalidOrigin,
                    PasswordRequirementsSpec());
    return;
  }

  // Canonicalize away trailing periods in hostname.
  while (!origin.host_piece().empty() && origin.host_piece().back() == '.') {
    std::string_view new_host =
        origin.host_piece().substr(0, origin.host_piece().length() - 1);
    GURL::Replacements replacements;
    replacements.SetHostStr(new_host);
    origin = origin.ReplaceComponents(replacements);
  }

  std::string hash_prefix = GetHashPrefix(origin, prefix_length_);

  // If a lookup is happening already, just register another callback.
  auto iter = lookups_in_flight_.find(hash_prefix);
  if (iter != lookups_in_flight_.end()) {
    iter->second->callbacks.emplace_back(origin, std::move(callback));
    VLOG(1) << "Lookup already in flight";
    return;
  }

  // Start another lookup otherwise.
  auto lookup = std::make_unique<LookupInFlight>();
  lookup->callbacks.emplace_back(origin, std::move(callback));
  lookup->start_of_request = base::TimeTicks::Now();

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("password_requirements_spec_fetch",
                                          R"(
      semantics {
        sender: "Password requirements specification fetcher"
        description:
          "Fetches the password requirements for a set of domains whose origin "
          "hash starts with a certain prefix."
        trigger:
          "When the user triggers a password generation (this can happen by "
          "just focussing a password field)."
        data:
          "The URL encodes a hash prefix from which it is not possible to "
          "derive the original origin. No user information is sent."
        destination: WEBSITE
      }
      policy {
        cookies_allowed: NO
        setting: "Unconditionally enabled."
        policy_exception_justification:
          "Not implemented, considered not useful."
      })");
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GetUrlForRequirementsSpec(version_, hash_prefix);
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  lookup->url_loader = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);
  lookup->url_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&PasswordRequirementsSpecFetcherImpl::OnFetchComplete,
                     base::Unretained(this), hash_prefix));

  lookup->download_timer.Start(
      FROM_HERE, base::Milliseconds(timeout_),
      base::BindOnce(&PasswordRequirementsSpecFetcherImpl::OnFetchTimeout,
                     base::Unretained(this), hash_prefix));

  lookups_in_flight_[hash_prefix] = std::move(lookup);
}

void PasswordRequirementsSpecFetcherImpl::OnFetchComplete(
    const std::string& hash_prefix,
    std::unique_ptr<std::string> response_body) {
  std::unique_ptr<LookupInFlight> lookup = RemoveLookupInFlight(hash_prefix);

  lookup->download_timer.Stop();
  UMA_HISTOGRAM_TIMES("PasswordManager.RequirementsSpecFetcher.NetworkDuration",
                      base::TimeTicks::Now() - lookup->start_of_request);
  // Network error codes are negative. See: src/net/base/net_error_list.h.
  base::UmaHistogramSparse(
      "PasswordManager.RequirementsSpecFetcher.NetErrorCode",
      -lookup->url_loader->NetError());
  if (lookup->url_loader->ResponseInfo() &&
      lookup->url_loader->ResponseInfo()->headers) {
    base::UmaHistogramSparse(
        "PasswordManager.RequirementsSpecFetcher.HttpResponseCode",
        lookup->url_loader->ResponseInfo()->headers->response_code());
  }

  if (!response_body || lookup->url_loader->NetError() != net::Error::OK) {
    VLOG(1) << "Fetch for " << hash_prefix << ": failed to fetch. Net Error: "
            << net::ErrorToString(lookup->url_loader->NetError());
    TriggerCallbackToAll(&lookup->callbacks, ResultCode::kErrorFailedToFetch,
                         PasswordRequirementsSpec());
    return;
  }

  PasswordRequirementsShard shard;
  if (!shard.ParseFromString(*response_body)) {
    VLOG(1) << "Fetch for " << hash_prefix << ": failed to parse response";
    TriggerCallbackToAll(&lookup->callbacks, ResultCode::kErrorFailedToParse,
                         PasswordRequirementsSpec());
    return;
  }
  for (auto& callback_pair : lookup->callbacks) {
    const GURL& origin = callback_pair.first;
    FetchCallback& callback_function = callback_pair.second;

    // Search shard for matches for origin by looking up the (canonicalized)
    // host name and then stripping domain prefixes until the eTLD+1 is reached.
    DCHECK(!origin.HostIsIPAddress());
    // |host| is a std::string instead of std::string_view as the protbuf::Map
    // implementation does not support StringPieces as parameters for find.
    std::string host = origin.host();
    auto host_iter = shard.specs().find(host);
    if (host_iter != shard.specs().end()) {
      const PasswordRequirementsSpec& spec = host_iter->second;
      VLOG(1) << "Found for " << host << ": " << spec;
      TriggerCallback(std::move(callback_function), ResultCode::kFoundSpec,
                      spec);
      continue;
    }

    bool found_entry = false;
    const std::string domain_and_registry =
        net::registry_controlled_domains::GetDomainAndRegistry(
            origin,
            net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
    while (host.length() > 0 && host != domain_and_registry) {
      size_t pos = host.find('.');
      if (pos != std::string::npos) {  // strip prefix
        host = host.substr(pos + 1);
      } else {
        break;
      }
      // If an entry has ben found, exit with that.
      auto it = shard.specs().find(host);
      if (it != shard.specs().end()) {
        const PasswordRequirementsSpec& spec = it->second;
        found_entry = true;
        VLOG(1) << "Found for " << host << ": " << spec;
        TriggerCallback(std::move(callback_function), ResultCode::kFoundSpec,
                        spec);
        break;
      }
    }

    if (!found_entry) {
      VLOG(1) << "Found no entry for " << host;
      // `found_entry` guards against moving out of `callback_function` twice.
      // NOLINTNEXTLINE(bugprone-use-after-move)
      TriggerCallback(std::move(callback_function), ResultCode::kFoundNoSpec,
                      PasswordRequirementsSpec());
    }
  }
}

void PasswordRequirementsSpecFetcherImpl::OnFetchTimeout(
    const std::string& hash_prefix) {
  std::unique_ptr<LookupInFlight> lookup = RemoveLookupInFlight(hash_prefix);
  UMA_HISTOGRAM_TIMES("PasswordManager.RequirementsSpecFetcher.NetworkDuration",
                      base::TimeTicks::Now() - lookup->start_of_request);
  TriggerCallbackToAll(&lookup->callbacks, ResultCode::kErrorTimeout,
                       PasswordRequirementsSpec());
}

void PasswordRequirementsSpecFetcherImpl::TriggerCallbackToAll(
    std::list<std::pair<GURL, FetchCallback>>* callbacks,
    ResultCode result,
    const PasswordRequirementsSpec& spec) {
  for (auto& callback_pair : *callbacks) {
    TriggerCallback(std::move(callback_pair.second), result, spec);
  }
}

void PasswordRequirementsSpecFetcherImpl::TriggerCallback(
    FetchCallback callback,
    ResultCode result,
    const PasswordRequirementsSpec& spec) {
  UMA_HISTOGRAM_ENUMERATION("PasswordManager.RequirementsSpecFetcher.Result",
                            result);
  std::move(callback).Run(spec);
}

std::unique_ptr<PasswordRequirementsSpecFetcherImpl::LookupInFlight>
PasswordRequirementsSpecFetcherImpl::RemoveLookupInFlight(
    const std::string& hash_prefix) {
  DCHECK(lookups_in_flight_.find(hash_prefix) != lookups_in_flight_.end());
  std::unique_ptr<LookupInFlight> lookup;
  std::swap(lookup, lookups_in_flight_[hash_prefix]);
  lookups_in_flight_.erase(hash_prefix);
  return lookup;
}

}  // namespace autofill
