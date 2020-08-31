// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_COMMON_SAFEBROWSING_CONSTANTS_H_
#define COMPONENTS_SAFE_BROWSING_CORE_COMMON_SAFEBROWSING_CONSTANTS_H_

#include <string>

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

// error_code to use when Safe Browsing blocks a request.
extern const int kNetErrorCodeForSafeBrowsing;

// The name of the histogram that records whether Safe Browsing is enabled.
extern const char kSafeBrowsingEnabledHistogramName[];

// Countries that has no endpoint for Safe Browsing.
const std::vector<std::string> GetExcludedCountries();

// This enum must be kept in-sync with content::ResourceType. This is enforced
// by static_asserts in safebrowsing_constants_content.cc. content::ResourceType
// cannot be used on iOS because iOS cannot depend on content/.
enum class ResourceType {
  kMainFrame = 0,        // top level page
  kSubFrame = 1,         // frame or iframe
  kStylesheet = 2,       // a CSS stylesheet
  kScript = 3,           // an external script
  kImage = 4,            // an image (jpg/gif/png/etc)
  kFontResource = 5,     // a font
  kSubResource = 6,      // an "other" subresource.
  kObject = 7,           // an object (or embed) tag for a plugin.
  kMedia = 8,            // a media resource.
  kWorker = 9,           // the main resource of a dedicated worker.
  kSharedWorker = 10,    // the main resource of a shared worker.
  kPrefetch = 11,        // an explicitly requested prefetch
  kFavicon = 12,         // a favicon
  kXhr = 13,             // a XMLHttpRequest
  kPing = 14,            // a ping request for <a ping>/sendBeacon.
  kServiceWorker = 15,   // the main resource of a service worker.
  kCspReport = 16,       // a report of Content Security Policy violations.
  kPluginResource = 17,  // a resource that a plugin requested.
  // a main-frame service worker navigation preload request.
  kNavigationPreloadMainFrame = 19,
  // a sub-frame service worker navigation preload request.
  kNavigationPreloadSubFrame = 20,
  kMaxValue = kNavigationPreloadSubFrame,
};
}

#endif  // COMPONENTS_SAFE_BROWSING_CORE_COMMON_SAFEBROWSING_CONSTANTS_H_
