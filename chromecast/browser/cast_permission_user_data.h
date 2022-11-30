// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_PERMISSION_USER_DATA_H_
#define CHROMECAST_BROWSER_CAST_PERMISSION_USER_DATA_H_

#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/supports_user_data.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

// TODO(b/191718807) Add App's page permission into this class.

namespace chromecast {
namespace shell {

class CastPermissionUserData : public base::SupportsUserData::Data {
 public:
  // Lifetime of the object is managed by |web_contents|.
  CastPermissionUserData(
      content::WebContents* web_contents,
      const std::string& app_id,
      const GURL& app_web_url,
      bool enforce_feature_permissions,
      std::vector<int32_t> feature_permissions,
      std::vector<std::string> additional_feature_permission_origins);
  CastPermissionUserData(const CastPermissionUserData&) = delete;
  CastPermissionUserData& operator=(const CastPermissionUserData&) = delete;
  ~CastPermissionUserData() override;

  static CastPermissionUserData* FromWebContents(
      content::WebContents* web_contents);
  std::string GetAppId() { return app_id_; }
  GURL GetAppWebUrl() { return app_web_url_; }
  bool GetEnforceFeaturePermissions() { return enforce_feature_permissions_; }
  const base::flat_set<int32_t>& GetFeaturePermissions() const {
    return feature_permissions_;
  }
  const std::vector<std::string>& GetAdditionalFeaturePermissionOrigins()
      const {
    return additional_feature_permission_origins_;
  }

 private:
  const std::string app_id_;
  const GURL app_web_url_;
  const bool enforce_feature_permissions_;
  base::flat_set<int32_t> feature_permissions_;
  const std::vector<std::string> additional_feature_permission_origins_;
};

}  // namespace shell
}  // namespace chromecast
#endif  // CHROMECAST_BROWSER_CAST_PERMISSION_USER_DATA_H_
