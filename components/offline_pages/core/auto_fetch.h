// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_AUTO_FETCH_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_AUTO_FETCH_H_

#include <optional>

#include "components/offline_pages/core/client_id.h"

// Most auto-fetch code is in browser/offline_pages. This file contains code
// that needs to be accessed within components/offline_pages.

namespace offline_pages {
namespace auto_fetch {

// This metadata is stored in the |ClientId|'s |id| field.
struct ClientIdMetadata {
  ClientIdMetadata();
  ClientIdMetadata(const ClientIdMetadata&);
  ClientIdMetadata& operator=(const ClientIdMetadata&);
  explicit ClientIdMetadata(int android_tab_id)
      : android_tab_id(android_tab_id) {}
  // ID of the Android tab that initiated the request.
  int android_tab_id;
};

ClientId MakeClientId(const ClientIdMetadata& metadata);
// Extract metadata from a |ClientId| that was created with |MakeClientId|.
std::optional<ClientIdMetadata> ExtractMetadata(const ClientId& id);

}  // namespace auto_fetch
}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_AUTO_FETCH_H_
