// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/os_registration.h"

#include "base/strings/string_piece.h"
#include "components/attribution_reporting/os_support.mojom.h"
#include "net/http/structured_headers.h"
#include "url/gurl.h"

namespace attribution_reporting {

GURL ParseOsSourceOrTriggerHeader(base::StringPiece header) {
  const auto item = net::structured_headers::ParseItem(header);
  if (!item || !item->item.is_string())
    return GURL();

  return GURL(item->item.GetString());
}

base::StringPiece GetSupportHeader(mojom::OsSupport os_support) {
  switch (os_support) {
    case mojom::OsSupport::kDisabled:
      return "web";
    case mojom::OsSupport::kEnabled:
      return "web, os";
  }
}

}  // namespace attribution_reporting
