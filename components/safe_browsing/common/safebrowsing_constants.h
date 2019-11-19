// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_COMMON_SAFEBROWSING_CONSTANTS_H_
#define COMPONENTS_SAFE_BROWSING_COMMON_SAFEBROWSING_CONSTANTS_H_

#include "base/files/file_path.h"

namespace safe_browsing {

extern const base::FilePath::CharType kSafeBrowsingBaseFilename[];

// Filename suffix for the cookie database.
extern const base::FilePath::CharType kCookiesFile[];

// The URL for the Safe Browsing page.
extern const char kSafeBrowsingUrl[];

// When a network::mojom::URLLoader is cancelled because of SafeBrowsing, this
// custom cancellation reason could be used to notify the implementation side.
// Please see network::mojom::URLLoader::kClientDisconnectReason for more
// details.
extern const char kCustomCancelReasonForURLLoader[];

// Returns the error_code to use when Safe Browsing blocks a request.
int GetNetErrorCodeForSafeBrowsing();
}

#endif  // COMPONENTS_SAFE_BROWSING_COMMON_SAFEBROWSING_CONSTANTS_H_
