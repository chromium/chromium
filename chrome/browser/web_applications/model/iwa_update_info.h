// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_MODEL_IWA_UPDATE_INFO_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_MODEL_IWA_UPDATE_INFO_H_

#include <optional>

#include "components/webapps/isolated_web_apps/types/update_channel.h"
#include "url/gurl.h"

namespace web_app {

// Represents an explicit update configuration override for an Isolated Web App.
// When passed during installation, these fields are persisted into the app's
// IsolationData, bypassing defaults and manifest configuration.
struct IwaUpdateInfo {
  IwaUpdateInfo(GURL update_manifest_url,
                std::optional<UpdateChannel> update_channel);
  ~IwaUpdateInfo();
  IwaUpdateInfo(const IwaUpdateInfo&);
  IwaUpdateInfo& operator=(const IwaUpdateInfo&);
  IwaUpdateInfo(IwaUpdateInfo&&);
  IwaUpdateInfo& operator=(IwaUpdateInfo&&);

  GURL update_manifest_url;
  std::optional<UpdateChannel> update_channel;

  friend bool operator==(const IwaUpdateInfo&, const IwaUpdateInfo&) = default;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_MODEL_IWA_UPDATE_INFO_H_
