// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PAGE_CRITERIA_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PAGE_CRITERIA_H_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "components/offline_pages/core/client_id.h"
#include "components/offline_pages/core/offline_page_client_policy.h"
#include "url/gurl.h"

namespace offline_pages {
struct OfflinePageItem;

// Criteria for matching an offline page. The default |PageCriteria| matches
// all pages.
struct PageCriteria {
  PageCriteria();
  ~PageCriteria();
  PageCriteria(const PageCriteria&);
  PageCriteria(PageCriteria&&);

  enum Order {
    kDescendingCreationTime,
    kAscendingAccessTime,
    kDescendingAccessTime,
  };

  // If non-empty, the page must match this URL. The provided URL
  // is matched both against the original and the actual URL fields (they
  // sometimes differ because of possible redirects).
  GURL url;
  // Whether to exclude pages that may only be opened in a specific tab.
  bool exclude_tab_bound_pages = false;
  // If specified, accepts pages that can be displayed in the specified tab.
  // That is, tab-bound pages are filtered out unless the tab ID matches this
  // field and non-tab-bound pages are always included.
  std::optional<int> pages_for_tab_id;
  // Whether to restrict pages to those in namespaces supported by the
  // downloads UI.
  bool supported_by_downloads = false;
  // If set, the page's lifetime type must match this.
  std::optional<LifetimeType> lifetime_type;
  // If set, the page's file_size must match.
  std::optional<int64_t> file_size;
  // If non-empty, the page's digest must match.
  std::string digest;
  // If set, the page's namespace must match.
  std::optional<std::vector<std::string>> client_namespaces;
  // If set, the page's client_id must match one of these.
  std::optional<std::vector<ClientId>> client_ids;
  // If non-empty, the page's client_id.id must match this.
  std::string guid;
  // If non-empty, the page's request_origin must match.
  std::string request_origin;
  // If set, the page's offline_id must match.
  std::optional<std::vector<int64_t>> offline_ids;
  // If non-null, this function is executed for each matching item. If it
  // returns false, the item will not be returned. This is evaluated last, and
  // only for pages that otherwise meet all other criteria.
  base::RepeatingCallback<bool(const OfflinePageItem&)> additional_criteria;
  // If > 0, returns at most this many pages.
  size_t maximum_matches = 0;
  // The order results are returned. Affects which results are dropped with
  // |maximum_matches|.
  Order result_order = kDescendingCreationTime;
};

// Returns true if an offline page with |client_id| could potentially match
// |criteria|.
bool MeetsCriteria(const PageCriteria& criteria, const ClientId& client_id);

// Returns whether |item| matches |criteria|.
bool MeetsCriteria(const PageCriteria& criteria, const OfflinePageItem& item);

// Returns the list of offline page namespaces that could potentially match
// Criteria. Returns an empty list if any namespace could match.
std::vector<std::string> PotentiallyMatchingNamespaces(
    const PageCriteria& criteria);

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PAGE_CRITERIA_H_
