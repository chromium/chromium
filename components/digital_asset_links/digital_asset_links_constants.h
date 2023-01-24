// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DIGITAL_ASSET_LINKS_DIGITAL_ASSET_LINKS_CONSTANTS_H_
#define COMPONENTS_DIGITAL_ASSET_LINKS_DIGITAL_ASSET_LINKS_CONSTANTS_H_

namespace digital_asset_links {

// When a network::mojom::URLLoader is cancelled because of digital asset link
// verification, this custom cancellation reason could be used to notify the
// implementation side. Please see
// network::mojom::URLLoader::kClientDisconnectReason for more details.
extern const char kCustomCancelReasonForURLLoader[];

// error_code to use when Safe Browsing blocks a request.
extern const int kNetErrorCodeForDigitalAssetLinks;

}  // namespace digital_asset_links

#endif  // COMPONENTS_SAFE_BROWSING_CORE_COMMON_SAFEBROWSING_CONSTANTS_H_
