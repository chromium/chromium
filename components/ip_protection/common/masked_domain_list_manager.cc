// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/masked_domain_list_manager.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/numerics/checked_math.h"
#include "base/time/time.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "components/ip_protection/common/ip_protection_data_types.h"
#include "components/ip_protection/common/ip_protection_telemetry.h"
#include "components/ip_protection/common/masked_domain_list.h"
#include "components/ip_protection/common/url_matcher_with_bypass.h"
#include "components/privacy_sandbox/masked_domain_list/masked_domain_list.pb.h"
#include "net/base/features.h"
#include "net/base/schemeful_site.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/proxy_config.mojom-shared.h"

namespace ip_protection {
namespace {
using ::masked_domain_list::Resource;
using ::masked_domain_list::ResourceOwner;
using ::network::mojom::IpProtectionProxyBypassPolicy;

bool UseFlatbuffer() {
  return base::FeatureList::IsEnabled(
      network::features::kMaskedDomainListFlatbufferImpl);
}

bool RestrictTopLevelSiteSchemes(
    const net::NetworkAnonymizationKey& network_anonymization_key,
    const std::optional<net::SchemefulSite>& top_frame_site) {
  // Only proxy traffic where the top-level site is an HTTP/HTTPS page or
  // where the NAK corresponds to a fenced frame.
  //
  // Note: It's possible that the top-level site could be a file: URL in
  // the case where an HTML file was downloaded and then opened. We
  // don't proxy in this case in favor of better compatibility. It's
  // also possible that the top-level site could be a blob URL, data
  // URL, or filesystem URL (the latter two with restrictions on how
  // they could have been navigated to), but we'll assume these aren't
  // used pervasively as the top-level site for pages that make the
  // types of requests that IP Protection will apply to.
  return net::features::kIpPrivacyRestrictTopLevelSiteSchemes.Get() &&
         !network_anonymization_key.GetNonce().has_value() &&
         !top_frame_site.value().GetURL().SchemeIsHTTPOrHTTPS();
}

}  // namespace

MaskedDomainListManager::MaskedDomainListManager(
    IpProtectionProxyBypassPolicy policy)
    : proxy_bypass_policy_{policy},
      creation_time_for_mdl_update_metric_(base::TimeTicks::Now()) {}

MaskedDomainListManager::~MaskedDomainListManager() = default;

MaskedDomainListManager::MaskedDomainListManager(
    const MaskedDomainListManager&) {}

bool MaskedDomainListManager::IsEnabled() const {
  return base::FeatureList::IsEnabled(network::features::kMaskedDomainList);
}

bool MaskedDomainListManager::IsPopulated() const {
  if (UseFlatbuffer()) {
    return default_mdl_ && regular_browsing_mdl_;
  }
  return url_matcher_with_bypass_.IsPopulated();
}

size_t MaskedDomainListManager::EstimateMemoryUsage() const {
  if (UseFlatbuffer()) {
    return 0;
  }
  return base::trace_event::EstimateMemoryUsage(url_matcher_with_bypass_);
}

bool MaskedDomainListManager::Matches(
    const GURL& request_url,
    const net::NetworkAnonymizationKey& network_anonymization_key,
    MdlType mdl_type) const {
  std::optional<net::SchemefulSite> top_frame_site =
      network_anonymization_key.GetTopFrameSite();

  if (UseFlatbuffer()) {
    MaskedDomainList* mdl = mdl_type == MdlType::kIncognito
                                ? default_mdl_.get()
                                : regular_browsing_mdl_.get();
    // If the MDL is not initialized yet, or initialization failed, nothing
    // matches. In this situation, `IsPopulated()` would have returned false,
    // so this serves as a backup check.
    if (!mdl) {
      return false;
    }
    std::string request_domain = request_url.host();
    if (request_domain.size() > 0 && request_domain.back() == '.') {
      request_domain = request_domain.substr(0, request_domain.length() - 1);
    }

    if (proxy_bypass_policy_ == IpProtectionProxyBypassPolicy::kNone) {
      // For the kNone policy, all owned resources are considered a match.
      return mdl->IsOwnedResource(request_domain);
    }

    std::string top_frame_domain;
    if (top_frame_site.has_value()) {
      const GURL& top_frame_url = top_frame_site->GetURL();
      top_frame_domain = top_frame_url.host();
      if (top_frame_domain.size() > 0 && top_frame_domain.back() == '.') {
        top_frame_domain =
            top_frame_domain.substr(0, top_frame_domain.length() - 1);
      }

      if (RestrictTopLevelSiteSchemes(network_anonymization_key,
                                      top_frame_site)) {
        return false;
      }
    } else {
      // If there is no top frame, that is never a match.
      return false;
    }

    return mdl->Matches(request_domain, top_frame_domain);
  }

  // Normalize `request_url` and `top_frame_site` URLs by removing any
  // trailing dot from their hosts if present.
  GURL sanitized_request_url;
  const GURL& request_url_ref =
      SanitizeURLIfNeeded(request_url, sanitized_request_url);

  if (top_frame_site.has_value()) {
    GURL sanitized_top_frame_url;
    const GURL& top_frame_url = top_frame_site->GetURL();
    const GURL& top_frame_url_ref =
        SanitizeURLIfNeeded(top_frame_url, sanitized_top_frame_url);
    if (top_frame_url_ref != top_frame_url) {
      top_frame_site = net::SchemefulSite(sanitized_top_frame_url);
    }
  }

  UrlMatcherWithBypassResult match_result;
  switch (proxy_bypass_policy_) {
    case IpProtectionProxyBypassPolicy::kNone:
    case IpProtectionProxyBypassPolicy::kExclusionList:
      match_result = url_matcher_with_bypass_.Matches(
          request_url_ref, top_frame_site, mdl_type,
          /*skip_bypass_check=*/true);
      break;
    case IpProtectionProxyBypassPolicy::kFirstPartyToTopLevelFrame:
      if (!top_frame_site.has_value()) {
        VLOG(3) << "MDLM::Matches(" << request_url_ref
                << ", empty top_frame_site) - false";
        return false;
      }
      VLOG(3) << "MDLM::Matches(" << request_url_ref << ", "
              << top_frame_site.value() << ")";

      // Bypass the proxy for same-site requests.
      net::SchemefulSite request_site(request_url_ref);
      if (top_frame_site.has_value() && top_frame_site == request_site) {
        return false;
      }

      if (RestrictTopLevelSiteSchemes(network_anonymization_key,
                                      top_frame_site)) {
        return false;
      }

      // If the NAK is transient (has a nonce and/or top_frame_origin is
      // opaque), we should skip the first party check and match only on the
      // request_url.
      match_result = url_matcher_with_bypass_.Matches(
          request_url_ref, top_frame_site, mdl_type,
          network_anonymization_key.IsTransient());
      break;
  }

  return match_result == UrlMatcherWithBypassResult::kMatchAndNoBypass;
}

void MaskedDomainListManager::UpdateMaskedDomainList(
    const masked_domain_list::MaskedDomainList& mdl,
    const std::vector<std::string>& exclusion_list) {
  // Browser should only call this method when flatbuffer is disabled.
  CHECK(!UseFlatbuffer());

  RecordCreationTime();

  // Clear the existing matchers.
  url_matcher_with_bypass_.Clear();

  // Only construct the exclusion set if the policy is kExclusionList.
  const std::unordered_set<std::string> exclusion_set =
      proxy_bypass_policy_ == IpProtectionProxyBypassPolicy::kExclusionList
          ? std::unordered_set<std::string>(exclusion_list.begin(),
                                            exclusion_list.end())
          : std::unordered_set<std::string>();

  for (const ResourceOwner& owner : mdl.resource_owners()) {
    // Only create a bypass matcher if the policy is
    // kFirstPartyToTopLevelFrame.
    url_matcher_with_bypass_.AddRules(
        owner, exclusion_set,
        /*create_bypass_matcher=*/proxy_bypass_policy_ ==
            IpProtectionProxyBypassPolicy::kFirstPartyToTopLevelFrame);
  }

  Telemetry().MdlEstimatedMemoryUsage(EstimateMemoryUsage());
}

void MaskedDomainListManager::UpdateMaskedDomainListFlatbuffer(
    base::File default_file,
    uint64_t default_file_size,
    base::File regular_browsing_file,
    uint64_t regular_browsing_file_size) {
  // Browser should only call this Mojo method when flatbuffer is enabled.
  CHECK(UseFlatbuffer());

  // Flatbuffer implementation is not compatible with the exclusion-list
  // policy.
  CHECK_NE(proxy_bypass_policy_, IpProtectionProxyBypassPolicy::kExclusionList);

  RecordCreationTime();

  default_mdl_ = std::make_unique<MaskedDomainList>(std::move(default_file),
                                                    default_file_size);
  regular_browsing_mdl_ = std::make_unique<MaskedDomainList>(
      std::move(regular_browsing_file), regular_browsing_file_size);

  // Note that MdlEstimatedMemoryUsage is not recorded in this branch, as
  // this data structure is not in-memory.
  return;
}

void MaskedDomainListManager::RecordCreationTime() {
  if (creation_time_for_mdl_update_metric_.has_value()) {
    Telemetry().MdlFirstUpdateTime(base::TimeTicks::Now() -
                                   *creation_time_for_mdl_update_metric_);
    creation_time_for_mdl_update_metric_.reset();
  }
}

const GURL& MaskedDomainListManager::SanitizeURLIfNeeded(
    const GURL& url,
    GURL& sanitized_url) const {
  if (!url.host().empty() && url.host().back() == '.') {
    std::string host = url.host();
    host = host.substr(0, host.length() - 1);
    GURL::Replacements replacements;
    replacements.SetHostStr(host);
    sanitized_url = url.ReplaceComponents(replacements);
    return sanitized_url;
  }
  return url;
}

}  // namespace ip_protection
