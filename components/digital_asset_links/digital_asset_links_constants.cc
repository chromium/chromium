// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/digital_asset_links/digital_asset_links_constants.h"

#include "net/base/net_errors.h"

namespace digital_asset_links {

const char kCustomCancelReasonForURLLoader[] = "DigitalAssetLinks";

const int kNetErrorCodeForDigitalAssetLinks = net::ERR_ACCESS_DENIED;

}  // namespace digital_asset_links
