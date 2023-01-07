// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_provider_logos/switches.h"

namespace search_provider_logos {
namespace switches {

// Overrides the URL used to fetch the current Google Doodle.
// Example: https://www.google.com/async/ddljson
// Testing? Try:
//   https://www.gstatic.com/chrome/ntp/doodle_test/ddljson_android0.json
//   https://www.gstatic.com/chrome/ntp/doodle_test/ddljson_android1.json
//   https://www.gstatic.com/chrome/ntp/doodle_test/ddljson_android2.json
//   https://www.gstatic.com/chrome/ntp/doodle_test/ddljson_android3.json
//   https://www.gstatic.com/chrome/ntp/doodle_test/ddljson_android4.json
const char kGoogleDoodleUrl[] = "google-doodle-url";

// Use a static URL for the logo of the default search engine.
// Example: https://www.google.com/branding/logo.png
const char kSearchProviderLogoURL[] = "search-provider-logo-url";

// Overrides the Doodle URL to use for third-party search engines.
// Testing? Try:
//   https://www.gstatic.com/chrome/ntp/doodle_test/third_party_simple.json
//   https://www.gstatic.com/chrome/ntp/doodle_test/third_party_animated.json
const char kThirdPartyDoodleURL[] = "third-party-doodle-url";

}  // namespace switches
}  // namespace search_provider_logos
