// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/common/safebrowsing_switches.h"

namespace safe_browsing {
namespace switches {

// List of comma-separated URLs to show the social engineering red interstitial
// for.
const char kMarkAsPhishing[] = "mark_as_phishing";

// List of comma-separated sha256 hashes of executable files which the
// download-protection service should treat as "dangerous."  For a file to
// show a warning, it also must be considered a dangerous filetype and not
// be allowlisted otherwise (by signature or URL) and must be on a supported
// OS. Hashes are in hex. This is used for manual testing when looking
// for ways to by-pass download protection.
const char kSbManualDownloadBlocklist[] =
    "safebrowsing-manual-download-blacklist";

// Enable Safe Browsing Enhanced Protection.
const char kSbEnableEnhancedProtection[] =
    "safebrowsing-enable-enhanced-protection";
}  // namespace switches
}  // namespace safe_browsing
