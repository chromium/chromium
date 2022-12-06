// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_AFFILIATION_ASSET_LINK_DATA_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_AFFILIATION_ASSET_LINK_DATA_H_

#include <string>
#include <vector>

#include "url/gurl.h"

namespace password_manager {

// The class parses an asset link file. The spec for the format is
// https://github.com/google/digitalassetlinks/blob/master/well-known/details.md
// The class cares only about two types of statements:
// - includes. Those are just a reference to a file to be loaded and parsed.
// - "get_login_creds" permission to a web page. That means that the target is
//   allowed to get the credentials saved for the source.
// Only HTTPS URLs are taken into account.
class AssetLinkData {
 public:
  AssetLinkData();
  AssetLinkData(AssetLinkData&& other) noexcept;

  AssetLinkData(const AssetLinkData&) = delete;
  AssetLinkData& operator=(const AssetLinkData&) = delete;

  ~AssetLinkData();

  AssetLinkData& operator=(AssetLinkData&& other);

  bool Parse(const std::string& data);

  const std::vector<GURL>& includes() const { return includes_; }
  const std::vector<GURL>& targets() const { return targets_; }

 private:
  std::vector<GURL> includes_;
  std::vector<GURL> targets_;
};

}  // namespace password_manager
#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_AFFILIATION_ASSET_LINK_DATA_H_
