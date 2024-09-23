// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/auto_fetch.h"

#include <string_view>

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "components/offline_pages/core/client_namespace_constants.h"

namespace offline_pages {
namespace auto_fetch {
ClientIdMetadata::ClientIdMetadata() = default;
ClientIdMetadata::ClientIdMetadata(const ClientIdMetadata&) = default;
ClientIdMetadata& ClientIdMetadata::operator=(const ClientIdMetadata&) =
    default;

ClientId MakeClientId(const ClientIdMetadata& metadata) {
  // Here, the 'A' prefix is used so that future versions can easily change the
  // format if necessary.
  return ClientId(
      kAutoAsyncNamespace,
      base::StrCat({"A", base::NumberToString(metadata.android_tab_id)}));
}

std::optional<ClientIdMetadata> ExtractMetadata(const ClientId& id) {
  if (id.name_space != kAutoAsyncNamespace) {
    return std::nullopt;
  }
  if (id.id.empty() || id.id[0] != 'A') {
    return std::nullopt;
  }
  ClientIdMetadata metadata;
  if (!base::StringToInt(std::string_view(id.id).substr(1),
                         &metadata.android_tab_id)) {
    return std::nullopt;
  }
  return metadata;
}

}  // namespace auto_fetch
}  // namespace offline_pages
