// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <string>

#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "third_party/metrics_proto/omnibox_input_type.pb.h"
#include "url/url_constants.h"

TestSchemeClassifier::TestSchemeClassifier() = default;

TestSchemeClassifier::~TestSchemeClassifier() = default;

metrics::OmniboxInputType TestSchemeClassifier::GetInputTypeForScheme(
    const std::string& scheme) const {
  DCHECK_EQ(scheme, base::ToLowerASCII(scheme));

#if BUILDFLAG(IS_IOS)
  // On iOS, treat the file: scheme like a query because it is not supported
  // for navigations.
  if (scheme == url::kFileScheme)
    return metrics::OmniboxInputType::QUERY;
#endif  // BUILDFLAG(IS_IOS)

  // This doesn't check the preference but check some chrome-ish schemes.
  const char* kKnownURLSchemes[] = {
      url::kHttpScheme, url::kHttpsScheme, url::kWsScheme,
      url::kWssScheme,  url::kFileScheme,  url::kAboutScheme,
      url::kFtpScheme,  url::kBlobScheme,  url::kFileSystemScheme,
      "view-source",    "javascript",      "chrome",
      "chrome-ui",      "devtools",
  };
  for (const char* known_scheme : kKnownURLSchemes) {
    if (scheme == known_scheme)
      return metrics::OmniboxInputType::URL;
  }

  return metrics::OmniboxInputType::EMPTY;
}
