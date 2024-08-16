// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/webstore_override.h"

#include <array>
#include <string>
#include <string_view>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/no_destructor.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "extensions/common/context_data.h"
#include "extensions/common/extension.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

// The set of features which this delegated availability check should apply to,
// which correspond to the features used by the webstore.
constexpr static std::array<std::string_view, 2> kWebstoreOverrideFeatureList =
    {
        "webstorePrivate",
        "management",
};

// Returns true if the "apps-gallery-url" command line flag has been used to
// specify a webstore override URL and if that URL is the same origin with the
// `url` this function is called with.
//
// Normally the APIs used by the webstore are only exposed to the actual domain
// used by the store, but this delegated availability check allows them to be
// exposed on a specified override URL. This is used both for our own API test
// and externally by the webstore team.
//
// Called by `SimpleFeature::IsAvailableToContextImpl()`, once that function
// determines that the feature has a "delegated availability check" and runs
// the check that was installed by `CreateAvailabilityCheckMap()` when the
// Extensions client was initialized.
bool AreWebstoreFeaturesAvailable(const std::string& api_full_name,
                                  const extensions::Extension* extension,
                                  extensions::mojom::ContextType context,
                                  const GURL& url,
                                  extensions::Feature::Platform platform,
                                  int context_id,
                                  bool check_developer_mode,
                                  const extensions::ContextData& context_data) {
  // We use a static local variable here to save on recreating it for repeated
  // calls. This is safe since the command line flags are constant for the
  // duration of the process.
  static base::NoDestructor<GURL> override_url([]() {
    std::string override_url_str =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kAppsGalleryURL);

    // Empty string means the command line switch was not used.
    if (override_url_str.empty()) {
      return GURL();
    }

    return GURL(override_url_str);
  }());

  if (!override_url->is_valid()) {
    // No override URL was specified. Early out.
    return false;
  }

  if (extension) {
    return false;
  }
  if (context != extensions::mojom::ContextType::kWebPage) {
    return false;
  }
  if (!base::Contains(kWebstoreOverrideFeatureList, api_full_name)) {
    return false;
  }

  // We only consider the scheme, domain and port of the supplied override URL,
  // which must match the current URL.
  if (!url.SchemeIs(override_url->scheme())) {
    return false;
  }
  if (!url.DomainIs(override_url->host())) {
    return false;
  }
  // We only compare port if one was supplied in the override URL, otherwise we
  // ignore it completely.
  if (override_url->has_port() && url.port() != override_url->port()) {
    return false;
  }
  return true;
}

}  // namespace

namespace extensions::webstore_override {

Feature::FeatureDelegatedAvailabilityCheckMap CreateAvailabilityCheckMap() {
  Feature::FeatureDelegatedAvailabilityCheckMap map;
  for (const auto item : kWebstoreOverrideFeatureList) {
    map.emplace(item, base::BindRepeating(&AreWebstoreFeaturesAvailable));
  }
  return map;
}

}  // namespace extensions::webstore_override
