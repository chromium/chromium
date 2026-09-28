// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_WEB_APP_MANIFEST_SECTION_H_
#define COMPONENTS_PAYMENTS_CORE_WEB_APP_MANIFEST_SECTION_H_

#include <stdint.h>

#include <string>
#include <vector>

namespace payments {

// Represents a "related_applications" entry in a Web App Manifest, used to
// verify native Android payment apps via package name, minimum version, and
// SHA-256 signing certificate fingerprints.
struct WebAppManifestSection {
  WebAppManifestSection();
  explicit WebAppManifestSection(const WebAppManifestSection& param);
  ~WebAppManifestSection();

  // The package name of the app.
  std::string id;

  // Minimum version number of the app.
  int64_t min_version = 0;

  // The result of SHA256(signing certificate bytes) for each certificate in the
  // app.
  std::vector<std::vector<uint8_t>> fingerprints;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_WEB_APP_MANIFEST_SECTION_H_
