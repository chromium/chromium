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

#include "base/check.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "components/ip_protection/common/ip_protection_data_types.h"
#include "components/ip_protection/common/ip_protection_telemetry.h"
#include "components/ip_protection/common/masked_domain_list.h"
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
  return default_mdl_ && regular_browsing_mdl_;
}

bool MaskedDomainListManager::Matches(
    const GURL& request_url,
    const net::NetworkAnonymizationKey& network_anonymization_key,
    MdlType mdl_type) const {
  std::optional<net::SchemefulSite> top_frame_site =
      network_anonymization_key.GetTopFrameSite();

  MaskedDomainList* mdl = mdl_type == MdlType::kIncognito
                              ? default_mdl_.get()
                              : regular_browsing_mdl_.get();
  // If the MDL is not initialized yet, or initialization failed, nothing
  // matches. In this situation, `IsPopulated()` would have returned false,
  // so this serves as a backup check.
  if (!mdl) {
    return false;
  }
  std::string request_domain = request_url.GetHost();
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
    top_frame_domain = top_frame_url.GetHost();
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

void MaskedDomainListManager::UpdateMaskedDomainList(
    base::File default_file,
    uint64_t default_file_size,
    base::File regular_browsing_file,
    uint64_t regular_browsing_file_size) {
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

void MaskedDomainListManager::UpdateMaskedDomainListForTesting(
    const masked_domain_list::MaskedDomainList& mdl) {
  // If the MDL is empty, don't try to create Flatbuffers for it.
  if (mdl.ByteSizeLong() == 0) {
    default_mdl_ = nullptr;
    regular_browsing_mdl_ = nullptr;
    return;
  }

  base::FilePath default_mdl_file_path;
  base::CreateTemporaryFile(&default_mdl_file_path);
  base::FilePath regular_browsing_mdl_file_path;
  base::CreateTemporaryFile(&regular_browsing_mdl_file_path);
  CHECK(ip_protection::MaskedDomainList::BuildFromProto(
      mdl, default_mdl_file_path, regular_browsing_mdl_file_path));

  base::File default_mdl_file(default_mdl_file_path,
                              base::File::Flags::FLAG_OPEN |
                                  base::File::Flags::FLAG_READ |
                                  base::File::Flags::FLAG_DELETE_ON_CLOSE);
  base::File regular_browsing_mdl_file(
      regular_browsing_mdl_file_path,
      base::File::Flags::FLAG_OPEN | base::File::Flags::FLAG_READ |
          base::File::Flags::FLAG_DELETE_ON_CLOSE);

  if (!default_mdl_file.IsValid() || !regular_browsing_mdl_file.IsValid()) {
    DLOG(ERROR) << "Could not open the MDL flatbuffer files";
    return;
  }

  uint64_t default_file_size = default_mdl_file.GetLength();
  uint64_t regular_browsing_file_size = regular_browsing_mdl_file.GetLength();
  UpdateMaskedDomainList(std::move(default_mdl_file), default_file_size,
                         std::move(regular_browsing_mdl_file),
                         regular_browsing_file_size);
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
  if (!url.GetHost().empty() && url.GetHost().back() == '.') {
    std::string host = url.GetHost();
    host = host.substr(0, host.length() - 1);
    GURL::Replacements replacements;
    replacements.SetHostStr(host);
    sanitized_url = url.ReplaceComponents(replacements);
    return sanitized_url;
  }
  return url;
}

}  // namespace ip_protection
