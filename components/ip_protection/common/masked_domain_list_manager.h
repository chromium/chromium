// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IP_PROTECTION_COMMON_MASKED_DOMAIN_LIST_MANAGER_H_
#define COMPONENTS_IP_PROTECTION_COMMON_MASKED_DOMAIN_LIST_MANAGER_H_

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <set>

#include "base/files/file.h"
#include "base/time/time.h"
#include "components/ip_protection/common/ip_protection_data_types.h"
#include "components/privacy_sandbox/masked_domain_list/masked_domain_list.pb.h"
#include "net/base/network_anonymization_key.h"
#include "services/network/public/mojom/proxy_config.mojom-shared.h"
#include "url/gurl.h"

namespace ip_protection {

class MaskedDomainList;

// Class MaskedDomainListManager is a pseudo-singleton owned by the
// NetworkService. It uses the MaskedDomainList to generate the
// CustomProxyConfigPtr needed for NetworkContexts that are using the Privacy
// Proxy and determines if pairs of request and top_frame URLs are eligible.
class MaskedDomainListManager {
 public:
  explicit MaskedDomainListManager(
      network::mojom::IpProtectionProxyBypassPolicy);
  ~MaskedDomainListManager();
  MaskedDomainListManager(const MaskedDomainListManager&);

  // Estimates dynamic memory usage.
  // See base/trace_event/memory_usage_estimator.h for more info.
  size_t EstimateMemoryUsage() const;

  // Returns true if the allow list is eligible to be used but does not indicate
  // that the allow list is currently populated.
  bool IsEnabled() const;

  // Returns true if there are entries in the allow list and it is possible to
  // match on them. If false, `Matches` will always return false.
  bool IsPopulated() const;

  // Determines if the request is eligible for the proxy by determining if the
  // request_url is for an eligible domain and if the NAK supports eligibility.
  // If the top_frame_origin of the NAK does not have the same owner as the
  // request_url and the request_url is in the allow list, the request is
  // eligible for the proxy.
  // TODO(crbug.com/354649091): Public Suffix List domains and subdomains
  // proxy 1st party requests because no same-origin check is performed.
  bool Matches(const GURL& request_url,
               const net::NetworkAnonymizationKey& network_anonymization_key,
               MdlType mdl_type) const;

  void UpdateMaskedDomainList(base::File default_file,
                              uint64_t default_file_size,
                              base::File regular_browsing_file,
                              uint64_t regular_browsing_file_size);

  // Builds Flatbuffer files for testing.
  void UpdateMaskedDomainListForTesting(
      const masked_domain_list::MaskedDomainList& mdl);

 private:
  void RecordCreationTime();

  // Sanitizes the given URL by removing a trailing dot from its host if
  // present. Returns a reference to either the modified sanitized URL or the
  // original URL if no changes were made.
  const GURL& SanitizeURLIfNeeded(const GURL& url, GURL& sanitized_url) const;

  // The MDLs, for each MdlType.
  std::unique_ptr<MaskedDomainList> default_mdl_;
  std::unique_ptr<MaskedDomainList> regular_browsing_mdl_;

  // Policy that determines which domains are bypassed from IP Protection.
  network::mojom::IpProtectionProxyBypassPolicy proxy_bypass_policy_;

  // If UpdateMaskedDomainList has not yet been called, stores the time at which
  // the manager was created. The first call to `RecordCreationTime` clears
  // this to nullopt on entry.
  std::optional<base::TimeTicks> creation_time_for_mdl_update_metric_;
};

}  // namespace ip_protection

#endif  // COMPONENTS_IP_PROTECTION_COMMON_MASKED_DOMAIN_LIST_MANAGER_H_
