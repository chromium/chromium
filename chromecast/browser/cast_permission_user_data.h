// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_PERMISSION_USER_DATA_H_
#define CHROMECAST_BROWSER_CAST_PERMISSION_USER_DATA_H_

#include <string>

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
  CastPermissionUserData(content::WebContents* web_contents,
                         const std::string& app_id,
                         const GURL& app_web_url);
  CastPermissionUserData(const CastPermissionUserData&) = delete;
  CastPermissionUserData& operator=(const CastPermissionUserData&) = delete;
  ~CastPermissionUserData() override;

  static CastPermissionUserData* FromWebContents(
      content::WebContents* web_contents);
  std::string GetAppId() { return app_id_; }
  GURL GetAppWebUrl() { return app_web_url_; }

 private:
  std::string app_id_;
  GURL app_web_url_;
};

}  // namespace shell
}  // namespace chromecast
#endif  // CHROMECAST_BROWSER_CAST_PERMISSION_USER_DATA_H_
