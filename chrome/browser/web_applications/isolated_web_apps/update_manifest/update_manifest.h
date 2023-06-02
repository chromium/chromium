// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_UPDATE_MANIFEST_UPDATE_MANIFEST_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_UPDATE_MANIFEST_UPDATE_MANIFEST_H_

#include <vector>

#include "base/strings/string_piece.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "base/version.h"
#include "url/gurl.h"

namespace web_app {

constexpr base::StringPiece kUpdateManifestAllVersionsKey = "versions";
constexpr base::StringPiece kUpdateManifestVersionKey = "version";
constexpr base::StringPiece kUpdateManifestSrcKey = "src";

// An Isolated Web App Update Manifest contains a list of versions and download
// URLs of an Isolated Web App. The format is described in more detail here:
// https://github.com/WICG/isolated-web-apps/blob/main/Updates.md#web-application-update-manifest
class UpdateManifest {
 public:
  enum class JsonFormatError {
    kRootNotADictionary,
    kVersionsNotAnArray,
    kVersionEntryNotADictionary,
    kNoApplicableVersion,
  };

  // Attempts to convert the provided JSON data into an instance of
  // `UpdateManifest`.
  //
  // Note that at least one version entry is required; otherwise the Update
  // Manifest is treated as invalid.
  //
  // `update_manifest_url` is used to resolve relative `src` URLs in `versions`.
  static base::expected<UpdateManifest, JsonFormatError> CreateFromJson(
      const base::Value& json,
      const GURL& update_manifest_url);

  UpdateManifest(const UpdateManifest& other);
  UpdateManifest& operator=(const UpdateManifest& other);

  ~UpdateManifest();

  class VersionEntry {
   public:
    VersionEntry(GURL src, base::Version version);
    ~VersionEntry() = default;

    GURL src() const;
    base::Version version() const;

   private:
    friend bool operator==(const VersionEntry& a, const VersionEntry& b);

    GURL src_;
    base::Version version_;
  };

  // This is guaranteed to always contain at least one element.
  const std::vector<VersionEntry>& versions() const { return version_entries_; }

 private:
  explicit UpdateManifest(std::vector<VersionEntry> version_entries);

  std::vector<VersionEntry> version_entries_;
};

// Returns the most up to date version contained in the `UpdateManifest`.
UpdateManifest::VersionEntry GetLatestVersionEntry(
    const UpdateManifest& update_manifest);

bool operator==(const UpdateManifest::VersionEntry& lhs,
                const UpdateManifest::VersionEntry& rhs);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_UPDATE_MANIFEST_UPDATE_MANIFEST_H_
