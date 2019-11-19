// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/page_criteria.h"

#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "components/offline_pages/core/offline_page_client_policy.h"
#include "components/offline_pages/core/offline_page_item.h"
#include "components/offline_pages/core/offline_page_item_utils.h"

namespace offline_pages {

PageCriteria::PageCriteria() = default;
PageCriteria::~PageCriteria() = default;
PageCriteria::PageCriteria(const PageCriteria&) = default;
PageCriteria::PageCriteria(PageCriteria&&) = default;

bool MeetsCriteria(const PageCriteria& criteria, const ClientId& client_id) {
  if (criteria.client_ids &&
      !base::Contains(criteria.client_ids.value(), client_id)) {
    return false;
  }
  if (criteria.client_namespaces &&
      !base::Contains(criteria.client_namespaces.value(),
                      client_id.name_space)) {
    return false;
  }
  if (!criteria.guid.empty() && client_id.id != criteria.guid)
    return false;
  // Only fetches the policy if it will be needed.
  if (criteria.exclude_tab_bound_pages || criteria.pages_for_tab_id ||
      criteria.supported_by_downloads || criteria.lifetime_type) {
    const OfflinePageClientPolicy& policy = GetPolicy(client_id.name_space);
    if (criteria.exclude_tab_bound_pages &&
        policy.is_restricted_to_tab_from_client_id) {
      return false;
    }
    if (criteria.pages_for_tab_id &&
        policy.is_restricted_to_tab_from_client_id) {
      std::string tab_id_str =
          base::NumberToString(criteria.pages_for_tab_id.value());
      if (client_id.id != tab_id_str)
        return false;
    }
    if (criteria.supported_by_downloads && !policy.is_supported_by_download) {
      return false;
    }
    if (criteria.lifetime_type &&
        criteria.lifetime_type.value() != policy.lifetime_type) {
      return false;
    }
  }

  return true;
}

bool MeetsCriteria(const PageCriteria& criteria, const OfflinePageItem& item) {
  if (!MeetsCriteria(criteria, item.client_id))
    return false;

  if (criteria.file_size && item.file_size != criteria.file_size.value())
    return false;
  if (!criteria.digest.empty() && item.digest != criteria.digest)
    return false;

  if (!criteria.request_origin.empty() &&
      item.request_origin != criteria.request_origin)
    return false;

  if (!criteria.url.is_empty() &&
      !EqualsIgnoringFragment(criteria.url, item.url) &&
      (item.original_url_if_different.is_empty() ||
       !EqualsIgnoringFragment(criteria.url, item.original_url_if_different))) {
    return false;
  }

  if (criteria.offline_ids &&
      !base::Contains(criteria.offline_ids.value(), item.offline_id)) {
    return false;
  }

  if (criteria.additional_criteria && !criteria.additional_criteria.Run(item))
    return false;

  return true;
}

std::vector<std::string> PotentiallyMatchingNamespaces(
    const PageCriteria& criteria) {
  std::vector<std::string> matching_namespaces;
  if (criteria.supported_by_downloads || criteria.lifetime_type) {
    std::vector<std::string> allowed_namespaces =
        criteria.client_namespaces ? criteria.client_namespaces.value()
                                   : GetAllPolicyNamespaces();
    std::vector<std::string> filtered;
    for (const std::string& name_space : allowed_namespaces) {
      const OfflinePageClientPolicy& policy = GetPolicy(name_space);
      if (criteria.supported_by_downloads && !policy.is_supported_by_download) {
        continue;
      }
      if (criteria.lifetime_type &&
          criteria.lifetime_type.value() != policy.lifetime_type) {
        continue;
      }
      matching_namespaces.push_back(name_space);
    }
  } else if (criteria.client_namespaces) {
    matching_namespaces = criteria.client_namespaces.value();
  }
  // no filter otherwise.

  return matching_namespaces;
}

}  // namespace offline_pages
