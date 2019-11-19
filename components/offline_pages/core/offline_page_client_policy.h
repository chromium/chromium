// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_PAGE_CLIENT_POLICY_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_PAGE_CLIENT_POLICY_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/time/time.h"

namespace offline_pages {

static const size_t kUnlimitedPages = 0;

// Enum whose values specify the lifetime characteristics of pages pertaining to
// a given client.
enum class LifetimeType {
  // A temporary offline page, which is automatically downloaded and cleaned
  // up when storage limits are reached or when caches are cleared. They are
  // normally stored in Chrome internal directories, not directly accessible to
  // the user.
  TEMPORARY,
  // A persistent offline page, manually downloaded by the user. It is stored in
  // the public Downloads directory and only the user can delete it.
  PERSISTENT,
};

// The struct describing policies for offline pages' clients (Bookmark, Last-N
// etc.) describing how their pages are handled by the offline page model.
struct OfflinePageClientPolicy {
  OfflinePageClientPolicy(std::string namespace_val,
                          LifetimeType lifetime_type_val);
  static OfflinePageClientPolicy CreateTemporary(
      const std::string& name_space,
      const base::TimeDelta& expiration_period);
  static OfflinePageClientPolicy CreatePersistent(
      const std::string& name_space);

  OfflinePageClientPolicy(const OfflinePageClientPolicy& other);
  ~OfflinePageClientPolicy();

  // Namespace that uniquely identifies this client.
  std::string name_space;

  // Lifetime type for the pages saved by this client.
  LifetimeType lifetime_type = LifetimeType::TEMPORARY;

  // The time after which pages expire. A zero value (default) means pages from
  // this client never expire.
  base::TimeDelta expiration_period;

  // The maximum number of pages allowed to be saved for this client.
  // |kUnlimitedPages| (default) means no limit is set.
  size_t page_limit = kUnlimitedPages;

  // The maximum number of pages for the same URL that can be stored for this
  // client. |kUnlimitedPages| (default) means no limit is set.
  size_t pages_allowed_per_url = kUnlimitedPages;

  // Whether pages are shown in the Downloads UI.
  bool is_supported_by_download = false;

  // Whether pages can only be viewed in a specific tab. Pages controlled by
  // this policy must have their ClientId::id field set to their assigned tab's
  // id.
  bool is_restricted_to_tab_from_client_id = false;

  // Whether this client should be "disabled" if any of these user settings are
  // set to:
  // * 3rd party cookies are blocked (prefs::kBlockThirdPartyCookies).
  // * Network predictions (prefs::kNetworkPredictionOptions) are fully
  //   disabled.
  bool requires_specific_user_settings = false;

  // Whether the pages originate from suggestion engines like Zine or the Feed.
  bool is_suggested = false;

  // Whether a background page download is allowed to be converted to a regular
  // download if the URL turns out to point to a file (i.e. a PDF).
  bool allows_conversion_to_background_file_download = false;

  // Whether background fetches are deferred while the active tab matches the
  // SavePageRequestURL.
  bool defer_background_fetch_while_page_is_active = false;
};

// Get the client policy for |name_space|.
const OfflinePageClientPolicy& GetPolicy(const std::string& name_space);
// Returns a list of all known namespaces.
const std::vector<std::string>& GetAllPolicyNamespaces();
// Returns a list of all temporary namespaces.
const std::vector<std::string>& GetTemporaryPolicyNamespaces();
// Returns a list of all persistent namespaces.
const std::vector<std::string>& GetPersistentPolicyNamespaces();

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_PAGE_CLIENT_POLICY_H_
